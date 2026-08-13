// Copyright Epic Games, Inc. All Rights Reserved.

#include "RhubarbTestActor.h"

#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Sound/SoundWave.h"

#if WITH_EDITORONLY_DATA
#include "EditorFramework/AssetImportData.h"
#endif

ARhubarbTestActor::ARhubarbTestActor()
{
	PrimaryActorTick.bCanEverTick = true;

	FaceMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FaceMesh"));
	RootComponent = FaceMesh;

	AudioPlayback = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioPlayback"));
	AudioPlayback->SetupAttachment(FaceMesh);
	AudioPlayback->bAutoActivate = false;
}

void ARhubarbTestActor::BeginPlay()
{
	Super::BeginPlay();

	if (!SoundToPlay)
	{
		UE_LOG(LogTemp, Warning, TEXT("ARhubarbTestActor: SoundToPlay is not set"));
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
		UE_LOG(LogTemp, Warning, TEXT("ARhubarbTestActor: could not resolve source file path for %s (editor-only data, see TODO.md)"), *SoundToPlay->GetName());
		return;
	}

	URhubarbLipSyncRunner* Runner = NewObject<URhubarbLipSyncRunner>(this);
	if (!Runner->RunOnAudioFile(AudioFilePath, MouthCues))
	{
		UE_LOG(LogTemp, Warning, TEXT("ARhubarbTestActor: RunOnAudioFile failed for %s"), *AudioFilePath);
		return;
	}

	ElapsedPlaybackTime = 0.f;
	ActiveCueIndex = INDEX_NONE;

	AudioPlayback->SetSound(SoundToPlay);
	AudioPlayback->Play();
}

void ARhubarbTestActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (MouthCues.Num() == 0)
	{
		return;
	}

	ElapsedPlaybackTime += DeltaTime;

	int32 CueIndex = INDEX_NONE;
	for (int32 Index = 0; Index < MouthCues.Num(); ++Index)
	{
		if (ElapsedPlaybackTime >= MouthCues[Index].Start && ElapsedPlaybackTime < MouthCues[Index].End)
		{
			CueIndex = Index;
			break;
		}
	}

	if (CueIndex == INDEX_NONE)
	{
		if (ActiveCueIndex != INDEX_NONE)
		{
			if (const FName* LastMorph = VisemeToMorphTarget.Find(MouthCues[ActiveCueIndex].Value))
			{
				FaceMesh->SetMorphTarget(*LastMorph, 0.f);
			}
			ActiveCueIndex = INDEX_NONE;
		}
		return;
	}

	ActiveCueIndex = CueIndex;

	const FRhubarbMouthCue& CurrentCue = MouthCues[CueIndex];
	const float Duration = CurrentCue.End - CurrentCue.Start;
	const float Alpha = Duration > KINDA_SMALL_NUMBER
		? FMath::Clamp((ElapsedPlaybackTime - CurrentCue.Start) / Duration, 0.f, 1.f)
		: 1.f;

	// Rhubarb enchaîne les cues bout à bout : la cue précédente est donc simplement
	// celle d'avant dans le tableau. Son poids est le complément de Alpha, ce qui
	// crossfade current/previous sur la durée propre de la cue courante.
	const bool bSameAsPrevious = CueIndex > 0 && MouthCues[CueIndex - 1].Value == CurrentCue.Value;

	if (const FName* CurrentMorph = VisemeToMorphTarget.Find(CurrentCue.Value))
	{
		FaceMesh->SetMorphTarget(*CurrentMorph, bSameAsPrevious ? 1.f : Alpha);
	}

	if (CueIndex > 0 && !bSameAsPrevious)
	{
		if (const FName* PreviousMorph = VisemeToMorphTarget.Find(MouthCues[CueIndex - 1].Value))
		{
			FaceMesh->SetMorphTarget(*PreviousMorph, 1.f - Alpha);
		}
	}
}
