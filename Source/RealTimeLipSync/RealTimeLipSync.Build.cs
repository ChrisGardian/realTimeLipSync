// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RealTimeLipSync : ModuleRules
{
	public RealTimeLipSync(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Subfolders organized by responsibility (Rhubarb/, FaceDriver/, Test/, Http/, Audio/, UI/), no Public/Private
		// split, so declare them explicitly for bare-filename #include to resolve across folders
		// (UBT does not add module subfolders to the include path by default).
		PrivateIncludePaths.AddRange(new string[] {
			ModuleDirectory + "/Rhubarb",
			ModuleDirectory + "/FaceDriver",
			ModuleDirectory + "/Test",
			ModuleDirectory + "/Http",
			ModuleDirectory + "/Audio",
			ModuleDirectory + "/UI",
		});

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Json", "JsonUtilities", "LiveLinkInterface", "HTTP" });

		// Rhubarb is launched as an external process, so it must exist as loose files next to the
		// packaged game: rhubarb.exe, its res/ folder (Sphinx models, loaded at startup) and its
		// LICENSE.md (MIT + BSD dependencies, must ship with the binary). "..." = recursive.
		RuntimeDependencies.Add("$(ProjectDir)/ThirdParty/Rhubarb/...");

		// Pure Slate demo UI (UI/SDemoScenarioWidget)
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
