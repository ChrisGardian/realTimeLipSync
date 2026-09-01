// Copyright Epic Games, Inc. All Rights Reserved.

#include "RhubarbMetaHumanActor.h"

#include "Components/AudioComponent.h"
#include "RhubarbLiveLinkSource.h"
#include "VisemeToArKitMapping.h"
#include "Sound/SoundWave.h"

#if WITH_EDITORONLY_DATA
#include "EditorFramework/AssetImportData.h"
#endif

void ARhubarbMetaHumanActor::BeginPlay()
{
	Super::BeginPlay();

	if (bDebugMode)
	{
		// No audio or Rhubarb in debug mode; Tick() pushes the calibration weights directly.
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
		UE_LOG(LogTemp, Warning, TEXT("ARhubarbMetaHumanActor: could not resolve source file path for %s (editor-only data)"), *SoundToPlay->GetName());
		return;
	}

	URhubarbLipSyncRunner* Runner = NewObject<URhubarbLipSyncRunner>(this);
	if (!Runner->RunOnAudioFile(AudioFilePath, RhubarbExecutablePath, MouthCues))
	{
		UE_LOG(LogTemp, Warning, TEXT("ARhubarbMetaHumanActor: RunOnAudioFile failed for %s"), *AudioFilePath);
		return;
	}

	ElapsedPlaybackTime = 0.f;

	AudioPlayback->SetSound(SoundToPlay);
	AudioPlayback->Play();
}

void ARhubarbMetaHumanActor::Tick(float DeltaTime)
{
	if (bDebugMode)
	{
		// Skips ARhubarbFaceActorBase::Tick (no smoothing, no blink, no MouthCues), but AActor::Tick
		// must still run.
		AActor::Tick(DeltaTime);

		if (!LiveLinkSource.IsValid())
		{
			return;
		}

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

		// No blink in debug mode: the calibration tool must stay locked to the manual sliders.
		for (int32 Index = 0; Index < FIdleFaceAnimator::GetCurveNames().Num(); ++Index)
		{
			DebugCurveValues.Add(0.f);
		}

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::Cyan, bDebugUseManualWeights
				? TEXT("Debug: manual weights")
				: FString::Printf(TEXT("Debug: forced viseme %s"), *DebugForcedViseme));
		}

		// No interpolation here: the exact effect of the sliders should be visible, unsmoothed.
		LiveLinkSource->PushCurveFrame(DebugCurveValues);
		return;
	}

	Super::Tick(DeltaTime);
}
