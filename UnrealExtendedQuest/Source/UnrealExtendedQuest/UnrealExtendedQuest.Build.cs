// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class UnrealExtendedQuest : ModuleRules
{
	public UnrealExtendedQuest(ReadOnlyTargetRules Target) : base(Target)
	{
		// Enable IWYU
		// https://docs.unrealengine.com/latest/INT/Programming/UnrealBuildSystem/IWYUReferenceGuide/index.html
		// https://docs.unrealengine.com/latest/INT/Programming/UnrealBuildSystem/Configuration/
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

#if UE_5_2_OR_LATER
		IWYUSupport = IWYUSupport.Full;
#else
		bEnforceIWYU = true;
#endif

		// bUseUnity = false;

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"GameplayTags",
				"Json",
				"JsonUtilities",
				"NetCore" // FFastArraySerializer for snapshot replication
				// ... add other public dependencies that you statically link with here ...
			});


		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"CoreUObject",
				"Engine",
				"CoreOnline", // FUniqueNetIdRepl for stable player-scoped fact save keys
				"Projects", // IPluginManager

				// UI
				"SlateCore",
				"Slate",
				"InputCore"
				// ... add private dependencies that you statically link with here ...
			});

		// Add MessageLog support
		if (Target.bBuildDeveloperTools)
		{
			PrivateDependencyModuleNames.Add("MessageLog");
		}

		// We need this dependency when the QuestPlugin works in the editor mode/built with editor
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("EditorWidgets");
			PrivateDependencyModuleNames.Add("UnrealEd");
			PrivateDependencyModuleNames.Add("EditorStyle");

			// Accessing the menu
			PrivateDependencyModuleNames.Add("WorkspaceMenuStructure");
		}

		// Add GameplayDebugger functionality if not 'Shipping' or 'Test' Target.
		if (Target.bBuildDeveloperTools ||
			(Target.Configuration != UnrealTargetConfiguration.Shipping && Target.Configuration != UnrealTargetConfiguration.Test))
		{
			PrivateDependencyModuleNames.Add("GameplayDebugger");
			PublicDefinitions.Add("WITH_GAMEPLAY_DEBUGGER=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_GAMEPLAY_DEBUGGER=0");
		}

#if UE_4_26_OR_LATER
		PrivateDependencyModuleNames.Add("DeveloperSettings");
#endif

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				// ... add any modules that your module loads dynamically here ...
			});
	}
}
