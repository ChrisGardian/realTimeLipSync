// Copyright Epic Games, Inc. All Rights Reserved.

#include "VisemeToArKitMapping.h"

namespace
{
	// Indices into UsedCurveNames (same order), avoiding a name lookup every frame.
	enum ECurveIndex : int32
	{
		JawOpen,
		MouthClose,
		MouthFunnel,
		MouthPucker,
		MouthRollLower,
		MouthUpperUpLeft,
		MouthUpperUpRight,
		TongueOut,
		MouthLowerDownLeft,
		MouthLowerDownRight,
		MouthShrugLower,
		MouthShrugUpper,
		CheekPuff,
		MouthPressLeft,
		MouthPressRight,
		Count
	};

	const TArray<FName> UsedCurveNames = {
		TEXT("JawOpen"),
		TEXT("MouthClose"),
		TEXT("MouthFunnel"),
		TEXT("MouthPucker"),
		TEXT("MouthRollLower"),
		TEXT("MouthUpperUpLeft"),
		TEXT("MouthUpperUpRight"),
		TEXT("TongueOut"),
		TEXT("MouthLowerDownLeft"),
		TEXT("MouthLowerDownRight"),
		TEXT("MouthShrugLower"),
		TEXT("MouthShrugUpper"),
		TEXT("CheekPuff"),
		TEXT("MouthPressLeft"),
		TEXT("MouthPressRight"),
	};
}

const TArray<FName>& VisemeToArKitMapping::GetUsedCurveNames()
{
	return UsedCurveNames;
}

void VisemeToArKitMapping::GetWeightsForViseme(const FString& Viseme, TArray<float>& OutWeights)
{
	OutWeights.Init(0.f, ECurveIndex::Count);

	// Based on the 9 Rhubarb (Preston Blair) mouth shapes: A=closed bilabial, B=neutral speaking,
	// C=medium open, D=wide open, E=rounded open, F=pucker, G=teeth/lip (F/V), H=L (tongue).
	if (Viseme == TEXT("A"))
	{
		OutWeights[MouthClose] = 1.0f;
		OutWeights[MouthRollLower] = 0.7f;
		OutWeights[MouthShrugLower] = 0.1f;
		OutWeights[MouthPressLeft] = 0.15f;
		OutWeights[MouthPressRight] = 0.15f;
	}
	else if (Viseme == TEXT("B"))
	{
		OutWeights[JawOpen] = 0.1f;
		OutWeights[MouthUpperUpLeft] = 0.2f;
		OutWeights[MouthUpperUpRight] = 0.2f;
		OutWeights[MouthLowerDownLeft] = 0.5f;
		OutWeights[MouthLowerDownRight] = 0.5f;
	}
	else if (Viseme == TEXT("C"))
	{
		OutWeights[JawOpen] = 0.1f;
		OutWeights[MouthRollLower] = 0.5f;
		OutWeights[MouthUpperUpLeft] = 0.4f;
		OutWeights[MouthUpperUpRight] = 0.4f;
		OutWeights[MouthLowerDownRight] = 0.32f;
		OutWeights[MouthLowerDownLeft] = 0.32f;
		OutWeights[MouthShrugUpper] = 0.22f;
	}
	else if (Viseme == TEXT("D"))
	{
		OutWeights[JawOpen] = 0.2f;
		OutWeights[MouthRollLower] = 0.5f;
		OutWeights[MouthUpperUpLeft] = 0.3f;
		OutWeights[MouthUpperUpRight] = 0.3f;
		OutWeights[MouthLowerDownRight] = 0.25f;
		OutWeights[MouthLowerDownLeft] = 0.25f;
		OutWeights[MouthShrugUpper] = 0.23f;
	}
	else if (Viseme == TEXT("E"))
	{
		OutWeights[JawOpen] = 0.33f;
		OutWeights[MouthFunnel] = 0.2f;
		OutWeights[MouthPucker] = 0.4f;
		OutWeights[MouthRollLower] = 0.5f;
		OutWeights[MouthLowerDownLeft] = 0.15f;
		OutWeights[MouthLowerDownRight] = 0.15f;
		OutWeights[MouthShrugUpper] = 0.15f;
		OutWeights[CheekPuff] = 0.05f;
	}
	else if (Viseme == TEXT("F"))
	{
		OutWeights[JawOpen] = 0.15f;
		OutWeights[MouthFunnel] = 0.42f;
		OutWeights[MouthPucker] = 0.42f;
		OutWeights[MouthRollLower] = 0.75f;
		OutWeights[MouthLowerDownLeft] = 0.135f;
		OutWeights[MouthLowerDownRight] = 0.135f;
		OutWeights[MouthShrugUpper] = 0.22f;
		OutWeights[CheekPuff] = 0.05f;
	}
	else if (Viseme == TEXT("G"))
	{
		OutWeights[JawOpen] = 0.1f;
		OutWeights[MouthRollLower] = 1.0f;
		OutWeights[MouthUpperUpLeft] = 0.3f;
		OutWeights[MouthUpperUpRight] = 0.3f;
		OutWeights[MouthShrugLower] = 0.24f;
		OutWeights[MouthShrugUpper] = 0.13f;
	}
	else if (Viseme == TEXT("H"))
	{
		OutWeights[JawOpen] = 0.3f;
		OutWeights[MouthFunnel] = 0.35f;
		OutWeights[MouthPucker] = 0.4f;
		OutWeights[MouthRollLower] = 0.5f;
		OutWeights[MouthUpperUpLeft] = 0.32f;
		OutWeights[MouthUpperUpRight] = 0.32f;
		OutWeights[TongueOut] = 0.1f;
	}
	else if (Viseme == TEXT("X"))
	{
		OutWeights[JawOpen] = 0.14f;
		OutWeights[MouthClose] = 0.24f;
		OutWeights[MouthRollLower] = 0.37f;
	}
}
