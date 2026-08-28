// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Paramètres calibrables d'une instance de FIdleFaceAnimator, exposés en UPROPERTY sur l'actor
// propriétaire (Details panel) et passés à Update() à chaque tick plutôt que dupliqués comme état interne.
struct FIdleFaceAnimationSettings
{
	float MinBlinkInterval = 2.5f;
	float MaxBlinkInterval = 4.5f;
	float BlinkDuration = 0.2f;
};

// Regroupe les micro-mouvements "idle animation" pilotés par curves ARKit (clignement des yeux pour
// l'instant, regard/sourcils envisagés plus tard) : timers et courbes de progression, pour éviter de
// dupliquer cette logique entre ARhubarbMetaHumanActor et ADynamicSpeechTestActor. Le mouvement de tête
// (rotation d'os, pas une curve ARKit) n'entre pas dans cette classe tant qu'il n'est pas implémenté.
class FIdleFaceAnimator
{
public:
	// Noms des curves ARKit poussées par cette classe, à ajouter à la fin de la liste de curves de
	// l'actor propriétaire avant de déclarer le subject LiveLink (voir WriteCurveValues).
	static const TArray<FName>& GetCurveNames();

	// Fait avancer l'état du clignement d'un DeltaTime.
	void Update(float DeltaTime, const FIdleFaceAnimationSettings& Settings);

	// Écrit les valeurs courantes dans les derniers éléments de OutFullCurveValues (même taille que
	// GetCurveNames(), en supposant que ces curves ont été ajoutées à la fin de la liste déclarée).
	void WriteCurveValues(TArray<float>& OutFullCurveValues) const;

private:
	float TimeUntilNextBlink = 0.f;
	float BlinkElapsedTime = 0.f;
	float CurrentBlinkWeight = 0.f;
	bool bIsBlinking = false;
	bool bInitialized = false;
};
