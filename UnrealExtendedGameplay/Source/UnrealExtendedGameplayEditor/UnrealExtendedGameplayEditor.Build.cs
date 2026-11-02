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
				// The curve modifier writes what FEGTurnInPlaceSolver::BuildYawTrack computes; the
				// placement editor library takes placement specs and visuals.
				"UnrealExtendedGameplay",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AnimationBlueprintLibrary",
				// Placement component visualizers and transactions.
				"UnrealEd",
				"GameplayTags",
			}
		);
	}
}
