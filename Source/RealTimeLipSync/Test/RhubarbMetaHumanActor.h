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
