// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

using UnrealBuildTool;

public class ExtendedEOSEditor : ModuleRules
{
	public ExtendedEOSEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

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
				"DeveloperSettings",
				// Log category (LogExtendedEOS) and the shared settings this module reads the
				// Dev Auth Tool address from.
				"ExtendedEOSShared"
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UnrealEd",			// FEditorDelegates, editor notifications
				"Slate",
				"SlateCore",
				"ToolMenus",
				"Json",				// credentials_<env>.json parsing
				"Sockets",			// liveness probe on the tool's local port
				"Projects",			// IPluginManager — locating the bundled tool
				// FOnlineAccountStoredCredentials (Public header, exported Encrypt/Decrypt).
				// UOnlinePIESettings itself is Private in this module, so its CDO is reached
				// through reflection — see EEOSPIELoginSync.cpp.
				"OnlineSubsystemUtils"
			}
			);
	}
}
