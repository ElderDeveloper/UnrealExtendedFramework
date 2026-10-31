// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

using UnrealBuildTool;

public class UnrealExtendedGameplayEditor : ModuleRules
{
	public UnrealExtendedGameplayEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"AnimationModifiers",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AnimationBlueprintLibrary",
			}
		);
	}
}
