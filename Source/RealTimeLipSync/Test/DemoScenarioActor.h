// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RhubarbFaceActorBase.h"
#include "DemoScenarioActor.generated.h"

class USoundWave;

// "Near-finished product" demo actor: gives the
// impression of a client talking to a real backend rather than an isolated technical test.
// Two entry points:
//   - PlayIntro(): a fixed intro line taken from a real backend scenario (content/scenarios/
//     0003/scenes/01-intro.json), generated ONCE via /api/v1/ai/tts and then imported as an
//     asset (SoundToPlay), so no network call on every Play. Same pattern as
//     ARhubarbMetaHumanActor (Phase 1): disk path resolved via AssetImportData (editor-only),
//     Rhubarb runs blocking since it is a local file.
//   - AskQuestion(): a free-form question sent to ChatGPT (/api/v1/ai/ask). The reply is
//     necessarily dynamic (different text every time), so it goes through the full network
//     pipeline (ai/tts to async ProcessIncomingAudioChunk, inherited from ARhubarbFaceActorBase).
// The scenario is fixed and hardcoded for now: no call to /api/v1/scenario/start or
// /scenario/scene, since there is no point dynamically fetching content that does not change yet.
//
// Only AskQuestion logs to Saved/DynamicSpeech/latency_log.csv ("source" = "DemoAsk"). PlayIntro
// no longer goes through ProcessIncomingAudioChunk now that it plays a local asset, so there is
// nothing left to measure on its network/pipeline side.
UCLASS()
class REALTIMELIPSYNC_API ADemoScenarioActor : public ARhubarbFaceActorBase
{
	GENERATED_BODY()

public:
	ADemoScenarioActor();

	// PHP middleware root (scheme+host, no /api/v1). Same meaning as on ADynamicSpeechTestActor.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Backend")
	FString BackendBaseUrl = TEXT("http://localhost:8080");

	// Intro audio pre-generated and imported once and for all (generated via PlayIntro/ai-tts with
	// bKeepTempAudio=true, then the kept .wav dragged from Saved/DynamicSpeech/ into the Content
	// Browser). Its original disk path is resolved automatically from its import metadata, like
	// ARhubarbMetaHumanActor::SoundToPlay.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Demo")
	USoundWave* SoundToPlay = nullptr;

	// Runs Rhubarb on SoundToPlay (blocking, local file) and plays it. No network call.
	UFUNCTION(CallInEditor, Category = "RhubarbLipSync|Demo")
	void PlayIntro();

	// Free-form question sent to ChatGPT.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Demo")
	FString QuestionText = TEXT("Haben Sie das Gefühl, dass es Ihnen inzwischen besser geht?");

	// Optional context sent with the question (ctx= parameter of /api/v1/ai/ask). Can stay empty;
	// may later reuse a scenario scene's "freeContext" if needed.
	UPROPERTY(EditAnywhere, Category = "RhubarbLipSync|Demo")
	FString QuestionContext;

	// Sends QuestionText to /api/v1/ai/ask, then makes the avatar speak the reply. Same TTS path
	// as PlayIntro, once the reply text is available.
	UFUNCTION(CallInEditor, Category = "RhubarbLipSync|Demo")
	void AskQuestion();

private:
	// If CachedSid/CachedSecretHex are already valid, runs OnReady right away; otherwise fetches
	// a session first (GET /session/id then /session/secret, see
	// FMiddlewareAuthClient::RequestSession) and caches it. Used by AskQuestion (PlayIntro no
	// longer needs a session now that it makes no network call).
	void EnsureSession(TFunction<void()> OnReady);

	// Signs and sends GET /api/v1/ai/tts?q=<Text>&fmt=wav, measures the network round trip
	// (Trace.RequestSent/ResponseReceived), then passes the response (WAV bytes) to
	// ProcessIncomingAudioChunk (inherited from ARhubarbFaceActorBase). Source is currently always
	// "DemoAsk" (the only remaining caller is AskQuestion, once the ChatGPT reply is available);
	// kept as a parameter in case a future network caller is added.
	void SendSignedTtsRequest(const FString& TextToSpeak, const FString& Source);

	// Signs and sends GET /api/v1/ai/ask?q=...&ctx=..., then calls OnComplete on the game thread
	// with the "answer" field of the JSON response (see /api/v1/ai/ask on the PHP side,
	// index.php). Calls OnComplete(false, "") if the request fails or the JSON has no "answer".
	// The ChatGPT round trip is not logged to latency_log.csv (the single network column is
	// already used by the ai/tts round trip that follows); it is only logged via Log/Error.
	void SendSignedAskRequest(const FString& Question, const FString& Context, TFunction<void(bool bSuccess, FString Answer)> OnComplete);

	FString CachedSid;
	FString CachedSecretHex;
	bool bHasSession = false;
};
