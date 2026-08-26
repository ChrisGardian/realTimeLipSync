// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RhubarbLipSyncRunner.h"
#include "DynamicSpeechTestActor.generated.h"

class UAudioComponent;
class FRhubarbLiveLinkSource;

// Timestamps (FPlatformTime::Seconds(), horloge monotone) posés à chaque étape du pipeline
// "packet reçu -> animation démarrée", pour mesurer les budgets de latence de calcul (thèse,
// voir AppendLatencySample). RequestSent/ResponseReceived restent à 0 pour SimulateIncomingChunk
// (pas de requête réseau dans ce chemin) : AppendLatencySample laisse la colonne réseau vide.
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

	// Racine du middleware PHP (scheme+host, sans /api/v1) — à adapter à l'endroit où tourne
	// le backend de test (ex: "http://localhost:8080" pour `php -S`, ou l'hôte réel).
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Backend")
	FString BackendBaseUrl = TEXT("http://localhost:8080");

	// Texte envoyé à /api/v1/ai/tts pour ce test.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Backend")
	FString TextToSpeak = TEXT("The mysterious professor whispered quietly at first, then suddenly shouted with joy, waving his umbrella boldly above his head. He had wandered through the foggy old library for hours, mumbling strange words under his breath, before finally discovering a dusty book that promised to reveal the secret that had baffled the entire village for almost thirty years. Overwhelmed with excitement, he rushed outside, laughing and shouting, determined to tell the whole world before another rival scholar could steal the glory.");

	// Si true, le .wav temporaire et son .json Rhubarb ne sont pas effacés après usage (voir
	// ProcessIncomingAudioChunk) : utile pour réutiliser un audio déjà généré (via TestWavPath +
	// SimulateIncomingChunk) sans refaire d'appel ElevenLabs à chaque test/vidéo.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Test")
	bool bKeepTempAudio = false;

	// Récupère une session (sid+secret, mise en cache pour les appels suivants si besoin),
	// signe et envoie GET /api/v1/ai/tts?q=...&fmt=wav, puis alimente ProcessIncomingAudioChunk
	// avec le corps de la réponse — même point d'entrée que SimulateIncomingChunk, seule la
	// source des octets change (réseau au lieu d'un fichier local).
	UFUNCTION(CallInEditor, Category = "RhubarbLipSync|Backend")
	void RequestSpeechFromBackend();

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
	// Trace est complétée au fil du pipeline puis journalisée (voir AppendLatencySample) une fois
	// l'animation démarrée ; Source identifie l'appelant ("Simulate" ou "Backend") dans le CSV.
	void ProcessIncomingAudioChunk(const TArray<uint8>& WavBytes, FLatencyTrace Trace, const FString& Source);

	// Signe et envoie la requête GET /api/v1/ai/tts une fois sid+secret disponibles (CachedSid/
	// CachedSecretHex déjà remplis par RequestSpeechFromBackend).
	void SendSignedTtsRequest();

	// Append une ligne dans Saved/DynamicSpeech/latency_log.csv (un delta en ms par étape du
	// pipeline, colonne réseau vide si Trace.RequestSent/ResponseReceived valent 0). Écrit l'en-tête
	// si le fichier n'existe pas encore.
	void AppendLatencySample(const FLatencyTrace& Trace, const FString& Source);

	TSharedPtr<FRhubarbLiveLinkSource> LiveLinkSource;
	TArray<FRhubarbMouthCue> MouthCues;
	float ElapsedPlaybackTime = 0.f;
	TArray<float> CurrentCurveValues;

	// Session HMAC mise en cache après le premier appel réussi à RequestSession, pour éviter de
	// refaire /session/id + /session/secret à chaque clic du bouton de test.
	FString CachedSid;
	FString CachedSecretHex;
	bool bHasSession = false;
};
