// Copyright Epic Games, Inc. All Rights Reserved.

#include "DynamicSpeechTestActor.h"

#include "Async/Async.h"
#include "Audio.h"
#include "Components/AudioComponent.h"
#include "Features/IModularFeatures.h"
#include "HAL/FileManager.h"
#include "ILiveLinkClient.h"
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

	ProcessIncomingAudioChunk(WavBytes);
}

void ADynamicSpeechTestActor::ProcessIncomingAudioChunk(const TArray<uint8>& WavBytes)
{
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

	USoundWaveProcedural* SoundWave = NewObject<USoundWaveProcedural>(this);
	SoundWave->SetSampleRate(*WaveInfo.pSamplesPerSec);
	SoundWave->NumChannels = *WaveInfo.pChannels;
	SoundWave->Duration = static_cast<float>(WaveInfo.SampleDataSize) / static_cast<float>(*WaveInfo.pAvgBytesPerSec);
	SoundWave->QueueAudio(WaveInfo.SampleDataStart, WaveInfo.SampleDataSize);

	MouthCues.Reset();
	ElapsedPlaybackTime = 0.f;
	AudioPlayback->SetSound(SoundWave);
	AudioPlayback->Play();

	UE_LOG(LogTemp, Log, TEXT("DynamicSpeechTestActor: playing runtime clip (%d Hz, %d ch, %.2fs)"),
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

	// NewObject doit rester sur le game thread ; Rhubarb (process externe bloquant) est ensuite
	// lancé hors game thread pour ne pas geler l'éditeur pendant l'analyse.
	TStrongObjectPtr<URhubarbLipSyncRunner> Runner(NewObject<URhubarbLipSyncRunner>());
	TWeakObjectPtr<ADynamicSpeechTestActor> WeakThis(this);

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Runner, TempWavPath, WeakThis]()
	{
		TArray<FRhubarbMouthCue> ResultMouthCues;
		const bool bSuccess = Runner->RunOnAudioFile(TempWavPath, ResultMouthCues);

		AsyncTask(ENamedThreads::GameThread, [bSuccess, ResultMouthCues, WeakThis]()
		{
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

			// Rearme le driver de timing du Tick : les visèmes commencent à être poussés via LiveLink
			// à partir d'ici, avec le retard éventuel accumulé depuis Play() (voir TODO.md, limitation connue).
			Actor->MouthCues = ResultMouthCues;
		});
	});
}
