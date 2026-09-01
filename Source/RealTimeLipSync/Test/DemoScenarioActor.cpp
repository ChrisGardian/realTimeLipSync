// Copyright Epic Games, Inc. All Rights Reserved.

#include "DemoScenarioActor.h"

#include "Components/AudioComponent.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "MiddlewareAuthClient.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sound/SoundWave.h"

#if WITH_EDITORONLY_DATA
#include "EditorFramework/AssetImportData.h"
#endif

ADemoScenarioActor::ADemoScenarioActor()
{
	// Same value as ADynamicSpeechTestActor: calibrated for the same TTS to Rhubarb to LiveLink
	// pipeline, so the same downstream LiveLink/AnimBP latency applies here.
	LipSyncDelaySeconds = 0.3f;
}

void ADemoScenarioActor::PlayIntro()
{
	// Same pattern as ARhubarbMetaHumanActor::BeginPlay (Phase 1): SoundToPlay is an asset imported
	// once and for all, so its original disk path (needed by Rhubarb) is resolved from its import
	// metadata instead of repeating a network call every time.
	if (!SoundToPlay)
	{
		UE_LOG(LogTemp, Warning, TEXT("DemoScenarioActor: SoundToPlay is not set"));
		return;
	}

	FString AudioFilePath;
#if WITH_EDITORONLY_DATA
	if (SoundToPlay->AssetImportData)
	{
		AudioFilePath = SoundToPlay->AssetImportData->GetFirstFilename();
	}
#endif

	if (AudioFilePath.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("DemoScenarioActor: could not resolve source file path for %s (editor-only data)"), *SoundToPlay->GetName());
		return;
	}

	// Local file: Rhubarb can run blocking directly on the game thread, no need for the AsyncTask
	// that ProcessIncomingAudioChunk uses for network responses.
	URhubarbLipSyncRunner* Runner = NewObject<URhubarbLipSyncRunner>(this);
	if (!Runner->RunOnAudioFile(AudioFilePath, RhubarbExecutablePath, MouthCues))
	{
		UE_LOG(LogTemp, Warning, TEXT("DemoScenarioActor: RunOnAudioFile failed for %s"), *AudioFilePath);
		return;
	}

	ElapsedPlaybackTime = 0.f;
	AudioPlayback->SetSound(SoundToPlay);
	AudioPlayback->Play();
}

void ADemoScenarioActor::AskQuestion()
{
	TWeakObjectPtr<ADemoScenarioActor> WeakThis(this);
	EnsureSession([WeakThis]()
	{
		ADemoScenarioActor* Actor = WeakThis.Get();
		if (!Actor)
		{
			return;
		}

		Actor->SendSignedAskRequest(Actor->QuestionText, Actor->QuestionContext,
			[WeakThis](bool bSuccess, FString Answer)
		{
			ADemoScenarioActor* InnerActor = WeakThis.Get();
			if (!InnerActor || !bSuccess)
			{
				return;
			}

			InnerActor->SendSignedTtsRequest(Answer, TEXT("DemoAsk"));
		});
	});
}

void ADemoScenarioActor::EnsureSession(TFunction<void()> OnReady)
{
	if (bHasSession)
	{
		OnReady();
		return;
	}

	TWeakObjectPtr<ADemoScenarioActor> WeakThis(this);
	FMiddlewareAuthClient::RequestSession(BackendBaseUrl, [WeakThis, OnReady](bool bSuccess, FString Sid, FString SecretHex)
	{
		ADemoScenarioActor* Actor = WeakThis.Get();
		if (!Actor)
		{
			return;
		}

		if (!bSuccess)
		{
			UE_LOG(LogTemp, Error, TEXT("DemoScenarioActor: could not obtain a backend session (sid/secret)"));
			return;
		}

		Actor->CachedSid = Sid;
		Actor->CachedSecretHex = SecretHex;
		Actor->bHasSession = true;
		OnReady();
	});
}

void ADemoScenarioActor::SendSignedTtsRequest(const FString& TextToSpeak, const FString& Source)
{
	TMap<FString, FString> QueryParams;
	QueryParams.Add(TEXT("q"), TextToSpeak);
	QueryParams.Add(TEXT("fmt"), TEXT("wav"));

	const FString SignedUrl = FMiddlewareAuthClient::BuildSignedUrl(
		BackendBaseUrl, TEXT("/api/v1/ai/tts"), QueryParams, CachedSid, CachedSecretHex);

	const TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(SignedUrl);
	Request->SetVerb(TEXT("GET"));

	TWeakObjectPtr<ADemoScenarioActor> WeakThis(this);
	const double RequestSentTime = FPlatformTime::Seconds();
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, RequestSentTime, Source](FHttpRequestPtr, const FHttpResponsePtr& Response, bool bSuccess)
	{
		const double ResponseReceivedTime = FPlatformTime::Seconds();

		ADemoScenarioActor* Actor = WeakThis.Get();
		if (!Actor)
		{
			return;
		}

		if (!bSuccess || !Response.IsValid() || Response->GetResponseCode() != 200)
		{
			UE_LOG(LogTemp, Error, TEXT("DemoScenarioActor: /api/v1/ai/tts failed (code %d): %s"),
				Response.IsValid() ? Response->GetResponseCode() : -1,
				Response.IsValid() ? *Response->GetContentAsString() : TEXT(""));
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("DemoScenarioActor: received %d bytes from /api/v1/ai/tts"), Response->GetContent().Num());

		FLatencyTrace Trace;
		Trace.RequestSent = RequestSentTime;
		Trace.ResponseReceived = ResponseReceivedTime;
		Actor->ProcessIncomingAudioChunk(Response->GetContent(), Trace, Source);
	});
	Request->ProcessRequest();
}

void ADemoScenarioActor::SendSignedAskRequest(const FString& Question, const FString& Context,
	TFunction<void(bool bSuccess, FString Answer)> OnComplete)
{
	TMap<FString, FString> QueryParams;
	QueryParams.Add(TEXT("q"), Question);
	QueryParams.Add(TEXT("ctx"), Context);

	const FString SignedUrl = FMiddlewareAuthClient::BuildSignedUrl(
		BackendBaseUrl, TEXT("/api/v1/ai/ask"), QueryParams, CachedSid, CachedSecretHex);

	const TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(SignedUrl);
	Request->SetVerb(TEXT("GET"));

	Request->OnProcessRequestComplete().BindLambda(
		[OnComplete](FHttpRequestPtr, const FHttpResponsePtr& Response, bool bSuccess)
	{
		if (!bSuccess || !Response.IsValid() || Response->GetResponseCode() != 200)
		{
			UE_LOG(LogTemp, Error, TEXT("DemoScenarioActor: /api/v1/ai/ask failed (code %d): %s"),
				Response.IsValid() ? Response->GetResponseCode() : -1,
				Response.IsValid() ? *Response->GetContentAsString() : TEXT(""));
			OnComplete(false, FString());
			return;
		}

		// Expected response: {"answer": "..."} (see /api/v1/ai/ask on the PHP side, index.php).
		FString Answer;
		TSharedPtr<FJsonObject> JsonObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
		if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid() || !JsonObject->TryGetStringField(TEXT("answer"), Answer))
		{
			UE_LOG(LogTemp, Error, TEXT("DemoScenarioActor: /api/v1/ai/ask returned unexpected body: %s"), *Response->GetContentAsString());
			OnComplete(false, FString());
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("DemoScenarioActor: ChatGPT answered: %s"), *Answer);
		OnComplete(true, Answer);
	});
	Request->ProcessRequest();
}
