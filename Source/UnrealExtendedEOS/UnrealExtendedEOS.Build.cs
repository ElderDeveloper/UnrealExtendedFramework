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
				"DeveloperSettings"
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"Json",
				"JsonUtilities",
				"Sockets",
				"OnlineSubsystemEOS",
				"EOSShared",
				"EOSSDK",
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
