// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class UnrealExtendedEOS : ModuleRules
{
	public UnrealExtendedEOS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// EOS SDK Include path from Thirdparty
		string ThirdPartyPath = Path.Combine(ModuleDirectory, "..", "..", "Thirdparty", "EOS", "SDK", "Include");

		PublicIncludePaths.AddRange(
			new string[] {
				ModuleDirectory
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				ThirdPartyPath
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
				"InputCore",
				"OnlineSubsystem",
				"OnlineSubsystemUtils",
				"DeveloperSettings",
				// Base module: shared types, settings, UEEOSSubsystem base class, log category.
				// Public because the subsystem public headers derive from / expose these.
				"ExtendedEOSShared",
				// Public because the subsystem headers expose WITH_EOS_SDK-gated raw SDK types
				// (handle maps, id members). Dependent modules (Blueprints, Testing) that include
				// those headers must see WITH_EOS_SDK=1 and the SDK include path consistently, or
				// they hit C4668 / an ODR mismatch on the class layout.
				"EOSShared",
				"EOSSDK"
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Slate",
				"SlateCore",
				"Json",
				"JsonUtilities",
				"Sockets",
				"OnlineSubsystemEOS",
				"VoiceChat",
				"UnrealExtendedFramework"
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
			}
			);
	}
}
