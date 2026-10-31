// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

using UnrealBuildTool;

/// <summary>
/// Unreal Extended Save runtime module: save slots, saveable actors and objects, migrations,
/// autosave and cloud sync through the foundation's platform layer.
///
/// Public dependencies are the modules whose types appear in this module's public headers.
/// </summary>
public class ExtendedSave : ModuleRules
{
	public ExtendedSave(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				// Message channels (Extended.Save.*) are native gameplay tags.
				"GameplayTags",
				"ExtendedFoundation"
			});
	}
}
