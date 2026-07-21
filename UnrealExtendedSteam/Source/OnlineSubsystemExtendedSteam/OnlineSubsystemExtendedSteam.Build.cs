// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

using UnrealBuildTool;

public class OnlineSubsystemExtendedSteam : ModuleRules
{
	public OnlineSubsystemExtendedSteam(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDefinitions.Add("ONLINESUBSYSTEMEXTENDEDSTEAM_PACKAGE=1");

		// Flat layout (house style): expose the module root so feature-folder includes resolve.
		PublicIncludePaths.Add(ModuleDirectory);

		// The public header derives from FOnlineSubsystemImpl (OnlineSubsystem) and pulls the
		// service name / log category from ExtendedSteamShared, so both must be public dependencies.
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"OnlineSubsystem",
				"ExtendedSteamShared"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Engine",
				"OnlineSubsystemUtils"
			}
		);

		ExtendedSteamLibrary.Apply(this, Target);
	}
}
