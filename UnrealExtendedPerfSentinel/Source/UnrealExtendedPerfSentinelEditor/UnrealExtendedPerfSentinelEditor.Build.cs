// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

using UnrealBuildTool;

public class UnrealExtendedPerfSentinelEditor : ModuleRules
{
	public UnrealExtendedPerfSentinelEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"UnrealExtendedPerfSentinel",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"InputCore",
			"ApplicationCore",
			"Slate",
			"SlateCore",
			"EditorStyle",
			"ToolMenus",
			"DesktopPlatform",
			"Settings",
			"Json",
			"TraceAnalysis",
			"TraceServices",
		});
	}
}
