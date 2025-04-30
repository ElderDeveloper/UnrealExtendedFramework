// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealExtendedEditor : ModuleRules
{
	public UnrealExtendedEditor(ReadOnlyTargetRules Target) : base(Target)
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
				"Slate",
				"SlateCore"
				// ... add other public dependencies that you statically link with here ...
			}
			);

		if (Target.Type == TargetType.Editor)
		{
			
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
				"UnrealEd",
				"BlueprintGraph",
				"GraphEditor",
				}
			);
			
			PrivateDependencyModuleNames.AddRange(new string[] {
				"EditorStyle",
				"KismetCompiler",
				"KismetWidgets",
				"Projects"
			});
		}
		
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
		
			
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
#if WITH_EDITOR
				"SkeletonEditor",
				//"AnimationBlueprintEditor",
#endif
			}
		);
	}
}
