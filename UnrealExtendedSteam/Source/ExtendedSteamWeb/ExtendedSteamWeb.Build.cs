// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

using UnrealBuildTool;

public class ExtendedSteamWeb : ModuleRules
{
	public ExtendedSteamWeb(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Flat layout (house style): ModuleDirectory on the public include path so dependents
		// resolve "Core/ESteamWebLog.h" and internal files resolve their siblings.
		PublicIncludePaths.Add(ModuleDirectory);

		// Deliberately SDK-free: the Steam Web API is plain HTTPS + JSON and must stay usable
		// on dedicated servers and platforms without the Steam client.
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"DeveloperSettings"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"HTTP"
			}
		);
	}
}
