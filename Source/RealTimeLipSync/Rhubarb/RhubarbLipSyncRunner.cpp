// Copyright Epic Games, Inc. All Rights Reserved.

#include "RhubarbLipSyncRunner.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

const FString URhubarbLipSyncRunner::RhubarbExecutablePath = TEXT("C:/Tools/Rhubarb-Lip-Sync-1.14.0-Windows/rhubarb.exe");

bool URhubarbLipSyncRunner::RunOnAudioFile(const FString& AudioFilePath, TArray<FRhubarbMouthCue>& OutMouthCues)
{
	OutMouthCues.Reset();

	if (!FPaths::FileExists(AudioFilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("RhubarbLipSyncRunner: audio file not found: %s"), *AudioFilePath);
		return false;
	}

	const FString OutputJsonPath = FPaths::ChangeExtension(AudioFilePath, TEXT("json"));
	const FString Args = BuildCommandLineArgs(AudioFilePath, OutputJsonPath);

	if (!ExecuteRhubarbProcess(Args))
	{
		return false;
	}

	return ParseMouthCuesFromJson(OutputJsonPath, OutMouthCues);
}

FString URhubarbLipSyncRunner::BuildCommandLineArgs(const FString& AudioFilePath, const FString& OutputJsonPath) const
{
	return FString::Printf(
		TEXT("-f json -o \"%s\" \"%s\" --recognizer phonetic"),
		*OutputJsonPath,
		*AudioFilePath);
}

bool URhubarbLipSyncRunner::ExecuteRhubarbProcess(const FString& Args) const
{
	if (!FPaths::FileExists(RhubarbExecutablePath))
	{
		UE_LOG(LogTemp, Error, TEXT("RhubarbLipSyncRunner: rhubarb.exe not found at %s"), *RhubarbExecutablePath);
		return false;
	}

	uint32 ProcessId = 0;
	FProcHandle ProcHandle = FPlatformProcess::CreateProc(
		*RhubarbExecutablePath,
		*Args,
		/*bLaunchDetached*/ false,
		/*bLaunchHidden*/ true,
		/*bLaunchReallyHidden*/ true,
		&ProcessId,
		/*PriorityModifier*/ 0,
		/*OptionalWorkingDirectory*/ nullptr,
		/*PipeWriteChild*/ nullptr);

	if (!ProcHandle.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("RhubarbLipSyncRunner: failed to launch rhubarb.exe"));
		return false;
	}

	while (FPlatformProcess::IsProcRunning(ProcHandle))
	{
		FPlatformProcess::Sleep(0.05f);
	}

	int32 ReturnCode = -1;
	const bool bGotReturnCode = FPlatformProcess::GetProcReturnCode(ProcHandle, &ReturnCode);
	FPlatformProcess::CloseProc(ProcHandle);

	if (!bGotReturnCode || ReturnCode != 0)
	{
		UE_LOG(LogTemp, Error, TEXT("RhubarbLipSyncRunner: rhubarb.exe exited with code %d"), ReturnCode);
		return false;
	}

	return true;
}

bool URhubarbLipSyncRunner::ParseMouthCuesFromJson(const FString& JsonFilePath, TArray<FRhubarbMouthCue>& OutMouthCues) const
{
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *JsonFilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("RhubarbLipSyncRunner: could not read %s"), *JsonFilePath);
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(JsonReader, RootObject) || !RootObject.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("RhubarbLipSyncRunner: failed to parse JSON in %s"), *JsonFilePath);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* MouthCuesArray = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("mouthCues"), MouthCuesArray))
	{
		UE_LOG(LogTemp, Error, TEXT("RhubarbLipSyncRunner: no 'mouthCues' field in %s"), *JsonFilePath);
		return false;
	}

	for (const TSharedPtr<FJsonValue>& CueValue : *MouthCuesArray)
	{
		const TSharedPtr<FJsonObject>* CueObject;
		if (!CueValue->TryGetObject(CueObject))
		{
			continue;
		}

		FRhubarbMouthCue MouthCue;
		(*CueObject)->TryGetNumberField(TEXT("start"), MouthCue.Start);
		(*CueObject)->TryGetNumberField(TEXT("end"), MouthCue.End);
		(*CueObject)->TryGetStringField(TEXT("value"), MouthCue.Value);

		OutMouthCues.Add(MouthCue);
	}

	return true;
}