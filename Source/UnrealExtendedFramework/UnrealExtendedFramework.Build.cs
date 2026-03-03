// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using System.Linq;
using System.IO;
using UnrealBuildTool;

public static class EFUpscalerPluginDetection
{
	/// <summary>
	/// Checks if a third-party plugin exists in project or engine directories and is not explicitly disabled.
	/// Ported from BlueshiftIQ Build.cs for compile-time upscaler detection (DLSS, FSR, XeSS).
	/// </summary>
	public static bool DoesPluginExistAndIsEnabled(ReadOnlyTargetRules Target, string PluginName)
	{
		// Project explicitly disables plugin?
		if (Target.DisablePlugins.Contains(PluginName))
		{
			return false;
		}

		// Look for the plugin .uplugin in project plugins recursively
		bool bFound = false;
		if (Target.ProjectFile != null)
		{
			var projectDir = Target.ProjectFile.Directory;
			bFound = Directory.EnumerateFiles(projectDir.FullName, PluginName + ".uplugin", SearchOption.AllDirectories).Any();
		}

		// If not found, look in engine plugins recursively
		if (bFound == false)
		{
			string EngineDir = Target.RelativeEnginePath;
			bFound = Directory.EnumerateFiles(EngineDir, PluginName + ".uplugin", SearchOption.AllDirectories).Any();
		}

		// Check project .uproject for explicit disable
		if (bFound && Target.ProjectFile != null)
		{
			var ProjectJson = JsonObject.Read(Target.ProjectFile);
			if (ProjectJson.TryGetObjectArrayField("Plugins", out var Plugins))
			{
				bool bExplicitlyDisabled = Plugins.Any(p =>
					p.TryGetStringField("Name", out var Name) && Name == PluginName
					&& p.TryGetBoolField("Enabled", out var bEnabled) && bEnabled == false
				);

				if (bExplicitlyDisabled)
				{
					return false;
				}
			}
		}

		return bFound;
	}
}

public class UnrealExtendedFramework : ModuleRules
{
	public UnrealExtendedFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"EnhancedInput",
				"DeveloperSettings",
				"Slate",
				"SlateCore",
				"UMG",
				"PreLoadScreen",
				"RenderCore",
				"ApplicationCore",
				"AIModule",
				"Niagara",
				"NavigationSystem",
				"GameplayAbilities",
				"GameplayTags",
				"GameplayTasks",
				"XmlParser",
				"EngineSettings",
				"OnlineSubsystem",
				"OnlineSubsystemUtils",
				"OnlineSubsystemNull",
				"Projects"

				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"GameplayTags",
				"GameplayTasks",
				"JsonUtilities",
				"Json",
				"XmlParser",
				"ApplicationCore", "AnimGraphRuntime", "Niagara",
				"PhysicsCore",
				"RHI"
			}
			);
		
		
		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"EditorStyle"
				});
		}

		// ─────────────────────────────────────────────────────────────────────
		//  Compile-time upscaler plugin detection (DLSS, FSR, XeSS)
		// ─────────────────────────────────────────────────────────────────────

		// Detect DLSS
		if (EFUpscalerPluginDetection.DoesPluginExistAndIsEnabled(Target, "DLSS"))
		{
			PublicDependencyModuleNames.Add("DLSSBlueprint");
			PublicDefinitions.Add("WITH_DLSS=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_DLSS=0");
		}

		// Detect FSR — as of UE 5.7 the newest FSR plugin uses a single "FSR" name
		if (EFUpscalerPluginDetection.DoesPluginExistAndIsEnabled(Target, "FSR"))
		{
			PublicDependencyModuleNames.Add("FFXFSRSettings");
			PublicDefinitions.Add("WITH_FSR=1");
			PublicDefinitions.Add("WITH_FSR_GENERIC=1");
		}
		else if (EFUpscalerPluginDetection.DoesPluginExistAndIsEnabled(Target, "FSR4"))
		{
			PublicDependencyModuleNames.Add("FFXFSR4Settings");
			PublicDefinitions.Add("WITH_FSR4=1");
			PublicDefinitions.Add("WITH_FSR_GENERIC=0");
		}
		else if (EFUpscalerPluginDetection.DoesPluginExistAndIsEnabled(Target, "FSR3"))
		{
			PublicDependencyModuleNames.Add("FFXFSR3Settings");
			PublicDefinitions.Add("WITH_FSR3=1");
			PublicDefinitions.Add("WITH_FSR_GENERIC=0");
		}
		else
		{
			PublicDefinitions.Add("WITH_FSR4=0");
			PublicDefinitions.Add("WITH_FSR3=0");
			PublicDefinitions.Add("WITH_FSR_GENERIC=0");
		}

		// Detect XeSS
		if (EFUpscalerPluginDetection.DoesPluginExistAndIsEnabled(Target, "XeSS"))
		{
			PublicDependencyModuleNames.Add("XeSSBlueprint");
			PublicDependencyModuleNames.Add("XeFGBlueprint");
			PublicDefinitions.Add("WITH_XESS=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_XESS=0");
		}
	}
}
