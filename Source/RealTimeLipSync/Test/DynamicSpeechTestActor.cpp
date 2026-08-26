// Copyright Epic Games, Inc. All Rights Reserved.

#include "DynamicSpeechTestActor.h"

#include "Async/Async.h"
#include "Audio.h"
#include "Components/AudioComponent.h"
#include "Features/IModularFeatures.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "ILiveLinkClient.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "MiddlewareAuthClient.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RhubarbLiveLinkSource.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/StrongObjectPtr.h"
#include "VisemeToArKitMapping.h"

ADynamicSpeechTestActor::ADynamicSpeechTestActor()
{
	PrimaryActorTick.bCanEverTick = true;

	AudioPlayback = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioPlayback"));
	RootComponent = AudioPlayback;
	AudioPlayback->bAutoActivate = false;
}

void ADynamicSpeechTestActor::BeginPlay()
{
	Super::BeginPlay();

	if (!IModularFeatures::Get().IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		UE_LOG(LogTemp, Error, TEXT("DynamicSpeechTestActor: LiveLink client modular feature not available"));
		return;
	}

	ILiveLinkClient& Client = IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
	LiveLinkSource = MakeShared<FRhubarbLiveLinkSource>(LiveLinkSubjectName);
	Client.AddSource(LiveLinkSource);
	LiveLinkSource->DeclareSubject(VisemeToArKitMapping::GetUsedCurveNames());
	CurrentCurveValues.Init(0.f, VisemeToArKitMapping::GetUsedCurveNames().Num());
}

void ADynamicSpeechTestActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (LiveLinkSource.IsValid())
	{
		if (IModularFeatures::Get().IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
		{
			IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName).RemoveSource(LiveLinkSource);
		}
		LiveLinkSource.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void ADynamicSpeechTestActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!LiveLinkSource.IsValid() || MouthCues.Num() == 0)
	{
		return;
	}

	ElapsedPlaybackTime += DeltaTime;

	// Temps utilisé pour chercher le cue, décalé par rapport au temps de lecture audio réel
	// (voir LipSyncDelaySeconds). Avant l'instant 0 ou après la dernière cue -> "X" (idle/neutre).
	const float VisemeSampleTime = ElapsedPlaybackTime - LipSyncDelaySeconds;

	FString CurrentViseme = TEXT("X");
	for (const FRhubarbMouthCue& Cue : MouthCues)
	{
		if (VisemeSampleTime >= Cue.Start && VisemeSampleTime < Cue.End)
		{
			CurrentViseme = Cue.Value;
			break;
		}
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::Yellow, FString::Printf(TEXT("Viseme: %s"), *CurrentViseme));
	}

	TArray<float> TargetCurveValues;
	VisemeToArKitMapping::GetWeightsForViseme(CurrentViseme, TargetCurveValues);

	for (int32 Index = 0; Index < CurrentCurveValues.Num(); ++Index)
	{
		CurrentCurveValues[Index] = FMath::FInterpTo(CurrentCurveValues[Index], TargetCurveValues[Index], DeltaTime, VisemeInterpSpeed);
	}

	LiveLinkSource->PushCurveFrame(CurrentCurveValues);
}

void ADynamicSpeechTestActor::SimulateIncomingChunk()
{
	TArray<uint8> WavBytes;
	if (!FFileHelper::LoadFileToArray(WavBytes, *TestWavPath.FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("DynamicSpeechTestActor: could not load %s"), *TestWavPath.FilePath);
		return;
	}

	ProcessIncomingAudioChunk(WavBytes, FLatencyTrace(), TEXT("Simulate"));
}

void ADynamicSpeechTestActor::ProcessIncomingAudioChunk(const TArray<uint8>& WavBytes, FLatencyTrace Trace, const FString& Source)
{
	Trace.ChunkReceived = FPlatformTime::Seconds();

	// --- Audio : parser l'entête WAV et construire un clip jouable sans passer par un asset importé.
	FWaveModInfo WaveInfo;
	FString WaveParseError;
	if (!WaveInfo.ReadWaveInfo(WavBytes.GetData(), WavBytes.Num(), &WaveParseError))
	{
		UE_LOG(LogTemp, Error, TEXT("DynamicSpeechTestActor: WAV parsing failed: %s"), *WaveParseError);
		return;
	}
	if (!WaveInfo.IsFormatUncompressed() || *WaveInfo.pBitsPerSample != 16)
	{
		UE_LOG(LogTemp, Error, TEXT("DynamicSpeechTestActor: expected 16-bit PCM WAV, got %d bits, format tag %d"),
			*WaveInfo.pBitsPerSample, *WaveInfo.pFormatTag);
		return;
	}
	Trace.WavParsed = FPlatformTime::Seconds();

	// Construit le clip mais ne le joue pas tout de suite : Rhubarb (ci-dessous) tourne en async et
	// met un temps variable à finir, donc démarrer Play() ici désynchroniserait audio et visèmes.
	// Les deux ne démarrent qu'ensemble, une fois les mouth cues prêtes (voir callback plus bas).
	TStrongObjectPtr<USoundWaveProcedural> SoundWave(NewObject<USoundWaveProcedural>());
	SoundWave->SetSampleRate(*WaveInfo.pSamplesPerSec);
	SoundWave->NumChannels = *WaveInfo.pChannels;
	SoundWave->Duration = static_cast<float>(WaveInfo.SampleDataSize) / static_cast<float>(*WaveInfo.pAvgBytesPerSec);
	SoundWave->QueueAudio(WaveInfo.SampleDataStart, WaveInfo.SampleDataSize);

	UE_LOG(LogTemp, Log, TEXT("DynamicSpeechTestActor: clip ready (%d Hz, %d ch, %.2fs), waiting for Rhubarb before playback"),
		*WaveInfo.pSamplesPerSec, *WaveInfo.pChannels, SoundWave->Duration);

	// --- Visèmes : écrire en fichier temporaire et lancer Rhubarb hors game thread (brique validée).
	const FString TempDir = FPaths::ProjectSavedDir() / TEXT("DynamicSpeech");
	IFileManager::Get().MakeDirectory(*TempDir, /*Tree*/ true);

	const FString TempWavPath = TempDir / FString::Printf(TEXT("chunk_%s.wav"), *FDateTime::Now().ToString(TEXT("%H%M%S_%s")));

	if (!FFileHelper::SaveArrayToFile(WavBytes, *TempWavPath))
	{
		UE_LOG(LogTemp, Error, TEXT("DynamicSpeechTestActor: could not write temp file %s"), *TempWavPath);
		return;
	}
	Trace.TempFileWritten = FPlatformTime::Seconds();

	// NewObject doit rester sur le game thread ; Rhubarb (process externe bloquant) est ensuite
	// lancé hors game thread pour ne pas geler l'éditeur pendant l'analyse.
	TStrongObjectPtr<URhubarbLipSyncRunner> Runner(NewObject<URhubarbLipSyncRunner>());
	TWeakObjectPtr<ADynamicSpeechTestActor> WeakThis(this);
	const bool bKeepFiles = bKeepTempAudio;

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Runner, TempWavPath, WeakThis, SoundWave, Trace, Source, bKeepFiles]() mutable
	{
		Trace.BackgroundTaskStarted = FPlatformTime::Seconds();

		TArray<FRhubarbMouthCue> ResultMouthCues;
		const bool bSuccess = Runner->RunOnAudioFile(TempWavPath, ResultMouthCues);
		Trace.RhubarbFinished = FPlatformTime::Seconds();

		// Le WAV temporaire et son JSON (mouth cues déjà parsées ci-dessus) ne servent plus une fois
		// Rhubarb terminé, succès ou non : on les efface pour ne pas accumuler indéfiniment dans Saved/,
		// sauf si bKeepTempAudio est coché (réutilisation via TestWavPath + SimulateIncomingChunk, pour
		// éviter de refaire un appel ElevenLabs à chaque test/vidéo).
		if (!bKeepFiles)
		{
			IFileManager::Get().Delete(*TempWavPath);
			IFileManager::Get().Delete(*FPaths::SetExtension(TempWavPath, TEXT("json")));
		}

		AsyncTask(ENamedThreads::GameThread, [bSuccess, ResultMouthCues, WeakThis, SoundWave, Trace, Source]() mutable
		{
			Trace.GameThreadTaskStarted = FPlatformTime::Seconds();

			ADynamicSpeechTestActor* Actor = WeakThis.Get();
			if (!Actor)
			{
				// L'acteur (ou le PIE) a été détruit pendant que Rhubarb tournait : rien à faire.
				return;
			}

			if (!bSuccess)
			{
				UE_LOG(LogTemp, Error, TEXT("DynamicSpeechTestActor: RunOnAudioFile failed"));
				return;
			}

			UE_LOG(LogTemp, Log, TEXT("DynamicSpeechTestActor: got %d mouth cues"), ResultMouthCues.Num());
			for (const FRhubarbMouthCue& Cue : ResultMouthCues)
			{
				UE_LOG(LogTemp, Log, TEXT("  [%.2f - %.2f] %s"), Cue.Start, Cue.End, *Cue.Value);
			}

			// Visèmes et audio démarrent ensemble, ici seulement : c'est ce qui garantit la synchro
			// (voir le commentaire plus haut sur pourquoi Play() n'est pas appelé plus tôt).
			Actor->MouthCues = ResultMouthCues;
			Actor->ElapsedPlaybackTime = 0.f;
			Actor->AudioPlayback->SetSound(SoundWave.Get());
			Trace.PlayStarted = FPlatformTime::Seconds();
			Actor->AudioPlayback->Play();
			Actor->AppendLatencySample(Trace, Source);
		});
	});
}

void ADynamicSpeechTestActor::AppendLatencySample(const FLatencyTrace& Trace, const FString& Source)
{
	const FString CsvPath = FPaths::ProjectSavedDir() / TEXT("DynamicSpeech") / TEXT("latency_log.csv");

	if (!IFileManager::Get().FileExists(*CsvPath))
	{
		const FString Header = TEXT("timestamp_iso,source,request_sent_to_response_ms,chunk_received_to_wav_parsed_ms,")
			TEXT("wav_parsed_to_tempfile_written_ms,tempfile_written_to_bgtask_started_ms,")
			TEXT("bgtask_started_to_rhubarb_finished_ms,rhubarb_finished_to_gamethread_started_ms,")
			TEXT("gamethread_started_to_play_ms,total_chunk_received_to_play_ms\n");
		FFileHelper::SaveStringToFile(Header, *CsvPath);
	}

	// Trace.RequestSent/ResponseReceived restent à 0 pour le chemin Simulate (pas de requête réseau) :
	// colonne laissée vide plutôt qu'à 0 pour ne pas la confondre avec une vraie mesure nulle.
	const FString NetworkMs = (Trace.RequestSent > 0.0 && Trace.ResponseReceived > 0.0)
		? FString::Printf(TEXT("%.2f"), (Trace.ResponseReceived - Trace.RequestSent) * 1000.0)
		: FString();

	const FString Line = FString::Printf(
		TEXT("%s,%s,%s,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f\n"),
		*FDateTime::Now().ToIso8601(),
		*Source,
		*NetworkMs,
		(Trace.WavParsed - Trace.ChunkReceived) * 1000.0,
		(Trace.TempFileWritten - Trace.WavParsed) * 1000.0,
		(Trace.BackgroundTaskStarted - Trace.TempFileWritten) * 1000.0,
		(Trace.RhubarbFinished - Trace.BackgroundTaskStarted) * 1000.0,
		(Trace.GameThreadTaskStarted - Trace.RhubarbFinished) * 1000.0,
		(Trace.PlayStarted - Trace.GameThreadTaskStarted) * 1000.0,
		(Trace.PlayStarted - Trace.ChunkReceived) * 1000.0);

	FFileHelper::SaveStringToFile(Line, *CsvPath, FFileHelper::EEncodingOptions::AutoDetect, &IFileManager::Get(), FILEWRITE_Append);
}

void ADynamicSpeechTestActor::RequestSpeechFromBackend()
{
	if (bHasSession)
	{
		SendSignedTtsRequest();
		return;
	}

	TWeakObjectPtr<ADynamicSpeechTestActor> WeakThis(this);
	FMiddlewareAuthClient::RequestSession(BackendBaseUrl, [WeakThis](bool bSuccess, FString Sid, FString SecretHex)
	{
		ADynamicSpeechTestActor* Actor = WeakThis.Get();
		if (!Actor)
		{
			return;
		}

		if (!bSuccess)
		{
			UE_LOG(LogTemp, Error, TEXT("DynamicSpeechTestActor: could not obtain a backend session (sid/secret)"));
			return;
		}

		Actor->CachedSid = Sid;
		Actor->CachedSecretHex = SecretHex;
		Actor->bHasSession = true;
		Actor->SendSignedTtsRequest();
	});
}

void ADynamicSpeechTestActor::SendSignedTtsRequest()
{
	TMap<FString, FString> QueryParams;
	QueryParams.Add(TEXT("q"), TextToSpeak);
	QueryParams.Add(TEXT("fmt"), TEXT("wav"));

	const FString SignedUrl = FMiddlewareAuthClient::BuildSignedUrl(
		BackendBaseUrl, TEXT("/api/v1/ai/tts"), QueryParams, CachedSid, CachedSecretHex);

	const TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(SignedUrl);
	Request->SetVerb(TEXT("GET"));

	TWeakObjectPtr<ADynamicSpeechTestActor> WeakThis(this);
	const double RequestSentTime = FPlatformTime::Seconds();
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, RequestSentTime](FHttpRequestPtr, const FHttpResponsePtr& Response, bool bSuccess)
	{
		const double ResponseReceivedTime = FPlatformTime::Seconds();

		ADynamicSpeechTestActor* Actor = WeakThis.Get();
		if (!Actor)
		{
			return;
		}

		if (!bSuccess || !Response.IsValid() || Response->GetResponseCode() != 200)
		{
			UE_LOG(LogTemp, Error, TEXT("DynamicSpeechTestActor: /api/v1/ai/tts failed (code %d): %s"),
				Response.IsValid() ? Response->GetResponseCode() : -1,
				Response.IsValid() ? *Response->GetContentAsString() : TEXT(""));
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("DynamicSpeechTestActor: received %d bytes from /api/v1/ai/tts"), Response->GetContent().Num());

		FLatencyTrace Trace;
		Trace.RequestSent = RequestSentTime;
		Trace.ResponseReceived = ResponseReceivedTime;
		Actor->ProcessIncomingAudioChunk(Response->GetContent(), Trace, TEXT("Backend"));
	});
	Request->ProcessRequest();
}
