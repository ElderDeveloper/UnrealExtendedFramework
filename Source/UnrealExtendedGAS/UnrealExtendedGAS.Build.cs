using UnrealBuildTool;

public class UnrealExtendedGAS : ModuleRules
{
	public UnrealExtendedGAS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(new string[] {
			ModuleDirectory
		});

		PrivateIncludePaths.AddRange(new string[] {
			ModuleDirectory
		});

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"UMG"
		});
	}
}
