// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Correspondance entre les 9 formes de bouche Rhubarb (A-X) et des poids sur un sous-ensemble
// des 52 blendshapes standard ARKit, consommées par le MetaHuman via mh_arkit_mapping_pose.
// Point de départ à ajuster visuellement dans l'éditeur (pas de mapping "correct" par construction,
// c'est un calibrage artistique) — voir CLAUDE.md / mémoire projet pour le contexte.
namespace VisemeToArKitMapping
{
	// Liste des noms de curves ARKit utilisées par la table, dans l'ordre attendu par GetWeightsForViseme.
	// À utiliser pour déclarer le subject LiveLink (FRhubarbLiveLinkSource::DeclareSubject).
	const TArray<FName>& GetUsedCurveNames();

	// Remplit OutWeights (un poids par curve de GetUsedCurveNames(), même ordre) pour le visème Rhubarb donné.
	// Visème inconnu ou "X" (idle) -> toutes les valeurs à zéro (bouche neutre).
	void GetWeightsForViseme(const FString& Viseme, TArray<float>& OutWeights);
}
