// Copyright Epic Games, Inc. All Rights Reserved.

#include "RhubarbFaceActorBase.h"

#include "Async/Async.h"
#include "Audio.h"
#include "Components/AudioComponent.h"
#include "RhubarbLiveLinkSource.h"
#include "VisemeToArKitMapping.h"
#include "Features/IModularFeatures.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "ILiveLinkClient.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sound/SoundWaveProcedural.h"
#include "UObject/StrongObjectPtr.h"

ARhubarbFaceActorBase::ARhubarbFaceActorBase()
{
	PrimaryActorTick.bCanEverTick = true;

	AudioPlayback = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioPlayback"));
	RootComponent = AudioPlayback;
	AudioPlayback->bAutoActivate = false;
}

void ARhubarbFaceActorBase::BeginPlay()
{
	Super::BeginPlay();

	if (!IModularFeatures::Get().IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		UE_LOG(LogTemp, Error, TEXT("%s: LiveLink client modular feature not available"), *GetClass()->GetName());
		return;
	}

	ILiveLinkClient& Client = IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
	LiveLinkSource = MakeShared<FRhubarbLiveLinkSource>(LiveLinkSubjectName);
	Client.AddSource(LiveLinkSource);

	// Curves de bouche (VisemeToArKitMapping) + curves d'idle animation (FIdleFaceAnimator) déclarées
	// ensemble, dans cet ordre : un seul subject LiveLink pour tout le visage.
	TArray<FName> AllCurveNames = VisemeToArKitMapping::GetUsedCurveNames();
	AllCurveNames.Append(FIdleFaceAnimator::GetCurveNames());
	LiveLinkSource->DeclareSubject(AllCurveNames);
	CurrentCurveValues.Init(0.f, AllCurveNames.Num());
}

void ARhubarbFaceActorBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
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

void ARhubarbFaceActorBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!LiveLinkSource.IsValid())
	{
		return;
	}

	// Échantillonnage des visèmes : seulement une fois qu'un audio a été traité (MouthCues rempli).
	// Avant ça, les curves de bouche restent à leur valeur initiale (0, bouche neutre) — mais l'idle
	// animation plus bas continue de tourner dès BeginPlay, pas seulement pendant la lecture d'un son.
	if (MouthCues.Num() > 0)
	{
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

		for (int32 Index = 0; Index < TargetCurveValues.Num(); ++Index)
		{
			CurrentCurveValues[Index] = FMath::FInterpTo(CurrentCurveValues[Index], TargetCurveValues[Index], DeltaTime, VisemeInterpSpeed);
		}
	}

	// Idle animation : tourne dès que le subject LiveLink existe, indépendamment de MouthCues, pour
	// que le NPC ne reste pas figé avant le premier son (voir TODO.md). Écrit directement les derniers
	// éléments de CurrentCurveValues (voir FIdleFaceAnimator::WriteCurveValues).
	FIdleFaceAnimationSettings IdleSettings;
	IdleSettings.MinBlinkInterval = MinBlinkInterval;
	IdleSettings.MaxBlinkInterval = MaxBlinkInterval;
	IdleSettings.BlinkDuration = BlinkDuration;
	IdleFaceAnimator.Update(DeltaTime, IdleSettings);
	IdleFaceAnimator.WriteCurveValues(CurrentCurveValues);

	LiveLinkSource->PushCurveFrame(CurrentCurveValues);
}

void ARhubarbFaceActorBase::ProcessIncomingAudioChunk(const TArray<uint8>& WavBytes, FLatencyTrace Trace, const FString& Source)
{
	Trace.ChunkReceived = FPlatformTime::Seconds();

	// --- Audio : parser l'entête WAV et construire un clip jouable sans passer par un asset importé.
	FWaveModInfo WaveInfo;
	FString WaveParseError;
	if (!WaveInfo.ReadWaveInfo(WavBytes.GetData(), WavBytes.Num(), &WaveParseError))
	{
		UE_LOG(LogTemp, Error, TEXT("%s: WAV parsing failed: %s"), *GetClass()->GetName(), *WaveParseError);
		return;
	}
	if (!WaveInfo.IsFormatUncompressed() || *WaveInfo.pBitsPerSample != 16)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: expected 16-bit PCM WAV, got %d bits, format tag %d"),
			*GetClass()->GetName(), *WaveInfo.pBitsPerSample, *WaveInfo.pFormatTag);
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

	UE_LOG(LogTemp, Log, TEXT("%s: clip ready (%d Hz, %d ch, %.2fs), waiting for Rhubarb before playback"),
		*GetClass()->GetName(), *WaveInfo.pSamplesPerSec, *WaveInfo.pChannels, SoundWave->Duration);

	// --- Visèmes : écrire en fichier temporaire et lancer Rhubarb hors game thread (brique validée).
	const FString TempDir = FPaths::ProjectSavedDir() / TEXT("DynamicSpeech");
	IFileManager::Get().MakeDirectory(*TempDir, /*Tree*/ true);

	const FString TempWavPath = TempDir / FString::Printf(TEXT("chunk_%s.wav"), *FDateTime::Now().ToString(TEXT("%H%M%S_%s")));

	if (!FFileHelper::SaveArrayToFile(WavBytes, *TempWavPath))
	{
		UE_LOG(LogTemp, Error, TEXT("%s: could not write temp file %s"), *GetClass()->GetName(), *TempWavPath);
		return;
	}
	Trace.TempFileWritten = FPlatformTime::Seconds();

	// NewObject doit rester sur le game thread ; Rhubarb (process externe bloquant) est ensuite
	// lancé hors game thread pour ne pas geler l'éditeur pendant l'analyse.
	TStrongObjectPtr<URhubarbLipSyncRunner> Runner(NewObject<URhubarbLipSyncRunner>());
	TWeakObjectPtr<ARhubarbFaceActorBase> WeakThis(this);
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

			ARhubarbFaceActorBase* Actor = WeakThis.Get();
			if (!Actor)
			{
				// L'acteur (ou le PIE) a été détruit pendant que Rhubarb tournait : rien à faire.
				return;
			}

			if (!bSuccess)
			{
				UE_LOG(LogTemp, Error, TEXT("%s: RunOnAudioFile failed"), *Actor->GetClass()->GetName());
				return;
			}

			UE_LOG(LogTemp, Log, TEXT("%s: got %d mouth cues"), *Actor->GetClass()->GetName(), ResultMouthCues.Num());
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

void ARhubarbFaceActorBase::AppendLatencySample(const FLatencyTrace& Trace, const FString& Source)
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

	// Trace.RequestSent/ResponseReceived restent à 0 quand l'appelant n'a pas fait de requête réseau
	// (ex: SimulateIncomingChunk) : colonne laissée vide plutôt qu'à 0 pour ne pas la confondre avec
	// une vraie mesure nulle.
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
