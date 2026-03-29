// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class UnrealExtendedPlayFab : ModuleRules
{
	public UnrealExtendedPlayFab(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				ModuleDirectory,
				Path.Combine(ModuleDirectory, "BlueprintActions")
			}
			);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
				"HTTP",
				"Json",
				"JsonUtilities",
				"DeveloperSettings"
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"UnrealExtendedFramework"
			}
			);
	}
}
