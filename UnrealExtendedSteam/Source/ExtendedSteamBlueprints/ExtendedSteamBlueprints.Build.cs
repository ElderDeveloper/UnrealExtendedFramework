// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

using UnrealBuildTool;

public class ExtendedSteamBlueprints : ModuleRules
{
	public ExtendedSteamBlueprints(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicIncludePaths.Add(ModuleDirectory);

		// Public: BlueprintActions/ESteamAsyncActionBase.h derives from UEFAsyncActionBase.
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"ExtendedFoundation"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"ExtendedSteamShared",
				"UnrealExtendedSteam"
			}
		);
	}
}
