// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ILiveLinkSource.h"

// Source LiveLink "virtuelle" : ne lit aucun matériel, pousse elle-même des frames de curves
// ARKit calculées à partir des visèmes Rhubarb (voir VisemeToArKitMapping). Permet de piloter
// le visage MetaHuman par le même chemin que la capture faciale iPhone (Face_AnimBP + RigLogic
// via mh_arkit_mapping_pose), sans passer par SetMorphTarget qui serait écrasé à chaque tick
// par le Post Process AnimBP (voir mémoire projet / CLAUDE.md pour le détail de cette décision).
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

	// Déclare le subject avec la liste de curves qui seront poussées. À appeler une fois,
	// après que la source a été ajoutée au client (Client->AddSource) et avant PushCurveFrame.
	void DeclareSubject(const TArray<FName>& CurveNames);

	// Pousse une frame de valeurs de curves : même ordre et même taille que CurveNames
	// passé à DeclareSubject.
	void PushCurveFrame(const TArray<float>& CurveValues);

private:
	ILiveLinkClient* Client = nullptr;
	FGuid SourceGuid;
	FName SubjectName;
	bool bSubjectDeclared = false;
};
