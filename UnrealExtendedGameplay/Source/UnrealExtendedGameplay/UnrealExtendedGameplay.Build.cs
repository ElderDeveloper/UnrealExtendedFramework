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
				// Anim nodes (Turn In Place, Foot Lock) derive from its node types.
				"AnimGraphRuntime",
				"PhysicsCore",
				"AIModule",
				"GameplayTags",
				"GameplayTasks",
				"NavigationSystem",
				// Asset loading helpers, settings base and status reporting. Public:
				// SoftCollision/EGSoftCollisionSettings.h derives from UEFDeveloperSettings.
				"ExtendedFoundation",
				// Public: Cover, Targeting, Footstep and the damage and soft-lock notifies use its types in public headers.
				"UnrealExtendedFramework"
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"NetCore",
				// Two-bone IK (Foot Lock).
				"AnimationCore",
				"EngineSettings",
				"Slate",
				"SlateCore",
				"DeveloperSettings",
				"ProceduralMeshComponent"
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
