// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealExtendedGameplay : ModuleRules
{
	public UnrealExtendedGameplay(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",	
				"Engine",
				"InputCore",
				"EngineSettings",
				"Slate",
				"SlateCore",
				"UMG",
				"Networking" ,
				"Sockets",
				"OnlineSubsystem",
				"OnlineSubsystemUtils",
				"OnlineSubsystemSteam",
				"OnlineSubsystemNull",
				"Niagara",
				"PhysicsCore",
				"AIModule",
				"GameplayAbilities",
				"GameplayTasks",
				"NavigationSystem"
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"EngineSettings",
				"Slate",
				"SlateCore",
				"GameplayTags",
				"GameplayTasks",
				"JsonUtilities",
				"Json",
				"UnrealExtendedFramework"
				// ... add private dependencies that you statically link with here ...	
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
