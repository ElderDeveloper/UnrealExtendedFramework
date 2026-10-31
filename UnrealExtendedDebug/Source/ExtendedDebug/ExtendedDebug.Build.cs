// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

using UnrealBuildTool;

/// <summary>
/// Unreal Extended Debug runtime module: cheats, debug menu and watches for development builds.
///
/// The module builds in every configuration so Blueprints that call it still load in Shipping,
/// but EXTENDED_DEBUG_ENABLED is 0 there and every function does nothing.
/// </summary>
public class ExtendedDebug : ModuleRules
{
	public ExtendedDebug(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				// EDbgSettings derives from UEFDeveloperSettings.
				"DeveloperSettings",
				// EDbgSettings: the menu key is an FKey.
				"InputCore",
				"ExtendedFoundation"
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// The debug menu and the key that opens it.
				"ApplicationCore",
				"Slate",
				"SlateCore"
			});

		bool bEnabled = Target.Configuration != UnrealTargetConfiguration.Shipping;
		PublicDefinitions.Add("EXTENDED_DEBUG_ENABLED=" + (bEnabled ? "1" : "0"));
	}
}
