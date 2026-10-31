// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

using UnrealBuildTool;

/** The anim graph nodes' editor side (UncookedOnly, as anim graph node modules must be). */
public class UnrealExtendedGameplayAnimGraph : ModuleRules
{
	public UnrealExtendedGameplayAnimGraph(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				// The graph nodes' headers include the editor base classes and the runtime nodes.
				"AnimGraph",
				"AnimGraphRuntime",
				"UnrealExtendedGameplay",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"BlueprintGraph",
			}
		);
	}
}
