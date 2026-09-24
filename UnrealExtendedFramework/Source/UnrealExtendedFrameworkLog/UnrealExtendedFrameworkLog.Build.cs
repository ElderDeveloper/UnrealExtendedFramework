// Copyright Moon Punch Games. All Rights Reserved.

using UnrealBuildTool;

/**
 * Extended Log: category log files written by the EF_LOG macro.
 *
 * Deliberately independent of UE_LOG / GLog, and deliberately light: it depends on nothing beyond
 * Core, CoreUObject, Engine (PIE instance labels, the Blueprint node) and DeveloperSettings, so any
 * module (Steam, EOS, PlayFab, game code) can use EF_LOG without inheriting the
 * UnrealExtendedFramework module's GAS / Niagara / OSS dependencies.
 * Plan: Documents/ExtendedFrameworkLogSystemPlan.md (DOP repository).
 */
public class UnrealExtendedFrameworkLog : ModuleRules
{
	public UnrealExtendedFrameworkLog(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"DeveloperSettings"
			});
	}
}
