// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Implements the HMAC-guard flow expected by the PHP middleware (see hmac_guard() on the backend
// side): fetches a sid plus session_secret, then signs GET requests to the /api/v1/* endpoints
// that require it (including /api/v1/ai/tts). Not a UObject: no need for Blueprint exposure, just
// static functions called from test/gameplay C++.
class FMiddlewareAuthClient
{
public:
	// Chains GET /api/v1/session/id then GET /api/v1/session/secret?sid=...
	// OnComplete(bSuccess, Sid, SecretHex) is called on the game thread whether the requests
	// succeeded or not (bSuccess=false if either fails or returns unexpected JSON).
	static void RequestSession(const FString& BackendBaseUrl, TFunction<void(bool bSuccess, FString Sid, FString SecretHex)> OnComplete);

	// Builds the signed GET URL for Path (e.g. "/api/v1/ai/tts"): adds sid/exp/nonce to the given
	// QueryParams, computes sig = HMAC-SHA256(SecretHex, "GET\n{Path}\n{canonical_query}") exactly
	// like string_to_sign()/hmac_guard() on the backend (query sorted by key, RFC3986, sig excluded).
	static FString BuildSignedUrl(const FString& BackendBaseUrl, const FString& Path,
		TMap<FString, FString> QueryParams, const FString& Sid, const FString& SecretHex);
};
