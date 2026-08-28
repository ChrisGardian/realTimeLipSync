// Copyright Epic Games, Inc. All Rights Reserved.

#include "RhubarbFaceActorBase.h"

#include "Components/AudioComponent.h"
#include "RhubarbLiveLinkSource.h"
#include "VisemeToArKitMapping.h"
#include "Features/IModularFeatures.h"
#include "ILiveLinkClient.h"

ARhubarbFaceActorBase::ARhubarbFaceActorBase()
{
	PrimaryActorTick.bCanEverTick = true;

	AudioPlayback = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioPlayback"));
	RootComponent = AudioPlayback;
	AudioPlayback->bAutoActivate = false;
}

void ARhubarbFaceActorBase::BeginPlay()
{
	Super::BeginPlay();

	if (!IModularFeatures::Get().IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		UE_LOG(LogTemp, Error, TEXT("%s: LiveLink client modular feature not available"), *GetClass()->GetName());
		return;
	}

	ILiveLinkClient& Client = IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
	LiveLinkSource = MakeShared<FRhubarbLiveLinkSource>(LiveLinkSubjectName);
	Client.AddSource(LiveLinkSource);

	// Curves de bouche (VisemeToArKitMapping) + curves d'idle animation (FIdleFaceAnimator) déclarées
	// ensemble, dans cet ordre : un seul subject LiveLink pour tout le visage.
	TArray<FName> AllCurveNames = VisemeToArKitMapping::GetUsedCurveNames();
	AllCurveNames.Append(FIdleFaceAnimator::GetCurveNames());
	LiveLinkSource->DeclareSubject(AllCurveNames);
	CurrentCurveValues.Init(0.f, AllCurveNames.Num());
}

void ARhubarbFaceActorBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
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

void ARhubarbFaceActorBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!LiveLinkSource.IsValid() || MouthCues.Num() == 0)
	{
		return;
	}

	ElapsedPlaybackTime += DeltaTime;

	// Temps utilisé pour chercher le cue, décalé par rapport au temps de lecture audio réel
	// (voir LipSyncDelaySeconds). Avant l'instant 0 ou après la dernière cue -> "X" (idle/neutre).
	const float VisemeSampleTime = ElapsedPlaybackTime - LipSyncDelaySeconds;

	FString CurrentViseme = TEXT("X");
	for (const FRhubarbMouthCue& Cue : MouthCues)
	{
		if (VisemeSampleTime >= Cue.Start && VisemeSampleTime < Cue.End)
		{
			CurrentViseme = Cue.Value;
			break;
		}
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::Yellow, FString::Printf(TEXT("Viseme: %s"), *CurrentViseme));
	}

	TArray<float> TargetCurveValues;
	VisemeToArKitMapping::GetWeightsForViseme(CurrentViseme, TargetCurveValues);

	for (int32 Index = 0; Index < TargetCurveValues.Num(); ++Index)
	{
		CurrentCurveValues[Index] = FMath::FInterpTo(CurrentCurveValues[Index], TargetCurveValues[Index], DeltaTime, VisemeInterpSpeed);
	}

	// Idle animation : indépendant du lissage des visèmes ci-dessus, écrit directement les derniers
	// éléments de CurrentCurveValues (voir FIdleFaceAnimator::WriteCurveValues).
	FIdleFaceAnimationSettings IdleSettings;
	IdleSettings.MinBlinkInterval = MinBlinkInterval;
	IdleSettings.MaxBlinkInterval = MaxBlinkInterval;
	IdleSettings.BlinkDuration = BlinkDuration;
	IdleFaceAnimator.Update(DeltaTime, IdleSettings);
	IdleFaceAnimator.WriteCurveValues(CurrentCurveValues);

	LiveLinkSource->PushCurveFrame(CurrentCurveValues);
}
