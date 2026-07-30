// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class UnrealExtendedAtlassianEditor : ModuleRules
{
	public UnrealExtendedAtlassianEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(Path.Combine(ModuleDirectory));

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"UnrealExtendedAtlassian",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"InputCore",
			"ApplicationCore",
			"ContentBrowser",
			"LevelEditor",
			"RHI",
			"Slate",
			"SlateCore",
			"EditorStyle",
			"EditorFramework",
			"ToolMenus",
			"UMG",
			"WorkspaceMenuStructure",
			"PropertyEditor",
			"Settings",
			"Projects",
			"HTTP",
			"Json",
			"JsonUtilities",
			"RenderCore",
			"ImageWrapper",
			"DesktopPlatform",
			"DirectoryWatcher",
		});
	}
}
