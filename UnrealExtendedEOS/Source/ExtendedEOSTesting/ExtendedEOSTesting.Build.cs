// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

using UnrealBuildTool;

public class ExtendedEOSTesting : ModuleRules
{
	public ExtendedEOSTesting(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"EOSSDK",
				"EOSShared",
				"OnlineSubsystem",
				"OnlineSubsystemUtils",
				"ExtendedEOSShared",
				"UnrealExtendedEOS"
			}
			);
	}
}
