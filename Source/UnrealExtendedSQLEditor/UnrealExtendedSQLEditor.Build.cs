// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

using UnrealBuildTool;

public class UnrealExtendedSQLEditor : ModuleRules
{
	public UnrealExtendedSQLEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Allow subdirectories (TableAssetEditor/, Validation/, etc.) to
		// include headers relative to the module root directory.
		PrivateIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"Engine",
			"UnrealExtendedSQL"     // Runtime module (UESQLTableAsset, FESQLDatabase, etc.)
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"BlueprintGraph",       // UK2Node, UEdGraphSchema_K2, node spawners
			"GraphEditor",         // Editor graph node integration
			"KismetCompiler",      // FKismetCompilerContext, compiler expansion support
			"KismetWidgets",       // Blueprint graph/editor widget support
			"Slate",
			"SlateCore",
			"UnrealEd",             // FAssetEditorToolkit, FAssetTypeActions
			"ContentBrowser",       // Context menu extenders
			"InputCore",
			"ApplicationCore",      // FPlatformApplicationMisc (clipboard)
			"EditorFramework",      // FAppStyle (UE 5.1+)
			"ToolMenus",
			"PropertyEditor",       // Details panel, IStructureDetailsView
			"AssetTools",           // IAssetTools, RegisterAssetTypeActions
			"Json",                 // Layout data persistence
			"DesktopPlatform",      // File open/save dialogs (CSV import/export)
			"StructUtils"           // UUserDefinedStruct
		});
	}
}
