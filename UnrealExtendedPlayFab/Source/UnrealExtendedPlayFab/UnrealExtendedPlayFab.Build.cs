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
				"DeveloperSettings",
				// Public: UnrealExtendedPlayFab.h includes EFLog.h, so anything including it needs the path.
				"UnrealExtendedFrameworkLog"
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"ApplicationCore",   // IPlatformInputDeviceMapper (input device connection tracking)
				"UnrealExtendedFramework"
			}
			);
	}
}
