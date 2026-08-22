// Copyright Epic Games, Inc. All Rights Reserved.

#include "VisemeToArKitMapping.h"

namespace
{
	// Indices dans UsedCurveNames (même ordre) : évite de rechercher par nom à chaque frame.
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
	};
}

const TArray<FName>& VisemeToArKitMapping::GetUsedCurveNames()
{
	return UsedCurveNames;
}

void VisemeToArKitMapping::GetWeightsForViseme(const FString& Viseme, TArray<float>& OutWeights)
{
	OutWeights.Init(0.f, ECurveIndex::Count);

	// Basé sur les 9 formes de bouche Rhubarb (Preston Blair) : A=bilabiale fermée, B=neutre parlant,
	// C=ouverte moyenne, D=grande ouverture, E=ouverte arrondie, F=pucker, G=dents/lèvre (F/V), H=L (langue).
	if (Viseme == TEXT("A"))
	{
		OutWeights[MouthClose] = 1.f;
	}
	else if (Viseme == TEXT("B"))
	{
		OutWeights[JawOpen] = 0.15f;
		OutWeights[MouthClose] = 0.2f;
	}
	else if (Viseme == TEXT("C"))
	{
		OutWeights[JawOpen] = 0.5f;
		OutWeights[MouthFunnel] = 0.1f;
	}
	else if (Viseme == TEXT("D"))
	{
		OutWeights[JawOpen] = 0.9f;
	}
	else if (Viseme == TEXT("E"))
	{
		OutWeights[JawOpen] = 0.4f;
		OutWeights[MouthFunnel] = 0.4f;
		OutWeights[MouthPucker] = 0.2f;
	}
	else if (Viseme == TEXT("F"))
	{
		OutWeights[MouthPucker] = 0.8f;
		OutWeights[MouthFunnel] = 0.3f;
	}
	else if (Viseme == TEXT("G"))
	{
		OutWeights[MouthRollLower] = 0.4f;
		OutWeights[MouthUpperUpLeft] = 0.3f;
		OutWeights[MouthUpperUpRight] = 0.3f;
		OutWeights[JawOpen] = 0.1f;
	}
	else if (Viseme == TEXT("H"))
	{
		OutWeights[JawOpen] = 0.3f;
		OutWeights[TongueOut] = 0.2f;
	}
	// "X" (idle) et toute valeur inconnue restent à zéro (bouche neutre).
}
