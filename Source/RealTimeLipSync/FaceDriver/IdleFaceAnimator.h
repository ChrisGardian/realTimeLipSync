// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Calibratable settings for an FIdleFaceAnimator instance, exposed as UPROPERTY on the owning
// actor (Details panel) and passed to Update() on every tick rather than duplicated as internal
// state.
struct FIdleFaceAnimationSettings
{
	float MinBlinkInterval = 2.5f;
	float MaxBlinkInterval = 4.5f;
	float BlinkDuration = 0.2f;
};

// Groups the ARKit curve-driven "idle animation" micro-movements (eye blink for now, gaze and
// eyebrows planned later): timers and progress curves, to avoid duplicating this logic between
// ARhubarbMetaHumanActor and ADynamicSpeechTestActor. Head movement (bone rotation, not an ARKit
// curve) does not belong in this class until it is implemented.
class FIdleFaceAnimator
{
public:
	// Names of the ARKit curves pushed by this class. Append them to the end of the owning
	// actor's curve list before declaring the LiveLink subject (see WriteCurveValues).
	static const TArray<FName>& GetCurveNames();

	// Advances the blink state by DeltaTime.
	void Update(float DeltaTime, const FIdleFaceAnimationSettings& Settings);

	// Writes the current values into the last elements of OutFullCurveValues (same size as
	// GetCurveNames(), assuming these curves were appended to the end of the declared list).
	void WriteCurveValues(TArray<float>& OutFullCurveValues) const;

private:
	float TimeUntilNextBlink = 0.f;
	float BlinkElapsedTime = 0.f;
	float CurrentBlinkWeight = 0.f;
	bool bIsBlinking = false;
	bool bInitialized = false;
};
