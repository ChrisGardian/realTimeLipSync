// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Implémente le flow HMAC-Guard attendu par le middleware PHP (voir hmac_guard() dans
// backend/public/index.php) : récupération d'un sid + session_secret, puis signature des
// requêtes GET vers les endpoints /api/v1/* qui l'exigent (dont /api/v1/ai/tts).
// Pas un UObject : pas besoin d'exposition Blueprint, juste des fonctions statiques appelées
// depuis du C++ de test/gameplay.
class FMiddlewareAuthClient
{
public:
	// Enchaîne GET /api/v1/session/id puis GET /api/v1/session/secret?sid=...
	// OnComplete(bSuccess, Sid, SecretHex) est appelé sur le game thread, que les requêtes
	// aient réussi ou non (bSuccess=false si l'une des deux échoue ou renvoie un JSON inattendu).
	static void RequestSession(const FString& BackendBaseUrl, TFunction<void(bool bSuccess, FString Sid, FString SecretHex)> OnComplete);

	// Construit l'URL GET signée pour Path (ex: "/api/v1/ai/tts") : ajoute sid/exp/nonce aux
	// QueryParams fournis, calcule sig = HMAC-SHA256(SecretHex, "GET\n{Path}\n{query_canonique}")
	// exactement comme string_to_sign()/hmac_guard() côté PHP (query triée par clé, RFC3986, sig exclu).
	static FString BuildSignedUrl(const FString& BackendBaseUrl, const FString& Path,
		TMap<FString, FString> QueryParams, const FString& Sid, const FString& SecretHex);
};
