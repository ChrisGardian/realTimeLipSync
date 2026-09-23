// Copyright Epic Games, Inc. All Rights Reserved.

#include "DemoScenarioActor.h"

#include "Dom/JsonObject.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MiddlewareAuthClient.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "SDemoScenarioWidget.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TimerManager.h"

ADemoScenarioActor::ADemoScenarioActor()
{
	// Same value as ADynamicSpeechTestActor: calibrated for the same TTS to Rhubarb to LiveLink
	// pipeline, so the same downstream LiveLink/AnimBP latency applies here.
	LipSyncDelaySeconds = 0.3f;
}

void ADemoScenarioActor::BeginPlay()
{
	Super::BeginPlay();

	// Lets a packaged build target another backend without the editor, e.g.
	// RealTimeLipSync.exe -BackendUrl=https://example.org (overrides the Details panel value).
	FString CommandLineUrl;
	if (FParse::Value(FCommandLine::Get(), TEXT("BackendUrl="), CommandLineUrl))
	{
		BackendBaseUrl = CommandLineUrl;
	}
	BackendBaseUrl.RemoveFromEnd(TEXT("/"));
	UE_LOG(LogTemp, Log, TEXT("DemoScenarioActor: backend = %s"), *BackendBaseUrl);

	UGameViewportClient* Viewport = GetWorld()->GetGameViewport();
	if (!bShowDemoUi || !Viewport)
	{
		return;
	}

	DemoWidget = SNew(SDemoScenarioWidget)
		.ScenarioTitle(ScenarioTitle)
		.ScenarioDescription(ScenarioDescription)
		.OnStartScenario(FOnDemoStartScenario::CreateUObject(this, &ADemoScenarioActor::HandleStartScenario))
		.OnAskQuestion(FOnDemoAskQuestion::CreateUObject(this, &ADemoScenarioActor::HandleAskQuestion))
		.OnQuit(FOnDemoQuit::CreateUObject(this, &ADemoScenarioActor::HandleQuit));
	Viewport->AddViewportWidgetContent(DemoWidget.ToSharedRef());

	// No gameplay input in this demo: the mouse and keyboard only drive the UI.
	if (APlayerController* PlayerController = GetWorld()->GetFirstPlayerController())
	{
		PlayerController->bShowMouseCursor = true;
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(DemoWidget);
		PlayerController->SetInputMode(InputMode);
	}
}

void ADemoScenarioActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(IntroTimerHandle);

	if (DemoWidget.IsValid())
	{
		if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
		{
			Viewport->RemoveViewportWidgetContent(DemoWidget.ToSharedRef());
		}
		DemoWidget.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void ADemoScenarioActor::HandleStartScenario()
{
	// SetTimer with a rate <= 0 clears the timer instead of firing it, hence the direct call.
	if (IntroDelaySeconds <= 0.f)
	{
		PlayIntro();
		return;
	}
	GetWorldTimerManager().SetTimer(IntroTimerHandle, this, &ADemoScenarioActor::PlayIntro, IntroDelaySeconds, /*bLoop*/ false);
}

void ADemoScenarioActor::HandleAskQuestion(const FString& Question)
{
	QuestionText = Question;
	AskQuestion();
}

void ADemoScenarioActor::HandleQuit()
{
	UKismetSystemLibrary::QuitGame(this, GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, /*bIgnorePlatformRestrictions*/ false);
}

void ADemoScenarioActor::PlayIntro()
{
	// Plain file shipped with the build (no imported asset, whose source path is editor-only data),
	// fed to the same pipeline as network responses: same decoding, async Rhubarb and latency log.
	const FString AudioFilePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir(), IntroAudioPath);

	TArray<uint8> AudioBytes;
	if (!FFileHelper::LoadFileToArray(AudioBytes, *AudioFilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("DemoScenarioActor: could not read intro audio %s"), *AudioFilePath);
		return;
	}

	ProcessIncomingAudioChunk(AudioBytes, FLatencyTrace(), TEXT("DemoIntro"));
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
	// No "fmt": the backend defaults to MP3, decoded client-side in ProcessIncomingAudioChunk.
	QueryParams.Add(TEXT("q"), TextToSpeak);

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
