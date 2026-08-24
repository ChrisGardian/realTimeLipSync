// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RhubarbLipSyncRunner.h"
#include "DynamicSpeechTestActor.generated.h"

class UAudioComponent;
class FRhubarbLiveLinkSource;

// Actor de test Phase 2a : simule la réception d'un chunk audio envoyé par le backend
// (bouton editor chargeant un .wav local en mémoire) pour valider le pipeline côté client
// indépendamment du HTTP. ProcessIncomingAudioChunk est le même point d'entrée qu'utilisera
// plus tard le callback HTTP réel : seule la source des octets change.
UCLASS()
class REALTIMELIPSYNC_API ADynamicSpeechTestActor : public AActor
{
	GENERATED_BODY()

public:
	ADynamicSpeechTestActor();

	// Lit le clip audio construit à l'exécution (USoundWaveProcedural, pas d'asset importé).
	UPROPERTY(VisibleAnywhere, Category = "RhubarbLipSync|Test")
	UAudioComponent* AudioPlayback;

	// Fichier .wav local utilisé pour simuler un chunk reçu du backend (ex: une phrase
	// ElevenLabs déjà générée en Phase 1, réutilisée telle quelle pour ce test).
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Test")
	FFilePath TestWavPath;

	// Nom du LiveLink Subject poussé par cet actor. Doit correspondre à "ARKit Face Subject"
	// sur le Blueprint du MetaHuman à animer. Même valeur par défaut que ARhubarbMetaHumanActor
	// (pas de conflit : les deux tests vivent dans des maps séparées, un seul des deux actors est
	// donc vivant/enregistré comme source à la fois — pas besoin de retoucher le champ sur le
	// MetaHuman en changeant de map de test).
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Test")
	FName LiveLinkSubjectName = TEXT("RhubarbLipSync");

	// Vitesse de lissage (FInterpTo) entre les poids du visème courant et ceux du visème cible.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Test")
	float VisemeInterpSpeed = 20.f;

	// Décalage (en secondes) entre le temps de lecture audio et l'instant utilisé pour échantillonner
	// les visèmes — même rôle que sur ARhubarbMetaHumanActor (calibré à 0.3s là-bas).
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Test")
	float LipSyncDelaySeconds = 0.3f;

	// Charge TestWavPath en mémoire et appelle ProcessIncomingAudioChunk avec ses octets,
	// exactement comme le fera plus tard le callback HTTP avec le corps de la réponse.
	UFUNCTION(CallInEditor, Category = "RhubarbLipSync|Test")
	void SimulateIncomingChunk();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	virtual void Tick(float DeltaTime) override;

private:
	// Point d'entrée unique du pipeline "chunk audio reçu" : construit un clip jouable à partir
	// des octets WAV (USoundWaveProcedural, pas d'asset importé) et le joue, puis écrit les octets
	// en fichier temporaire et lance Rhubarb sur un thread background (process bloquant). Les mouth
	// cues obtenues réarment ElapsedPlaybackTime/MouthCues, lus par Tick() pour piloter le LiveLink.
	void ProcessIncomingAudioChunk(const TArray<uint8>& WavBytes);

	TSharedPtr<FRhubarbLiveLinkSource> LiveLinkSource;
	TArray<FRhubarbMouthCue> MouthCues;
	float ElapsedPlaybackTime = 0.f;
	TArray<float> CurrentCurveValues;
};
