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
		// ExtendedFoundation is public: Core/ESteamWebSettings.h derives from UEFDeveloperSettings.
		// Requests go through its FEFHttpOperation (Core/ESteamWebClient.cpp); HTTP stays listed
		// because that header includes the engine's HTTP interfaces.
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"DeveloperSettings",
				"ExtendedFoundation"
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
