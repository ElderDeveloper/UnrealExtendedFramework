// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class UnrealExtendedPerfSentinel : ModuleRules
{
	public UnrealExtendedPerfSentinel(ReadOnlyTargetRules Target) : base(Target)
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
			"RenderCore",
			"RHI",
			"TraceLog",
			"UMG",
			"ImageWrapper",
			"Json",
			"JsonUtilities",
			"Projects",
		});
	}
}
