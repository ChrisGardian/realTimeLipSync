// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IdleFaceAnimator.h"
#include "RhubarbLipSyncRunner.h"
#include "RhubarbFaceActorBase.generated.h"

class UAudioComponent;
class FRhubarbLiveLinkSource;

// Base commune aux actors qui pilotent un visage MetaHuman via curves ARKit poussées par un
// ILiveLinkSource custom (visèmes Rhubarb + idle animation) : ARhubarbMetaHumanActor (Phase 1,
// audio pré-enregistré) et ADynamicSpeechTestActor (Phase 2, audio reçu du backend). Regroupe le
// setup LiveLink, l'échantillonnage des mouth cues, le lissage FInterpTo et le blink — tout ce qui
// ne dépend pas de la source de l'audio (chaque sous-classe reste responsable de remplir MouthCues
// et de lancer AudioPlayback->Play()).
UCLASS(Abstract)
class REALTIMELIPSYNC_API ARhubarbFaceActorBase : public AActor
{
	GENERATED_BODY()

public:
	ARhubarbFaceActorBase();

	// Lit le son en même temps que l'animation faciale.
	UPROPERTY(VisibleAnywhere, Category = "RhubarbLipSync")
	UAudioComponent* AudioPlayback;

	// Nom du LiveLink Subject poussé par cet actor. Doit correspondre à "ARKit Face Subject"
	// sur le Blueprint du MetaHuman.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync")
	FName LiveLinkSubjectName = TEXT("RhubarbLipSync");

	// Vitesse de lissage (FInterpTo) entre les poids du visème courant et ceux du visème cible.
	// Plus haut = transition plus rapide/plus proche du "pop" d'origine ; plus bas = plus lisse.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync")
	float VisemeInterpSpeed = 20.f;

	// Décalage (en secondes) entre le temps de lecture audio et l'instant utilisé pour échantillonner
	// les visèmes. Positif = la bouche bouge plus tard que le son (compense une latence LiveLink/AnimBP
	// en aval) ; négatif = la bouche bouge plus tôt. A régler empiriquement en Play.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync")
	float LipSyncDelaySeconds = 0.f;

	// Idle animation (voir TODO.md, retours qualité 2026-08-28 ; logique dans FIdleFaceAnimator) :
	// intervalle aléatoire entre deux clignements, en secondes. Repère de calibrage : un humain
	// cligne environ toutes les 2s, de façon irrégulière -> intervalle tiré au hasard entre ces
	// deux bornes à chaque clignement.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Blink")
	float MinBlinkInterval = 2.5f;

	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Blink")
	float MaxBlinkInterval = 4.5f;

	// Durée totale d'un clignement (fermeture + réouverture). Courbe triangle linéaire (0->1->0),
	// pas d'easing pour ce premier essai.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Blink")
	float BlinkDuration = 0.2f;

protected:
	// Crée la source LiveLink et déclare le subject (curves bouche + idle animation). Les sous-classes
	// qui ont besoin de charger un audio au BeginPlay appellent Super::BeginPlay() puis leur propre logique.
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Échantillonne MouthCues au temps courant, lisse vers les poids ARKit cibles, fait avancer le
	// blink, et pousse la frame de curves résultante. Ne fait rien si MouthCues est vide (avant que
	// la sous-classe n'ait chargé un audio) — voir ARhubarbMetaHumanActor pour un exemple de sous-classe
	// qui court-circuite ce comportement (mode debug).
	virtual void Tick(float DeltaTime) override;

	TSharedPtr<FRhubarbLiveLinkSource> LiveLinkSource;
	TArray<FRhubarbMouthCue> MouthCues;
	float ElapsedPlaybackTime = 0.f;
	TArray<float> CurrentCurveValues;

	// Clignement des yeux ; d'autres micro-mouvements idle curve-based (regard, sourcils)
	// rejoindront cette même instance plus tard (voir IdleFaceAnimator.h).
	FIdleFaceAnimator IdleFaceAnimator;
};
