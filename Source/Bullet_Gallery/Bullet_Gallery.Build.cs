// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Bullet_Gallery : ModuleRules
{
	public Bullet_Gallery(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Niagara", "UMG" });

		// Lab 5: artificial intelligence (AI controller, perception, behaviour tree),
		// gameplay tasks used by the behaviour tree tasks and the navigation system
		PublicDependencyModuleNames.AddRange(new string[] { "AIModule", "GameplayTasks", "NavigationSystem" });

		// Slate is needed for the widget styles (brushes, fonts) built in code
		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
