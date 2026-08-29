// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RhubarbFaceActorBase.h"
#include "DemoScenarioActor.generated.h"

class USoundWave;

// Actor de démo "produit pseudo-fini" (voir TODO.md, section du même nom) : donne l'impression
// d'un client qui parle avec un vrai backend plutôt qu'un simple test technique isolé. Deux
// entrées :
//   - PlayIntro() : une phrase d'intro fixe issue d'un vrai scénario du backend (content/
//     scenarios/0003/scenes/01-intro.json), générée UNE SEULE FOIS via /api/v1/ai/tts puis importée
//     en asset (SoundToPlay) -- pas d'appel réseau à chaque Play, exactement le même pattern que
//     ARhubarbMetaHumanActor (Phase 1) : chemin disque résolu via AssetImportData (editor-only),
//     Rhubarb tourne en bloquant (pas d'async nécessaire, c'est un fichier local). Voir TODO.md
//     pour la procédure de génération/import.
//   - AskQuestion() : une question libre envoyée à ChatGPT (/api/v1/ai/ask) ; la réponse est
//     forcément dynamique (texte différent à chaque fois), donc repasse par le pipeline réseau
//     complet (ai/tts -> ProcessIncomingAudioChunk async, hérité d'ARhubarbFaceActorBase).
// Le scénario est fixe et codé en dur pour l'instant : pas d'appel à /api/v1/scenario/start ou
// /scenario/scene -- inutile d'aller chercher dynamiquement un contenu qui ne change pas encore
// (voir décision dans TODO.md si un jour il faut l'étendre).
//
// Seul AskQuestion journalise dans Saved/DynamicSpeech/latency_log.csv (colonne "source" =
// "DemoAsk") : PlayIntro ne passe plus par ProcessIncomingAudioChunk depuis qu'il joue un asset
// local, donc plus rien à mesurer côté réseau/pipeline pour lui.
UCLASS()
class REALTIMELIPSYNC_API ADemoScenarioActor : public ARhubarbFaceActorBase
{
	GENERATED_BODY()

public:
	ADemoScenarioActor();

	// Racine du middleware PHP (scheme+host, sans /api/v1) -- même signification que sur
	// ADynamicSpeechTestActor.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Backend")
	FString BackendBaseUrl = TEXT("http://localhost:8080");

	// Audio d'intro pré-généré et importé une fois pour toutes (voir TODO.md pour la procédure :
	// générer via PlayIntro/ai-tts avec bKeepTempAudio=true, puis glisser le .wav gardé dans
	// Saved/DynamicSpeech/ dans le Content Browser). Son chemin disque d'origine est retrouvé
	// automatiquement via ses métadonnées d'import, comme ARhubarbMetaHumanActor::SoundToPlay.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Demo")
	USoundWave* SoundToPlay = nullptr;

	// Lance Rhubarb sur SoundToPlay (bloquant, fichier local) et le joue -- aucun appel réseau.
	UFUNCTION(CallInEditor, Category = "RhubarbLipSync|Demo")
	void PlayIntro();

	// Question libre envoyée à ChatGPT.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Demo")
	FString QuestionText = TEXT("Haben Sie das Gefühl, dass es Ihnen inzwischen besser geht?");

	// Contexte optionnel envoyé avec la question (paramètre ctx= de /api/v1/ai/ask). Peut rester
	// vide ; pourra reprendre le "freeContext" d'une scène de scénario si besoin plus tard.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Demo")
	FString QuestionContext;

	// Envoie QuestionText à /api/v1/ai/ask, puis fait parler l'avatar avec la réponse reçue --
	// même chemin TTS que PlayIntro, une fois le texte de réponse en main.
	UFUNCTION(CallInEditor, Category = "RhubarbLipSync|Demo")
	void AskQuestion();

private:
	// Si CachedSid/CachedSecretHex sont déjà valides, exécute OnReady tout de suite ; sinon
	// récupère d'abord une session (GET /session/id puis /session/secret, voir
	// FMiddlewareAuthClient::RequestSession) et la met en cache. Utilisé par AskQuestion (PlayIntro
	// n'a plus besoin de session depuis qu'il ne fait plus d'appel réseau).
	void EnsureSession(TFunction<void()> OnReady);

	// Signe et envoie GET /api/v1/ai/tts?q=<Text>&fmt=wav, mesure le round-trip réseau (Trace.
	// RequestSent/ResponseReceived), puis passe la réponse (octets WAV) à ProcessIncomingAudioChunk
	// (héritée d'ARhubarbFaceActorBase). Source vaut toujours "DemoAsk" pour l'instant (seul appelant
	// restant est AskQuestion, une fois la réponse ChatGPT en main) ; gardé en paramètre au cas où
	// un futur appelant réseau s'ajoute.
	void SendSignedTtsRequest(const FString& TextToSpeak, const FString& Source);

	// Signe et envoie GET /api/v1/ai/ask?q=...&ctx=..., puis appelle OnComplete sur le game thread
	// avec le champ "answer" de la réponse JSON (voir /api/v1/ai/ask côté PHP, index.php).
	// OnComplete(false, "") si la requête échoue ou si le JSON ne contient pas "answer". Le
	// round-trip ChatGPT n'est pas journalisé dans latency_log.csv (schéma à une seule colonne
	// réseau, déjà utilisée par le round-trip ai/tts qui suit) -- juste loggé en Log/Error.
	void SendSignedAskRequest(const FString& Question, const FString& Context, TFunction<void(bool bSuccess, FString Answer)> OnComplete);

	FString CachedSid;
	FString CachedSecretHex;
	bool bHasSession = false;
};
