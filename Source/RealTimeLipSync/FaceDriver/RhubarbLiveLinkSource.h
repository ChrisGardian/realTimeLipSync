// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ILiveLinkSource.h"

// "Virtual" LiveLink source: reads no hardware, instead pushes ARKit curve frames computed from
// Rhubarb visemes (see VisemeToArKitMapping). Lets the MetaHuman face be driven through the same
// path as iPhone facial capture (Face_AnimBP plus RigLogic via mh_arkit_mapping_pose), instead of
// SetMorphTarget, which would be overwritten every tick by the Post Process AnimBP.
class FRhubarbLiveLinkSource : public ILiveLinkSource, public TSharedFromThis<FRhubarbLiveLinkSource>
{
public:
	explicit FRhubarbLiveLinkSource(FName InSubjectName);

	//~ Begin ILiveLinkSource interface
	virtual void ReceiveClient(ILiveLinkClient* InClient, FGuid InSourceGuid) override;
	virtual bool IsSourceStillValid() const override;
	virtual bool RequestSourceShutdown() override;
	virtual FText GetSourceType() const override;
	virtual FText GetSourceMachineName() const override;
	virtual FText GetSourceStatus() const override;
	//~ End ILiveLinkSource interface

	// Declares the subject with the list of curves that will be pushed. Call once, after the
	// source has been added to the client (Client->AddSource) and before PushCurveFrame.
	void DeclareSubject(const TArray<FName>& CurveNames);

	// Pushes a frame of curve values: same order and size as CurveNames passed to DeclareSubject.
	void PushCurveFrame(const TArray<float>& CurveValues);

private:
	ILiveLinkClient* Client = nullptr;
	FGuid SourceGuid;
	FName SubjectName;
	bool bSubjectDeclared = false;
};
