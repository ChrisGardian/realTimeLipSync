// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IdleFaceAnimator.h"
#include "RhubarbLipSyncRunner.h"
#include "RhubarbFaceActorBase.generated.h"

class UAudioComponent;
class USkeletalMeshComponent;
class UAnimSequence;
class FRhubarbLiveLinkSource;

// Timestamps (FPlatformTime::Seconds(), monotonic clock) taken at each step of the "packet
// received to animation started" pipeline, to measure compute latency budgets (thesis, see
// ARhubarbFaceActorBase::AppendLatencySample). RequestSent/ResponseReceived stay at 0 when the
// caller made no network request (e.g. ADynamicSpeechTestActor::SimulateIncomingChunk); in that
// case AppendLatencySample leaves the network column empty instead of 0.
struct FLatencyTrace
{
	double RequestSent = 0.0;
	double ResponseReceived = 0.0;
	double ChunkReceived = 0.0;
	double WavParsed = 0.0;
	double TempFileWritten = 0.0;
	double BackgroundTaskStarted = 0.0;
	double RhubarbFinished = 0.0;
	double GameThreadTaskStarted = 0.0;
	double PlayStarted = 0.0;
};

// Base shared by actors that drive a MetaHuman face via ARKit curves pushed through a custom
// ILiveLinkSource (Rhubarb visemes plus idle animation): ARhubarbMetaHumanActor (Phase 1,
// pre-recorded audio), ADynamicSpeechTestActor (Phase 2, audio received from the backend) and
// ADemoScenarioActor (the "near-finished product" demo). Groups the LiveLink setup, mouth cue
// sampling, FInterpTo smoothing, blinking, and the "WAV bytes received to playable clip to async
// Rhubarb to Play" pipeline with its latency measurement (see ProcessIncomingAudioChunk below).
// Everything that does not depend on how the audio was obtained lives here; each subclass stays
// responsible for obtaining the WAV bytes (e.g. local file vs. signed HTTP request).
UCLASS(Abstract)
class REALTIMELIPSYNC_API ARhubarbFaceActorBase : public AActor
{
	GENERATED_BODY()

public:
	ARhubarbFaceActorBase();

	// Plays the sound alongside the facial animation.
	UPROPERTY(VisibleAnywhere, Category = "RhubarbLipSync")
	UAudioComponent* AudioPlayback;

	// Name of the LiveLink Subject pushed by this actor. Must match "ARKit Face Subject"
	// on the MetaHuman Blueprint.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync")
	FName LiveLinkSubjectName = TEXT("RhubarbLipSync");

	// Smoothing speed (FInterpTo) between the current viseme weights and the target ones.
	// Higher means a faster transition, closer to the original "pop"; lower means smoother.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync")
	float VisemeInterpSpeed = 20.f;

	// Offset (in seconds) between the audio playback time and the instant used to sample the
	// visemes. Positive means the mouth moves later than the sound (compensates for downstream
	// LiveLink/AnimBP latency); negative means the mouth moves earlier. Tune empirically in Play.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync")
	float LipSyncDelaySeconds = 0.f;

	// Idle animation:
	// random interval between two blinks, in seconds. Calibration reference: a human blinks
	// roughly every 2s, irregularly, so a random interval is drawn between these two bounds
	// on every blink.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Blink")
	float MinBlinkInterval = 2.5f;

	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Blink")
	float MaxBlinkInterval = 4.5f;

	// Total duration of a blink (closing plus reopening). Linear triangle curve (0 to 1 to 0)
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Blink")
	float BlinkDuration = 0.2f;

	// If true, the temp .wav and its Rhubarb .json written by ProcessIncomingAudioChunk are not
	// deleted after use. Useful to reuse already generated audio without repeating an ElevenLabs
	// call for every test/video (see ADynamicSpeechTestActor::TestWavPath).
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync")
	bool bKeepTempAudio = false;

	// Absolute path to rhubarb.exe on this machine.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync")
	FString RhubarbExecutablePath = TEXT("C:/Tools/Rhubarb-Lip-Sync-1.14.0-Windows/rhubarb.exe");

	// MetaHuman actor placed in the level (BP_Ada/BP_Taro) whose body should be animated. This
	// C++ actor does not contain the mesh itself; it only pushes LiveLink curves to the MetaHuman,
	// so the reference is assigned in the level's Details panel.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Body")
	AActor* BodyActor = nullptr;

	// Lightweight idle animation (retargeted Mixamo) looped on BodyActor's "Body" component from
	// BeginPlay
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Body")
	UAnimSequence* IdleBodyAnimation = nullptr;

protected:
	// Creates the LiveLink source and declares the subject (mouth curves plus idle animation).
	// Subclasses that need to load audio in BeginPlay call Super::BeginPlay() then their own logic.
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Samples MouthCues at the current time, smooths towards the target ARKit weights, advances
	// the blink, and pushes the resulting curve frame. Does nothing if MouthCues is empty (before
	// the subclass has loaded audio); see ARhubarbMetaHumanActor for a subclass that bypasses this
	// behavior entirely (debug mode).
	virtual void Tick(float DeltaTime) override;

	// Single entry point for the "WAV bytes received to playable clip to async Rhubarb to Play"
	// pipeline, shared by every subclass that receives audio to process regardless of the source
	// (local file, HTTP response from /api/v1/ai/tts, etc.). Builds a USoundWaveProcedural without
	// going through an imported asset, writes a temp file, runs Rhubarb on it on a background
	// thread (blocking process, hence off the game thread), then arms MouthCues and starts Play()
	// once the mouth cues are ready. Both start together, on purpose, to guarantee audio/viseme
	// sync. Trace is filled in as the pipeline progresses and logged (AppendLatencySample) once
	// the animation starts; Source identifies the caller in the CSV (e.g. "Simulate", "Backend",
	// "DemoIntro", "DemoAsk").
	void ProcessIncomingAudioChunk(const TArray<uint8>& WavBytes, FLatencyTrace Trace, const FString& Source);

	// Appends a row to Saved/DynamicSpeech/latency_log.csv (one delta in ms per pipeline step,
	// network column left empty if Trace.RequestSent/ResponseReceived are 0). Writes the header
	// if the file does not exist yet. Shared so every measurement (raw tests and demo) lives in
	// the same file, with Source used to tell them apart during analysis.
	void AppendLatencySample(const FLatencyTrace& Trace, const FString& Source);

	TSharedPtr<FRhubarbLiveLinkSource> LiveLinkSource;
	TArray<FRhubarbMouthCue> MouthCues;
	float ElapsedPlaybackTime = 0.f;
	TArray<float> CurrentCurveValues;

	// Eye blink for now; other curve-based idle micro-movements (gaze, eyebrows) will join this
	// same instance later (see IdleFaceAnimator.h).
	FIdleFaceAnimator IdleFaceAnimator;
};
