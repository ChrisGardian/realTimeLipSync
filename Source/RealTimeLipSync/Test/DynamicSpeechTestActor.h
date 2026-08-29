// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RhubarbFaceActorBase.h"
#include "DynamicSpeechTestActor.generated.h"

// Actor de test Phase 2a : simule la réception d'un chunk audio envoyé par le backend
// (bouton editor chargeant un .wav local en mémoire) pour valider le pipeline côté client
// indépendamment du HTTP. ProcessIncomingAudioChunk est le même point d'entrée qu'utilisera
// plus tard le callback HTTP réel : seule la source des octets change. Le pilotage LiveLink
// (curves bouche + idle animation, BeginPlay/EndPlay/Tick) vient entièrement de
// ARhubarbFaceActorBase, pas besoin de le redéfinir ici.
UCLASS()
class REALTIMELIPSYNC_API ADynamicSpeechTestActor : public ARhubarbFaceActorBase
{
	GENERATED_BODY()

public:
	// Redéfinit juste LipSyncDelaySeconds (calibré séparément à 0.3s ici, voir ARhubarbFaceActorBase
	// pour la valeur par défaut 0.f) — tout le reste de la construction vient de la classe de base.
	ADynamicSpeechTestActor();

	// Fichier .wav local utilisé pour simuler un chunk reçu du backend (ex: une phrase
	// ElevenLabs déjà générée en Phase 1, réutilisée telle quelle pour ce test).
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Test")
	FFilePath TestWavPath;

	// Charge TestWavPath en mémoire et appelle ProcessIncomingAudioChunk avec ses octets,
	// exactement comme le fera plus tard le callback HTTP avec le corps de la réponse.
	UFUNCTION(CallInEditor, Category = "RhubarbLipSync|Test")
	void SimulateIncomingChunk();

	// Racine du middleware PHP (scheme+host, sans /api/v1) — à adapter à l'endroit où tourne
	// le backend de test (ex: "http://localhost:8080" pour `php -S`, ou l'hôte réel).
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Backend")
	FString BackendBaseUrl = TEXT("http://localhost:8080");

	// Texte envoyé à /api/v1/ai/tts pour ce test.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Backend")
	FString TextToSpeak = TEXT("The mysterious professor whispered quietly at first, then suddenly shouted with joy, waving his umbrella boldly above his head. He had wandered through the foggy old library for hours, mumbling strange words under his breath, before finally discovering a dusty book that promised to reveal the secret that had baffled the entire village for almost thirty years. Overwhelmed with excitement, he rushed outside, laughing and shouting, determined to tell the whole world before another rival scholar could steal the glory.");

	// Récupère une session (sid+secret, mise en cache pour les appels suivants si besoin),
	// signe et envoie GET /api/v1/ai/tts?q=...&fmt=wav, puis alimente ProcessIncomingAudioChunk
	// avec le corps de la réponse — même point d'entrée que SimulateIncomingChunk, seule la
	// source des octets change (réseau au lieu d'un fichier local).
	UFUNCTION(CallInEditor, Category = "RhubarbLipSync|Backend")
	void RequestSpeechFromBackend();

private:
	// Signe et envoie la requête GET /api/v1/ai/tts une fois sid+secret disponibles (CachedSid/
	// CachedSecretHex déjà remplis par RequestSpeechFromBackend), puis passe la réponse à
	// ProcessIncomingAudioChunk (héritée d'ARhubarbFaceActorBase, voir RhubarbFaceActorBase.h).
	void SendSignedTtsRequest();

	// Session HMAC mise en cache après le premier appel réussi à RequestSession, pour éviter de
	// refaire /session/id + /session/secret à chaque clic du bouton de test.
	FString CachedSid;
	FString CachedSecretHex;
	bool bHasSession = false;
};
