// Copyright Moon Punch Games. All Rights Reserved.

using UnrealBuildTool;

/**
 * Device profile selector: picks the device profile before the first frame (a Steam Deck gets
 * "SteamDeck", every other machine the platform's own profile).
 *
 * The device profile manager loads it on demand during engine pre-init, long before UObjects or the
 * game module exist, so it uses nothing but Core, the config system and the engine's selector
 * interface. Rules come from [ExtendedDeviceProfileSelector] in the engine ini.
 */
public class ExtendedDeviceProfileSelector : ModuleRules
{
	public ExtendedDeviceProfileSelector(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine"
			});
	}
}
