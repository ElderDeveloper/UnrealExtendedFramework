// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

using UnrealBuildTool;

public class ExtendedEOSShared : ModuleRules
{
	public ExtendedEOSShared(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Flat layout (matches the main module): ModuleDirectory on the public include path so
		// dependents resolve "Shared/EEOSTypes.h" and internal .cpp files resolve their siblings.
		PublicIncludePaths.AddRange(
			new string[]
			{
				ModuleDirectory
			}
			);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"OnlineSubsystem",
				"OnlineSubsystemUtils",
				"DeveloperSettings"
			}
			);

		// Private: only EEOSSubsystem.cpp touches the raw SDK (platform handle via IEOSSDKManager).
		// The public Shared headers are SDK-macro-clean, so dependents don't inherit the SDK.
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"EOSShared",
				"EOSSDK"
			}
			);
	}
}
