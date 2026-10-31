// Copyright Moon Punch Games. All Rights Reserved.

using UnrealBuildTool;

/// <summary>
/// The shared base every Extended plugin can depend on.
///
/// Keep this module light: it may depend only on modules that ship with every engine install and
/// need no plugin. Anything tied to gameplay, UI, AI, audio or a specific online service belongs in
/// the plugin that needs it, never here. Public dependencies are the modules that appear in this
/// module's public headers.
/// </summary>
public class ExtendedFoundation : ModuleRules
{
	public ExtendedFoundation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"DeveloperSettings",
				"HTTP",
				"Json",
				"JsonUtilities"
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// Testing/EFTestHelpers: plugin fixture lookup.
				"Projects"
			});

		if (Target.bBuildEditor)
		{
			// UEFDeveloperSettings::OpenInEditor; resolved at runtime, so only the interface header is needed.
			PrivateIncludePathModuleNames.Add("Settings");
		}

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// FEFCredentialStore: DPAPI (CryptProtectData / CryptUnprotectData).
			PublicSystemLibraries.Add("Crypt32.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			// FEFCredentialStore: Keychain Services.
			PublicFrameworks.Add("Security");
		}
	}
}
