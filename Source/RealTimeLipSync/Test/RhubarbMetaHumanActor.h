// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RhubarbFaceActorBase.h"
#include "RhubarbMetaHumanActor.generated.h"

class USoundWave;

// Actor de test Phase 1 pour MetaHuman : joue un audio pré-enregistré et pousse les visèmes
// Rhubarb comme curves ARKit via un ILiveLinkSource custom (voir ARhubarbFaceActorBase pour la
// partie commune avec ADynamicSpeechTestActor). Le MetaHuman doit avoir "Use ARKit Face" = true
// et "ARKit Face Subject" = LiveLinkSubjectName (variables exposées sur son Blueprint, ex. BP_Ada)
// pour écouter ce flux.
UCLASS()
class REALTIMELIPSYNC_API ARhubarbMetaHumanActor : public ARhubarbFaceActorBase
{
	GENERATED_BODY()

public:
	// Le son à jouer. Son chemin disque d'origine (nécessaire pour Rhubarb) est retrouvé
	// automatiquement via ses métadonnées d'import (editor-only, voir TODO.md).
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync")
	USoundWave* SoundToPlay;

	// Si activé, ignore complètement l'audio/Rhubarb : sert à calibrer les poids ARKit à la main
	// pendant le Play (édition live des propriétés ci-dessous dans le Details panel).
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Debug")
	bool bDebugMode = false;

	// En mode debug : true = utiliser les 8 sliders DebugWeight_* ci-dessous ; false = prévisualiser
	// les poids actuels de la table VisemeToArKitMapping pour DebugForcedViseme.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Debug", meta = (EditCondition = "bDebugMode"))
	bool bDebugUseManualWeights = true;

	// Visème (A-H, X) dont on prévisualise les poids de la table quand bDebugUseManualWeights = false.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Debug", meta = (EditCondition = "bDebugMode && !bDebugUseManualWeights"))
	FString DebugForcedViseme = TEXT("X");

	// Poids manuels (mêmes curves, même ordre que VisemeToArKitMapping::GetUsedCurveNames) utilisés
	// quand bDebugUseManualWeights = true.
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
