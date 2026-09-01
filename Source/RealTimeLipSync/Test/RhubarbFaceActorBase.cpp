// Copyright Epic Games, Inc. All Rights Reserved.

#include "RhubarbFaceActorBase.h"

#include "Async/Async.h"
#include "Audio.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
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

	// Mouth curves (VisemeToArKitMapping) and idle animation curves (FIdleFaceAnimator) declared
	// together, in this order: a single LiveLink subject for the whole face.
	TArray<FName> AllCurveNames = VisemeToArKitMapping::GetUsedCurveNames();
	AllCurveNames.Append(FIdleFaceAnimator::GetCurveNames());
	LiveLinkSource->DeclareSubject(AllCurveNames);
	CurrentCurveValues.Init(0.f, AllCurveNames.Num());

	// Idle body animation: starts at BeginPlay, same as the blink, so the NPC is not frozen before
	// the first sound. BodyActor has two USkeletalMeshComponent (Face and Body); filter by name so
	// the animation does not play on the wrong component.
	if (BodyActor && IdleBodyAnimation)
	{
		TArray<USkeletalMeshComponent*> SkeletalMeshComponents;
		BodyActor->GetComponents(SkeletalMeshComponents);

		USkeletalMeshComponent* BodyMesh = nullptr;
		for (USkeletalMeshComponent* Component : SkeletalMeshComponents)
		{
			if (Component->GetName() == TEXT("Body"))
			{
				BodyMesh = Component;
				break;
			}
		}

		if (BodyMesh)
		{
			BodyMesh->PlayAnimation(IdleBodyAnimation, /*bLooping=*/ true);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("%s: BodyActor %s has no component named \"Body\""),
				*GetClass()->GetName(), *BodyActor->GetName());
		}
	}
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

	// Viseme sampling only runs once audio has been processed (MouthCues filled); until then the
	// mouth curves stay at their initial value, but the idle animation below keeps running from
	// BeginPlay regardless of whether a sound is playing.
	if (MouthCues.Num() > 0)
	{
		ElapsedPlaybackTime += DeltaTime;

		// Offset from the real audio playback time (see LipSyncDelaySeconds). Falls back to "X"
		// (neutral) before time 0 or after the last cue.
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

	// Runs as soon as the LiveLink subject exists, independently of MouthCues, so the NPC is not
	// frozen before the first sound
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

	// Parse the WAV header and build a playable clip without going through an imported asset.
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

	// Built but not played yet: Rhubarb below runs async and takes a variable time to finish, so
	// starting Play() here would desync audio and visemes. Both start together once the mouth
	// cues are ready (see callback below).
	TStrongObjectPtr<USoundWaveProcedural> SoundWave(NewObject<USoundWaveProcedural>());
	SoundWave->SetSampleRate(*WaveInfo.pSamplesPerSec);
	SoundWave->NumChannels = *WaveInfo.pChannels;
	SoundWave->Duration = static_cast<float>(WaveInfo.SampleDataSize) / static_cast<float>(*WaveInfo.pAvgBytesPerSec);
	SoundWave->QueueAudio(WaveInfo.SampleDataStart, WaveInfo.SampleDataSize);

	UE_LOG(LogTemp, Log, TEXT("%s: clip ready (%d Hz, %d ch, %.2fs), waiting for Rhubarb before playback"),
		*GetClass()->GetName(), *WaveInfo.pSamplesPerSec, *WaveInfo.pChannels, SoundWave->Duration);

	const FString TempDir = FPaths::ProjectSavedDir() / TEXT("DynamicSpeech");
	IFileManager::Get().MakeDirectory(*TempDir, /*Tree*/ true);

	const FString TempWavPath = TempDir / FString::Printf(TEXT("chunk_%s.wav"), *FDateTime::Now().ToString(TEXT("%H%M%S_%s")));

	if (!FFileHelper::SaveArrayToFile(WavBytes, *TempWavPath))
	{
		UE_LOG(LogTemp, Error, TEXT("%s: could not write temp file %s"), *GetClass()->GetName(), *TempWavPath);
		return;
	}
	Trace.TempFileWritten = FPlatformTime::Seconds();

	// NewObject must stay on the game thread; Rhubarb itself (a blocking external process) then
	// runs off the game thread so the editor does not freeze during analysis.
	TStrongObjectPtr<URhubarbLipSyncRunner> Runner(NewObject<URhubarbLipSyncRunner>());
	TWeakObjectPtr<ARhubarbFaceActorBase> WeakThis(this);
	const bool bKeepFiles = bKeepTempAudio;
	const FString ExecutablePath = RhubarbExecutablePath;

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Runner, TempWavPath, WeakThis, SoundWave, Trace, Source, bKeepFiles, ExecutablePath]() mutable
	{
		Trace.BackgroundTaskStarted = FPlatformTime::Seconds();

		TArray<FRhubarbMouthCue> ResultMouthCues;
		const bool bSuccess = Runner->RunOnAudioFile(TempWavPath, ExecutablePath, ResultMouthCues);
		Trace.RhubarbFinished = FPlatformTime::Seconds();

		// Delete the temp WAV and JSON unless bKeepTempAudio is set (reuse via TestWavPath and
		// SimulateIncomingChunk, to avoid a fresh ElevenLabs call for every test/video).
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
				// Actor (or PIE session) was destroyed while Rhubarb was running.
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

	// Left empty rather than 0 when the caller made no network request (e.g. SimulateIncomingChunk),
	// so it is not confused with an actual zero measurement.
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
