// Copyright Epic Games, Inc. All Rights Reserved.

#include "RhubarbLiveLinkSource.h"

#include "HAL/PlatformTime.h"
#include "ILiveLinkClient.h"
#include "Roles/LiveLinkBasicRole.h"

FRhubarbLiveLinkSource::FRhubarbLiveLinkSource(FName InSubjectName)
	: SubjectName(InSubjectName)
{
}

void FRhubarbLiveLinkSource::ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid)
{
	Client = InClient;
	SourceGuid = InSourceGuid;
}

bool FRhubarbLiveLinkSource::IsSourceStillValid() const
{
	return Client != nullptr;
}

bool FRhubarbLiveLinkSource::RequestSourceShutdown()
{
	Client = nullptr;
	bSubjectDeclared = false;
	return true;
}

FText FRhubarbLiveLinkSource::GetSourceType() const
{
	return NSLOCTEXT("RealTimeLipSync", "RhubarbLiveLinkSourceType", "Rhubarb Lip Sync");
}

FText FRhubarbLiveLinkSource::GetSourceMachineName() const
{
	return NSLOCTEXT("RealTimeLipSync", "RhubarbLiveLinkSourceMachine", "Local");
}

FText FRhubarbLiveLinkSource::GetSourceStatus() const
{
	return IsSourceStillValid()
		? NSLOCTEXT("RealTimeLipSync", "RhubarbLiveLinkSourceActive", "Active")
		: NSLOCTEXT("RealTimeLipSync", "RhubarbLiveLinkSourceInactive", "Inactive");
}

void FRhubarbLiveLinkSource::DeclareSubject(const TArray<FName>& CurveNames)
{
	if (!Client)
	{
		UE_LOG(LogTemp, Warning, TEXT("FRhubarbLiveLinkSource: DeclareSubject called before the source was added to a client"));
		return;
	}

	FLiveLinkStaticDataStruct StaticDataStruct(FLiveLinkBaseStaticData::StaticStruct());
	FLiveLinkBaseStaticData* StaticData = StaticDataStruct.Cast<FLiveLinkBaseStaticData>();
	StaticData->PropertyNames = CurveNames;

	const FLiveLinkSubjectKey SubjectKey(SourceGuid, SubjectName);
	Client->PushSubjectStaticData_AnyThread(SubjectKey, ULiveLinkBasicRole::StaticClass(), MoveTemp(StaticDataStruct));
	bSubjectDeclared = true;
}

void FRhubarbLiveLinkSource::PushCurveFrame(const TArray<float>& CurveValues)
{
	if (!Client || !bSubjectDeclared)
	{
		return;
	}

	FLiveLinkFrameDataStruct FrameDataStruct(FLiveLinkBaseFrameData::StaticStruct());
	FLiveLinkBaseFrameData* FrameData = FrameDataStruct.Cast<FLiveLinkBaseFrameData>();
	FrameData->WorldTime = FLiveLinkWorldTime(FPlatformTime::Seconds());
	FrameData->PropertyValues = CurveValues;

	const FLiveLinkSubjectKey SubjectKey(SourceGuid, SubjectName);
	Client->PushSubjectFrameData_AnyThread(SubjectKey, MoveTemp(FrameDataStruct));
}
