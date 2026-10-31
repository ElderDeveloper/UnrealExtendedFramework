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

		// ExtendedFoundation is public: Publish/ESteamPublishSettings.h derives from UEFDeveloperSettings.
		// The publish password lives in its FEFCredentialStore.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			"ExtendedFoundation",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ExtendedSteamShared",
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
