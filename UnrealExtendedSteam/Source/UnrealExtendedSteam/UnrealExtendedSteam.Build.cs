// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

using UnrealBuildTool;

public class UnrealExtendedSteam : ModuleRules
{
	public UnrealExtendedSteam(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Several subsystem .cpp files declare identically-named anonymous-namespace
		// helpers (e.g. ParseIPv4 in Matchmaking + MatchmakingServers). Those are fine
		// as separate translation units but collide (ODR) when merged into a unity blob,
		// which happens here because the plugin's files are outside this project's
		// adaptive-unity working set. Disable unity for this module to keep them isolated.
		bUseUnity = false;

		// Flat layout (house style): ModuleDirectory on the public include path so dependents
		// resolve "Shared/ESteamBlueprintLibrary.h" and internal files resolve their siblings.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"ExtendedSteamShared",
				"OnlineSubsystem"
			}
		);

		// AudioExtensions provides IAudioProxyDataFactory, the base of USoundWave used by
		// UESteamVoiceSoundWave (USoundWaveProcedural) for Steam voice playback.
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AudioExtensions"
			}
		);

		ExtendedSteamLibrary.Apply(this, Target);
	}
}
