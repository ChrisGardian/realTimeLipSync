// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "RhubarbLipSyncRunner.generated.h"

// Une "mouth cue" : une forme de bouche (A-X) active entre Start et End (secondes),
// telle que sortie par Rhubarb Lip Sync dans le tableau JSON "mouthCues".
USTRUCT(BlueprintType)
struct FRhubarbMouthCue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "RhubarbLipSync")
	float Start = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "RhubarbLipSync")
	float End = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "RhubarbLipSync")
	FString Value;
};

// Lance l'exécutable Rhubarb Lip Sync sur un fichier audio et parse la timeline
// de visèmes (mouth cues) produite en JSON. Bloquant : à appeler hors du game thread
// si l'audio est long, sinon acceptable pour les tests en Phase 1.
UCLASS(BlueprintType)
class REALTIMELIPSYNC_API URhubarbLipSyncRunner : public UObject
{
	GENERATED_BODY()

public:
	// Chemin absolu vers rhubarb.exe. Configuré ici pour le poste de dev ;
	// à externaliser (config projet) si le pipeline doit tourner ailleurs.
	static const FString RhubarbExecutablePath;

	// Lance Rhubarb sur AudioFilePath, attend la fin du process, parse le JSON produit.
	// Le JSON est écrit à côté du fichier audio (même dossier, même nom de base, extension .json)
	// et est réécrit à chaque appel.
	UFUNCTION(BlueprintCallable, Category = "RhubarbLipSync")
	bool RunOnAudioFile(const FString& AudioFilePath, TArray<FRhubarbMouthCue>& OutMouthCues);

private:
	// Construit la ligne d'arguments passée à rhubarb.exe.
	FString BuildCommandLineArgs(const FString& AudioFilePath, const FString& OutputJsonPath) const;

	// Lance le process et bloque jusqu'à sa fin. Retourne false si le process n'a pas pu être créé
	// ou si son code de sortie indique une erreur.
	bool ExecuteRhubarbProcess(const FString& Args) const;

	// Charge et parse le fichier JSON produit par Rhubarb en tableau de mouth cues.
	bool ParseMouthCuesFromJson(const FString& JsonFilePath, TArray<FRhubarbMouthCue>& OutMouthCues) const;
};