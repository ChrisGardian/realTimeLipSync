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

void ARhubarbMetaHumanActor::Tick(float DeltaTime)
{
	if (bDebugMode)
	{
		// Tourne indépendamment de la logique commune (pas de lissage, pas de blink, pas de MouthCues) :
		// on saute ARhubarbFaceActorBase::Tick, mais AActor::Tick doit quand même s'exécuter.
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

		// Pas de blink en mode debug : l'outil de calibrage doit rester figé sur les sliders manuels.
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

		// Pas d'interpolation ici : on veut voir l'effet exact des sliders, sans lissage.
		LiveLinkSource->PushCurveFrame(DebugCurveValues);
		return;
	}

	Super::Tick(DeltaTime);
}
