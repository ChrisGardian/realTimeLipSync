// Copyright Epic Games, Inc. All Rights Reserved.

#include "RhubarbMetaHumanActor.h"

#include "Components/AudioComponent.h"
#include "RhubarbLiveLinkSource.h"
#include "VisemeToArKitMapping.h"
#include "Features/IModularFeatures.h"
#include "ILiveLinkClient.h"
#include "Sound/SoundWave.h"

#if WITH_EDITORONLY_DATA
#include "EditorFramework/AssetImportData.h"
#endif

ARhubarbMetaHumanActor::ARhubarbMetaHumanActor()
{
	PrimaryActorTick.bCanEverTick = true;

	AudioPlayback = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioPlayback"));
	RootComponent = AudioPlayback;
	AudioPlayback->bAutoActivate = false;
}

void ARhubarbMetaHumanActor::BeginPlay()
{
	Super::BeginPlay();

	if (!SoundToPlay)
	{
		UE_LOG(LogTemp, Warning, TEXT("ARhubarbMetaHumanActor: SoundToPlay is not set"));
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
		UE_LOG(LogTemp, Warning, TEXT("ARhubarbMetaHumanActor: could not resolve source file path for %s (editor-only data, see TODO.md)"), *SoundToPlay->GetName());
		return;
	}

	URhubarbLipSyncRunner* Runner = NewObject<URhubarbLipSyncRunner>(this);
	if (!Runner->RunOnAudioFile(AudioFilePath, MouthCues))
	{
		UE_LOG(LogTemp, Warning, TEXT("ARhubarbMetaHumanActor: RunOnAudioFile failed for %s"), *AudioFilePath);
		return;
	}

	if (!IModularFeatures::Get().IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		UE_LOG(LogTemp, Error, TEXT("ARhubarbMetaHumanActor: LiveLink client modular feature not available"));
		return;
	}

	ElapsedPlaybackTime = 0.f;

	ILiveLinkClient& Client = IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
	LiveLinkSource = MakeShared<FRhubarbLiveLinkSource>(LiveLinkSubjectName);
	Client.AddSource(LiveLinkSource);
	LiveLinkSource->DeclareSubject(VisemeToArKitMapping::GetUsedCurveNames());

	AudioPlayback->SetSound(SoundToPlay);
	AudioPlayback->Play();
}

void ARhubarbMetaHumanActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (LiveLinkSource.IsValid())
	{
		if (IModularFeatures::Get().IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
		{
			IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName).RemoveSource(LiveLinkSource);
		}
		LiveLinkSource.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void ARhubarbMetaHumanActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (MouthCues.Num() == 0 || !LiveLinkSource.IsValid())
	{
		return;
	}

	ElapsedPlaybackTime += DeltaTime;

	// Visème actif à l'instant courant ; "X" (idle/neutre) si on est entre deux cues ou après la dernière.
	FString CurrentViseme = TEXT("X");
	for (const FRhubarbMouthCue& Cue : MouthCues)
	{
		if (ElapsedPlaybackTime >= Cue.Start && ElapsedPlaybackTime < Cue.End)
		{
			CurrentViseme = Cue.Value;
			break;
		}
	}

	TArray<float> CurveValues;
	VisemeToArKitMapping::GetWeightsForViseme(CurrentViseme, CurveValues);
	LiveLinkSource->PushCurveFrame(CurveValues);
}
