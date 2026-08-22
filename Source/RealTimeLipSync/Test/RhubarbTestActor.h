// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RhubarbLipSyncRunner.h"
#include "RhubarbTestActor.generated.h"

class USkeletalMeshComponent;
class UAudioComponent;
class USoundWave;

// Actor de test Phase 1 : joue un audio pré-enregistré et anime les Morph Targets
// du visage de test en suivant les mouth cues produites par Rhubarb Lip Sync.
UCLASS()
class REALTIMELIPSYNC_API ARhubarbTestActor : public AActor
{
	GENERATED_BODY()

public:
	ARhubarbTestActor();

	// Le visage de test, portant les Morph Targets Viseme_A à Viseme_X.
	UPROPERTY(VisibleAnywhere, Category = "RhubarbLipSync")
	USkeletalMeshComponent* FaceMesh;

	// Lit le son en même temps que l'animation faciale.
	UPROPERTY(VisibleAnywhere, Category = "RhubarbLipSync")
	UAudioComponent* AudioPlayback;

	// Le son à jouer. Son chemin disque d'origine (nécessaire pour Rhubarb) est retrouvé
	// automatiquement via ses métadonnées d'import (editor-only, voir TODO.md).
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync")
	USoundWave* SoundToPlay;

	// Mapping valeur Rhubarb ("A".."X") -> nom du Morph Target sur FaceMesh.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync")
	TMap<FString, FName> VisemeToMorphTarget;

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

private:
	TArray<FRhubarbMouthCue> MouthCues;
	float ElapsedPlaybackTime = 0.f;
	int32 ActiveCueIndex = INDEX_NONE;
};
