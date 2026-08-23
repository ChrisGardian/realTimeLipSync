// Copyright Epic Games, Inc. All Rights Reserved.

#include "RhubarbMetaHumanActor.h"

#include "Components/AudioComponent.h"
#include "RhubarbLiveLinkSource.h"
#include "VisemeToArKitMapping.h"
#include "Features/IModularFeatures.h"
#include "ILiveLinkClient.h"
#include "Sound/SoundWave.h"

#if WITH_EDITORONLY_DATA
#include "EditorFramework/AssetImportData.h"
#endif

ARhubarbMetaHumanActor::ARhubarbMetaHumanActor()
{
	PrimaryActorTick.bCanEverTick = true;

	AudioPlayback = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioPlayback"));
	RootComponent = AudioPlayback;
	AudioPlayback->bAutoActivate = false;
}

void ARhubarbMetaHumanActor::BeginPlay()
{
	Super::BeginPlay();

	if (!IModularFeatures::Get().IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		UE_LOG(LogTemp, Error, TEXT("ARhubarbMetaHumanActor: LiveLink client modular feature not available"));
		return;
	}

	ILiveLinkClient& Client = IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
	LiveLinkSource = MakeShared<FRhubarbLiveLinkSource>(LiveLinkSubjectName);
	Client.AddSource(LiveLinkSource);
	LiveLinkSource->DeclareSubject(VisemeToArKitMapping::GetUsedCurveNames());
	CurrentCurveValues.Init(0.f, VisemeToArKitMapping::GetUsedCurveNames().Num());

	if (bDebugMode)
	{
		// Pas d'audio ni de Rhubarb en mode debug : Tick() pousse directement les poids de calibrage.
		return;
	}

	if (!SoundToPlay)
	{
		UE_LOG(LogTemp, Warning, TEXT("ARhubarbMetaHumanActor: SoundToPlay is not set"));
		return;
	}

	FString AudioFilePath;
#if WITH_EDITORONLY_DATA
	if (SoundToPlay->AssetImportData)
	{
		AudioFilePath = SoundToPlay->AssetImportData->GetFirstFilename();
	}
#endif

	if (AudioFilePath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("ARhubarbMetaHumanActor: could not resolve source file path for %s (editor-only data, see TODO.md)"), *SoundToPlay->GetName());
		return;
	}

	URhubarbLipSyncRunner* Runner = NewObject<URhubarbLipSyncRunner>(this);
	if (!Runner->RunOnAudioFile(AudioFilePath, MouthCues))
	{
		UE_LOG(LogTemp, Warning, TEXT("ARhubarbMetaHumanActor: RunOnAudioFile failed for %s"), *AudioFilePath);
		return;
	}

	ElapsedPlaybackTime = 0.f;

	AudioPlayback->SetSound(SoundToPlay);
	AudioPlayback->Play();
}

void ARhubarbMetaHumanActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
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

void ARhubarbMetaHumanActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!LiveLinkSource.IsValid())
	{
		return;
	}

	if (bDebugMode)
	{
		TArray<float> DebugCurveValues;
		if (bDebugUseManualWeights)
		{
			DebugCurveValues = {
				DebugWeight_JawOpen,
				DebugWeight_MouthClose,
				DebugWeight_MouthFunnel,
				DebugWeight_MouthPucker,
				DebugWeight_MouthRollLower,
				DebugWeight_MouthUpperUpLeft,
				DebugWeight_MouthUpperUpRight,
				DebugWeight_TongueOut,
				DebugWeight_MouthLowerDownLeft,
				DebugWeight_MouthLowerDownRight,
				DebugWeight_MouthShrugLower,
				DebugWeight_MouthShrugUpper,
				DebugWeight_CheekPuff,
				DebugWeight_MouthPressLeft,
				DebugWeight_MouthPressRight,
			};
		}
		else
		{
			VisemeToArKitMapping::GetWeightsForViseme(DebugForcedViseme, DebugCurveValues);
		}

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::Cyan, bDebugUseManualWeights
				? TEXT("Debug: manual weights")
				: FString::Printf(TEXT("Debug: forced viseme %s"), *DebugForcedViseme));
		}

		// Pas d'interpolation ici : on veut voir l'effet exact des sliders, sans lissage.
		LiveLinkSource->PushCurveFrame(DebugCurveValues);
		return;
	}

	if (MouthCues.Num() == 0)
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
