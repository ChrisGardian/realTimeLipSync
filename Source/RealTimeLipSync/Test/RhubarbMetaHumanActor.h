// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RhubarbLipSyncRunner.h"
#include "RhubarbMetaHumanActor.generated.h"

class UAudioComponent;
class USoundWave;
class FRhubarbLiveLinkSource;

// Actor de test Phase 1 pour MetaHuman : joue un audio pré-enregistré et pousse les visèmes
// Rhubarb comme curves ARKit via un ILiveLinkSource custom (FRhubarbLiveLinkSource).
// Le MetaHuman doit avoir "Use ARKit Face" = true et "ARKit Face Subject" = LiveLinkSubjectName
// (variables exposées sur son Blueprint, ex. BP_Ada) pour écouter ce flux.
UCLASS()
class REALTIMELIPSYNC_API ARhubarbMetaHumanActor : public AActor
{
	GENERATED_BODY()

public:
	ARhubarbMetaHumanActor();

	// Lit le son en même temps que l'animation faciale.
	UPROPERTY(VisibleAnywhere, Category = "RhubarbLipSync")
	UAudioComponent* AudioPlayback;

	// Le son à jouer. Son chemin disque d'origine (nécessaire pour Rhubarb) est retrouvé
	// automatiquement via ses métadonnées d'import (editor-only, voir TODO.md).
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync")
	USoundWave* SoundToPlay;

	// Nom du LiveLink Subject poussé par cet actor. Doit correspondre à "ARKit Face Subject"
	// sur le Blueprint du MetaHuman.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync")
	FName LiveLinkSubjectName = TEXT("RhubarbLipSync");

	// Vitesse de lissage (FInterpTo) entre les poids du visème courant et ceux du visème cible.
	// Plus haut = transition plus rapide/plus proche du "pop" d'origine ; plus bas = plus lisse.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync")
	float VisemeInterpSpeed = 20.f;

	// Décalage (en secondes) entre le temps de lecture audio et l'instant utilisé pour échantillonner
	// les visèmes. Positif = la bouche bouge plus tard que le son (compense une latence LiveLink/AnimBP
	// en aval) ; négatif = la bouche bouge plus tôt. A régler empiriquement en Play.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync")
	float LipSyncDelaySeconds = 0.f;

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
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaTime) override;

private:
	TSharedPtr<FRhubarbLiveLinkSource> LiveLinkSource;
	TArray<FRhubarbMouthCue> MouthCues;
	float ElapsedPlaybackTime = 0.f;
	TArray<float> CurrentCurveValues;
};
