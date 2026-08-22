// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RealTimeLipSync : ModuleRules
{
	public RealTimeLipSync(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Sous-dossiers par responsabilité (Rhubarb/, FaceDriver/, Test/) : pas de split Public/Private,
		// donc on les déclare explicitement pour que les #include par simple nom de fichier résolvent
		// entre dossiers (UBT n'ajoute pas les sous-dossiers du module au chemin d'include par défaut).
		PrivateIncludePaths.AddRange(new string[] {
			ModuleDirectory + "/Rhubarb",
			ModuleDirectory + "/FaceDriver",
			ModuleDirectory + "/Test",
		});

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Json", "JsonUtilities", "LiveLinkInterface" });

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
