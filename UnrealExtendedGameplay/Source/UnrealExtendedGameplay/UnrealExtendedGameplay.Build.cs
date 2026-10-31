// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

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
				"EnhancedInput",
				"InputCore",
				"EngineSettings",
				"Slate",
				"SlateCore",
				"UMG",
				"Niagara",
				"MotionWarping",
				"PhysicsCore",
				"AIModule",
				"GameplayTags",
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
				"NetCore",
				"EngineSettings",
				"Slate",
				"SlateCore",
				"UnrealExtendedFramework",
				// Async nodes and asset loading helpers.
				"ExtendedFoundation"
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
