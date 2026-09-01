// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RhubarbFaceActorBase.h"
#include "RhubarbMetaHumanActor.generated.h"

class USoundWave;

// Phase 1 MetaHuman test actor: plays a pre-recorded audio and pushes Rhubarb visemes as ARKit
// curves through a custom ILiveLinkSource (see ARhubarbFaceActorBase for the part shared with
// ADynamicSpeechTestActor). The MetaHuman must have "Use ARKit Face" = true and "ARKit Face
// Subject" = LiveLinkSubjectName (exposed on its Blueprint, e.g. BP_Ada) to listen to this stream.
UCLASS()
class REALTIMELIPSYNC_API ARhubarbMetaHumanActor : public ARhubarbFaceActorBase
{
	GENERATED_BODY()

public:
	// Sound to play. Its original disk path (needed by Rhubarb) is resolved automatically from
	// its import metadata
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync")
	USoundWave* SoundToPlay;

	// If enabled, bypasses audio/Rhubarb entirely, for hand-calibrating ARKit weights during Play
	// (live-editing the properties below in the Details panel).
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Debug")
	bool bDebugMode = false;

	// In debug mode: true uses the 8 DebugWeight_* sliders below, false previews the current
	// weights from the VisemeToArKitMapping table for DebugForcedViseme.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Debug", meta = (EditCondition = "bDebugMode"))
	bool bDebugUseManualWeights = true;

	// Viseme (A-H, X) whose table weights are previewed when bDebugUseManualWeights is false.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Debug", meta = (EditCondition = "bDebugMode && !bDebugUseManualWeights"))
	FString DebugForcedViseme = TEXT("X");

	// Manual weights (same curves, same order as VisemeToArKitMapping::GetUsedCurveNames), used
	// when bDebugUseManualWeights is true.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Debug", meta = (EditCondition = "bDebugMode && bDebugUseManualWeights", ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float DebugWeight_JawOpen = 0.f;

	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Debug", meta = (EditCondition = "bDebugMode && bDebugUseManualWeights", ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float DebugWeight_MouthClose = 0.f;

	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Debug", meta = (EditCondition = "bDebugMode && bDebugUseManualWeights", ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float DebugWeight_MouthFunnel = 0.f;

	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Debug", meta = (EditCondition = "bDebugMode && bDebugUseManualWeights", ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float DebugWeight_MouthPucker = 0.f;

	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Debug", meta = (EditCondition = "bDebugMode && bDebugUseManualWeights", ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float DebugWeight_MouthRollLower = 0.f;

	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Debug", meta = (EditCondition = "bDebugMode && bDebugUseManualWeights", ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float DebugWeight_MouthUpperUpLeft = 0.f;

	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Debug", meta = (EditCondition = "bDebugMode && bDebugUseManualWeights", ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float DebugWeight_MouthUpperUpRight = 0.f;

	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Debug", meta = (EditCondition = "bDebugMode && bDebugUseManualWeights", ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float DebugWeight_TongueOut = 0.f;

	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Debug", meta = (EditCondition = "bDebugMode && bDebugUseManualWeights", ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float DebugWeight_MouthLowerDownLeft = 0.f;

	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Debug", meta = (EditCondition = "bDebugMode && bDebugUseManualWeights", ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float DebugWeight_MouthLowerDownRight = 0.f;

	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Debug", meta = (EditCondition = "bDebugMode && bDebugUseManualWeights", ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float DebugWeight_MouthShrugLower = 0.f;

	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Debug", meta = (EditCondition = "bDebugMode && bDebugUseManualWeights", ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float DebugWeight_MouthShrugUpper = 0.f;

	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Debug", meta = (EditCondition = "bDebugMode && bDebugUseManualWeights", ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float DebugWeight_CheekPuff = 0.f;

	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Debug", meta = (EditCondition = "bDebugMode && bDebugUseManualWeights", ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float DebugWeight_MouthPressLeft = 0.f;

	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Debug", meta = (EditCondition = "bDebugMode && bDebugUseManualWeights", ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float DebugWeight_MouthPressRight = 0.f;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
};
