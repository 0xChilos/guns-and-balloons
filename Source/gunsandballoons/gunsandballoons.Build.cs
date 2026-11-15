// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class gunsandballoons : ModuleRules
{
	public gunsandballoons(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput" });

		// Slate: the hook debug menu (FHookDebugMenu).
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// UMG: the main menu widget and its in-world panel. EngineSettings: the project version label.
		PrivateDependencyModuleNames.AddRange(new string[] { "UMG", "EngineSettings" });

		// Niagara: the hook pickups' sparkle (AHookPickup::Sparkle).
		PrivateDependencyModuleNames.Add("Niagara");
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "AnimGraph", "BlueprintGraph", "AnimGraphRuntime" });
		}
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
