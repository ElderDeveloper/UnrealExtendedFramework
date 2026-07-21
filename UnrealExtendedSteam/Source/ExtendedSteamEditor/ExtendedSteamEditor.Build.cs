// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

using UnrealBuildTool;

public class ExtendedSteamEditor : ModuleRules
{
	public ExtendedSteamEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Flat layout (house style): ModuleDirectory on the public include path so
		// internal files resolve their siblings via "Publish/ESteamPublishSettings.h".
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"Slate",
			"SlateCore",
			"InputCore",
			"ToolMenus",
			"Settings",
			"Projects",
		});
	}
}
