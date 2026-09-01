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

// Timestamps (FPlatformTime::Seconds(), horloge monotone) posés à chaque étape du pipeline "packet
// reçu -> animation démarrée", pour mesurer les budgets de latence de calcul (thèse, voir
// ARhubarbFaceActorBase::AppendLatencySample). RequestSent/ResponseReceived restent à 0 quand
// l'appelant n'a pas fait de requête réseau (ex: ADynamicSpeechTestActor::SimulateIncomingChunk) :
// AppendLatencySample laisse alors la colonne réseau vide plutôt qu'à 0.
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

// Base commune aux actors qui pilotent un visage MetaHuman via curves ARKit poussées par un
// ILiveLinkSource custom (visèmes Rhubarb + idle animation) : ARhubarbMetaHumanActor (Phase 1,
// audio pré-enregistré), ADynamicSpeechTestActor (Phase 2, audio reçu du backend) et
// ADemoScenarioActor (démo "produit pseudo-fini"). Regroupe le setup LiveLink, l'échantillonnage
// des mouth cues, le lissage FInterpTo, le blink, et le pipeline "octets WAV reçus -> clip jouable
// -> Rhubarb async -> Play" avec sa mesure de latence (voir ProcessIncomingAudioChunk plus bas) —
// tout ce qui ne dépend pas de la manière dont l'audio a été obtenu (chaque sous-classe reste
// responsable d'obtenir les octets WAV, ex: fichier local vs requête HTTP signée).
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

	// Si true, le .wav temporaire et son .json Rhubarb écrits par ProcessIncomingAudioChunk ne sont
	// pas effacés après usage : utile pour réutiliser un audio déjà généré sans refaire d'appel
	// ElevenLabs à chaque test/vidéo (voir ADynamicSpeechTestActor::TestWavPath).
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync")
	bool bKeepTempAudio = false;

	// Actor MetaHuman placé dans le niveau (BP_Ada/BP_Taro) dont on veut animer le corps. Cet actor
	// C++ ne contient pas lui-même le mesh -- il ne fait que pousser des curves LiveLink vers le
	// MetaHuman -- donc la référence se fait par assignation dans le Details panel du niveau.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Body")
	AActor* BodyActor = nullptr;

	// Idle animation légère (Mixamo retargetée) jouée en boucle sur le component "Body" de BodyActor
	// dès BeginPlay, comme le blink -- pas de logique de génération, juste un clip qui tourne.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Body")
	UAnimSequence* IdleBodyAnimation = nullptr;

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

	// Point d'entrée unique du pipeline "octets WAV reçus -> clip jouable -> Rhubarb async -> Play",
	// partagé par toutes les sous-classes qui reçoivent de l'audio à traiter (peu importe la source :
	// fichier local, réponse HTTP de /api/v1/ai/tts, etc.) : construit un USoundWaveProcedural sans
	// passer par un asset importé, écrit un fichier temporaire, lance Rhubarb dessus sur un thread
	// background (process bloquant, donc hors game thread), puis arme MouthCues et démarre Play()
	// une fois les mouth cues prêtes — les deux ne démarrent qu'ensemble pour garantir la synchro
	// audio/visèmes. Trace est complétée au fil du pipeline puis journalisée (AppendLatencySample)
	// une fois l'animation démarrée ; Source identifie l'appelant dans le CSV (ex: "Simulate",
	// "Backend", "DemoIntro", "DemoAsk").
	void ProcessIncomingAudioChunk(const TArray<uint8>& WavBytes, FLatencyTrace Trace, const FString& Source);

	// Append une ligne dans Saved/DynamicSpeech/latency_log.csv (un delta en ms par étape du pipeline,
	// colonne réseau vide si Trace.RequestSent/ResponseReceived valent 0). Écrit l'en-tête si le
	// fichier n'existe pas encore. Partagé pour que toutes les mesures (tests bruts et démo) vivent
	// dans le même fichier, Source servant à les distinguer en analyse.
	void AppendLatencySample(const FLatencyTrace& Trace, const FString& Source);

	TSharedPtr<FRhubarbLiveLinkSource> LiveLinkSource;
	TArray<FRhubarbMouthCue> MouthCues;
	float ElapsedPlaybackTime = 0.f;
	TArray<float> CurrentCurveValues;

	// Clignement des yeux ; d'autres micro-mouvements idle curve-based (regard, sourcils)
	// rejoindront cette même instance plus tard (voir IdleFaceAnimator.h).
	FIdleFaceAnimator IdleFaceAnimator;
};
