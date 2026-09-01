// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RhubarbLipSyncRunner.h"
#include "RhubarbTestActor.generated.h"

class USkeletalMeshComponent;
class UAudioComponent;
class USoundWave;

// Phase 1 test actor: plays a pre-recorded audio and animates the test face's Morph Targets
// following the mouth cues produced by Rhubarb Lip Sync.
UCLASS()
class REALTIMELIPSYNC_API ARhubarbTestActor : public AActor
{
	GENERATED_BODY()

public:
	ARhubarbTestActor();

	// Test face, carrying the Viseme_A to Viseme_X Morph Targets.
	UPROPERTY(VisibleAnywhere, Category = "RhubarbLipSync")
	USkeletalMeshComponent* FaceMesh;

	// Plays the sound alongside the facial animation.
	UPROPERTY(VisibleAnywhere, Category = "RhubarbLipSync")
	UAudioComponent* AudioPlayback;

	// Sound to play. Its original disk path (needed by Rhubarb) is resolved automatically from
	// its import metadata (editor-only).
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync")
	USoundWave* SoundToPlay;

	// Maps a Rhubarb value ("A" to "X") to the Morph Target name on FaceMesh.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync")
	TMap<FString, FName> VisemeToMorphTarget;

	// Absolute path to rhubarb.exe on this machine.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync")
	FString RhubarbExecutablePath = TEXT("C:/Tools/Rhubarb-Lip-Sync-1.14.0-Windows/rhubarb.exe");

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

private:
	TArray<FRhubarbMouthCue> MouthCues;
	float ElapsedPlaybackTime = 0.f;
	int32 ActiveCueIndex = INDEX_NONE;
};
