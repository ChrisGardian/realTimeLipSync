// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Maps the 9 Rhubarb mouth shapes (A-X) to weights on a subset of the 52 standard ARKit
// blendshapes, consumed by the MetaHuman through mh_arkit_mapping_pose. A starting point meant
// to be adjusted visually in the editor: there is no "correct" mapping by construction, this is
// artistic calibration.
namespace VisemeToArKitMapping
{
	// Names of the ARKit curves used by the table, in the order expected by GetWeightsForViseme.
	// Use this to declare the LiveLink subject (FRhubarbLiveLinkSource::DeclareSubject).
	const TArray<FName>& GetUsedCurveNames();

	// Fills OutWeights (one weight per curve of GetUsedCurveNames(), same order) for the given
	// Rhubarb viseme. An unknown viseme or "X" (idle) resets every value to zero (neutral mouth).
	void GetWeightsForViseme(const FString& Viseme, TArray<float>& OutWeights);
}
