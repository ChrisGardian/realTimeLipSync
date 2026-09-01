// Copyright Epic Games, Inc. All Rights Reserved.

#include "DynamicSpeechTestActor.h"

#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "MiddlewareAuthClient.h"
#include "Misc/FileHelper.h"

ADynamicSpeechTestActor::ADynamicSpeechTestActor()
{
	LipSyncDelaySeconds = 0.2f;
}

void ADynamicSpeechTestActor::SimulateIncomingChunk()
{
	TArray<uint8> WavBytes;
	if (!FFileHelper::LoadFileToArray(WavBytes, *TestWavPath.FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("DynamicSpeechTestActor: could not load %s"), *TestWavPath.FilePath);
		return;
	}

	ProcessIncomingAudioChunk(WavBytes, FLatencyTrace(), TEXT("Simulate"));
}

void ADynamicSpeechTestActor::RequestSpeechFromBackend()
{
	if (bHasSession)
	{
		SendSignedTtsRequest();
		return;
	}

	TWeakObjectPtr<ADynamicSpeechTestActor> WeakThis(this);
	FMiddlewareAuthClient::RequestSession(BackendBaseUrl, [WeakThis](bool bSuccess, FString Sid, FString SecretHex)
	{
		ADynamicSpeechTestActor* Actor = WeakThis.Get();
		if (!Actor)
		{
			return;
		}

		if (!bSuccess)
		{
			UE_LOG(LogTemp, Error, TEXT("DynamicSpeechTestActor: could not obtain a backend session (sid/secret)"));
			return;
		}

		Actor->CachedSid = Sid;
		Actor->CachedSecretHex = SecretHex;
		Actor->bHasSession = true;
		Actor->SendSignedTtsRequest();
	});
}

void ADynamicSpeechTestActor::SendSignedTtsRequest()
{
	TMap<FString, FString> QueryParams;
	QueryParams.Add(TEXT("q"), TextToSpeak);
	QueryParams.Add(TEXT("fmt"), TEXT("wav"));

	const FString SignedUrl = FMiddlewareAuthClient::BuildSignedUrl(
		BackendBaseUrl, TEXT("/api/v1/ai/tts"), QueryParams, CachedSid, CachedSecretHex);

	const TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(SignedUrl);
	Request->SetVerb(TEXT("GET"));

	TWeakObjectPtr<ADynamicSpeechTestActor> WeakThis(this);
	const double RequestSentTime = FPlatformTime::Seconds();
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, RequestSentTime](FHttpRequestPtr, const FHttpResponsePtr& Response, bool bSuccess)
	{
		const double ResponseReceivedTime = FPlatformTime::Seconds();

		ADynamicSpeechTestActor* Actor = WeakThis.Get();
		if (!Actor)
		{
			return;
		}

		if (!bSuccess || !Response.IsValid() || Response->GetResponseCode() != 200)
		{
			UE_LOG(LogTemp, Error, TEXT("DynamicSpeechTestActor: /api/v1/ai/tts failed (code %d): %s"),
				Response.IsValid() ? Response->GetResponseCode() : -1,
				Response.IsValid() ? *Response->GetContentAsString() : TEXT(""));
			return;
		}

		UE_LOG(LogTemp, Log, TEXT("DynamicSpeechTestActor: received %d bytes from /api/v1/ai/tts"), Response->GetContent().Num());

		FLatencyTrace Trace;
		Trace.RequestSent = RequestSentTime;
		Trace.ResponseReceived = ResponseReceivedTime;
		Actor->ProcessIncomingAudioChunk(Response->GetContent(), Trace, TEXT("Backend"));
	});
	Request->ProcessRequest();
}
