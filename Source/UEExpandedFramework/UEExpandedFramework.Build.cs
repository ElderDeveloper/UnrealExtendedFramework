// Some copyright should be here...

using UnrealBuildTool;

public class UEExpandedFramework : ModuleRules
{
	public UEExpandedFramework(ReadOnlyTargetRules Target) : base(Target)
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
				"EditorSubsystem",
				"Slate",
				"SlateCore",
				"UMG",
				"Networking" ,
				"Http", "Json", "JsonUtilities" , "Sockets",
				"Niagara",
				"PhysicsCore",
				"AIModule",
				"GameplayAbilities",
				"GameplayTasks",
				"GameplayTasks",
				"NavigationSystem",
				"JsonUtilities",
				"Json",
				"AnimGraphRuntime"
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
