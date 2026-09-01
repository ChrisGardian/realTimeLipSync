// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "RhubarbLipSyncRunner.generated.h"

// A "mouth cue": a mouth shape (A-X) active between Start and End (seconds), as output by
// Rhubarb Lip Sync in the "mouthCues" JSON array.
USTRUCT(BlueprintType)
struct FRhubarbMouthCue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "RhubarbLipSync")
	float Start = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "RhubarbLipSync")
	float End = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "RhubarbLipSync")
	FString Value;
};

// Runs the Rhubarb Lip Sync executable on an audio file and parses the JSON viseme (mouth cue)
// timeline it produces. Blocking: call off the game thread for long audio, otherwise fine for
// Phase 1 tests.
UCLASS(BlueprintType)
class REALTIMELIPSYNC_API URhubarbLipSyncRunner : public UObject
{
	GENERATED_BODY()

public:
	// Runs Rhubarb on AudioFilePath, waits for the process to finish, parses the resulting JSON.
	// RhubarbExecutablePath is the absolute path to rhubarb.exe, provided by the caller (see the
	// RhubarbExecutablePath UPROPERTY on the actors that use this runner). The JSON is written
	// next to the audio file (same folder, same base name, .json extension) and overwritten on
	// every call.
	UFUNCTION(BlueprintCallable, Category = "RhubarbLipSync")
	bool RunOnAudioFile(const FString& AudioFilePath, const FString& RhubarbExecutablePath, TArray<FRhubarbMouthCue>& OutMouthCues);

private:
	// Builds the argument line passed to rhubarb.exe.
	FString BuildCommandLineArgs(const FString& AudioFilePath, const FString& OutputJsonPath) const;

	// Runs the process and blocks until it finishes. Returns false if the process could not be
	// created or its exit code indicates an error.
	bool ExecuteRhubarbProcess(const FString& RhubarbExecutablePath, const FString& Args) const;

	// Loads and parses the JSON file produced by Rhubarb into an array of mouth cues.
	bool ParseMouthCuesFromJson(const FString& JsonFilePath, TArray<FRhubarbMouthCue>& OutMouthCues) const;
};