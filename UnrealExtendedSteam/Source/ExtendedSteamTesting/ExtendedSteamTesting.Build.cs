// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

using UnrealBuildTool;

public class ExtendedSteamTesting : ModuleRules
{
	public ExtendedSteamTesting(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateIncludePaths.Add(ModuleDirectory);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Sockets",
				"ExtendedSteamShared",
				"ExtendedSteamSockets",
				"ExtendedSteamWeb",
				"OnlineSubsystem",
				"OnlineSubsystemExtendedSteam",
				"UnrealExtendedSteam"
			}
		);
	}
}
