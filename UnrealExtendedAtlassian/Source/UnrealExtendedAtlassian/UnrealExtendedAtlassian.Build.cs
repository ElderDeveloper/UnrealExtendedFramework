// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class UnrealExtendedAtlassian : ModuleRules
{
	public UnrealExtendedAtlassian(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.Add(Path.Combine(ModuleDirectory));

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"HTTP",
			"Json",
			"JsonUtilities",
			"Projects",
		});
		AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");

		// Settings actions (Save Credentials / Test Connection) report their outcome through an
		// editor notification. Editor-only, guarded by WITH_EDITOR in the settings implementation.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"Slate",
				"SlateCore",
			});
		}

		// DPAPI (CryptProtectData/CryptUnprotectData) backs the per-user credential
		// store so the Atlassian API token is never written to disk in plain text.
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemLibraries.Add("Crypt32.lib");
		}
	}
}
