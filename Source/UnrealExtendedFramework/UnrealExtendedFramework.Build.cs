// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealExtendedFramework : ModuleRules
{
	public UnrealExtendedFramework(ReadOnlyTargetRules Target) : base(Target)
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
				"CoreUObject",
				"Engine",
				"InputCore",
				"EnhancedInput",
				"DeveloperSettings",
				"Slate",
				"SlateCore",
				"UMG",
				"PreLoadScreen",
				"RenderCore",
				"ApplicationCore",
				"AIModule",
				"Niagara",
				"NavigationSystem",
				"GameplayAbilities",
				"GameplayTasks",
				"XmlParser",
				"EngineSettings",
				"OnlineSubsystem",
				"OnlineSubsystemUtils",
				"OnlineSubsystemNull"

				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"GameplayTags",
				"GameplayTasks",
				"JsonUtilities",
				"Json",
				"XmlParser",
				"ApplicationCore", "AnimGraphRuntime", "Niagara"
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
