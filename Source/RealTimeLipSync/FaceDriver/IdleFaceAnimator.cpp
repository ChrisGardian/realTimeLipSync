// Copyright Epic Games, Inc. All Rights Reserved.

#include "IdleFaceAnimator.h"

namespace
{
	const TArray<FName> IdleCurveNames = {
		TEXT("EyeBlinkLeft"),
		TEXT("EyeBlinkRight"),
	};
}

const TArray<FName>& FIdleFaceAnimator::GetCurveNames()
{
	return IdleCurveNames;
}

void FIdleFaceAnimator::Update(float DeltaTime, const FIdleFaceAnimationSettings& Settings)
{
	if (!bInitialized)
	{
		TimeUntilNextBlink = FMath::RandRange(Settings.MinBlinkInterval, Settings.MaxBlinkInterval);
		bInitialized = true;
	}

	if (bIsBlinking)
	{
		BlinkElapsedTime += DeltaTime;
		if (BlinkElapsedTime >= Settings.BlinkDuration)
		{
			bIsBlinking = false;
			TimeUntilNextBlink = FMath::RandRange(Settings.MinBlinkInterval, Settings.MaxBlinkInterval);
		}
	}
	else
	{
		TimeUntilNextBlink -= DeltaTime;
		if (TimeUntilNextBlink <= 0.f)
		{
			bIsBlinking = true;
			BlinkElapsedTime = 0.f;
		}
	}

	// Linear triangle from 0 to 1 to 0 over the blink duration.
	CurrentBlinkWeight = (bIsBlinking && Settings.BlinkDuration > 0.f)
		? 1.f - FMath::Abs(1.f - 2.f * (BlinkElapsedTime / Settings.BlinkDuration))
		: 0.f;
}

void FIdleFaceAnimator::WriteCurveValues(TArray<float>& OutFullCurveValues) const
{
	const int32 Offset = OutFullCurveValues.Num() - IdleCurveNames.Num();
	if (Offset < 0)
	{
		return;
	}

	// EyeBlinkLeft, EyeBlinkRight: same weight for both eyes for now.
	OutFullCurveValues[Offset] = CurrentBlinkWeight;
	OutFullCurveValues[Offset + 1] = CurrentBlinkWeight;
}
