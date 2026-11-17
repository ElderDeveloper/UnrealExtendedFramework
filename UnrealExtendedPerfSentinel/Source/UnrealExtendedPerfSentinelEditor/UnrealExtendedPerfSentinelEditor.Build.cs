// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

using System.IO;
using EpicGames.Core;
using UnrealBuildTool;

public class UnrealExtendedPerfSentinelEditor : ModuleRules
{
	public UnrealExtendedPerfSentinelEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"UnrealExtendedPerfSentinel",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"InputCore",
			"ApplicationCore",
			"Slate",
			"SlateCore",
			"EditorStyle",
			"ToolMenus",
			"DesktopPlatform",
			"Settings",
			"Json",
			"JsonUtilities",
			"Projects",
			"TraceAnalysis",
			"TraceServices",
		});

		// The agent toolset builds on Unreal's ToolsetRegistry plugin, which first ships in UE 5.8.
		// Its UCLASSes live outside this folder, so UHT and the compiler only see them where the
		// registry exists; an #if around a UCLASS is not something UHT accepts. The .uplugin lists
		// ToolsetRegistry as optional for the same reason.
		bool bWithToolsetRegistry = Target.Version.MajorVersion > 5 || (Target.Version.MajorVersion == 5 && Target.Version.MinorVersion >= 8);
		string ToolsetRegistryDirectory = Path.Combine(PluginDirectory, "Source", "UnrealExtendedPerfSentinelToolsetRegistry");
		bWithToolsetRegistry = bWithToolsetRegistry && ConditionalAddModuleDirectory(new DirectoryReference(ToolsetRegistryDirectory));
		if (bWithToolsetRegistry)
		{
			// The two folders include each other's headers.
			PrivateIncludePaths.Add(ModuleDirectory);
			PrivateIncludePaths.Add(ToolsetRegistryDirectory);
			PublicDependencyModuleNames.Add("ToolsetRegistry");
		}
		PrivateDefinitions.Add("PERFSENTINEL_WITH_TOOLSET_REGISTRY=" + (bWithToolsetRegistry ? "1" : "0"));
	}
}
