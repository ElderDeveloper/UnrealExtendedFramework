// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class UnrealExtendedFrameworkEditor : ModuleRules
{
	public UnrealExtendedFrameworkEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory));

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"Slate",
				"SlateCore"
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
					"AnimGraph",
					"AnimGraphRuntime",
					"UnrealExtendedFramework",
				}
			);

			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"EditorStyle",
				"KismetCompiler",
				"KismetWidgets",
				"Projects",

				// Shared editor feature infrastructure (UI Lab, Localization Workbench)
				"ToolMenus",
				"WorkspaceMenuStructure",
				"Json",
				"DeveloperSettings",
				"PropertyEditor",

				// UI Lab
				"UMG",
				"UMGEditor",
				"ContentBrowser",
				"EnhancedInput",
				"ApplicationCore",
				"AutomationDriver",
				"AutomationController",

				// Localization Workbench
				"Localization",
				"LocalizationCommandletExecution",
				"LocalizationService",
				"InternationalizationSettings",
				"SourceControl"
			});
		}

		PrivateIncludePathModuleNames.AddRange(
			new string[]
			{
#if WITH_EDITOR
				"SkeletonEditor",
#endif
			}
		);
	}
}
