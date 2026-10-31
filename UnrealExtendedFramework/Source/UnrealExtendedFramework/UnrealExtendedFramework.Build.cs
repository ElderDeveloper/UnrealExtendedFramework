// Copyright Moon Punch Games. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using EpicGames.Core;
using UnrealBuildTool;

/// <summary>
/// Compile-time detection of optional vendor upscaler plugins (DLSS, FSR, XeSS).
/// </summary>
public static class EFUpscalerPluginDetection
{
	// Plugins sit a few folders below a Plugins root (Plugins/<Category>/<Name>/<Name>.uplugin) and
	// never inside another plugin, so the search stays shallow and skips plugin internals. The old
	// search walked the whole project and engine trees on every build.
	private const int MaxSearchDepth = 4;

	private static readonly HashSet<string> SkippedFolders = new HashSet<string>(StringComparer.OrdinalIgnoreCase)
	{
		"Binaries", "Config", "Content", "DerivedDataCache", "Intermediate", "Resources", "Saved", "Shaders", "Source", "ThirdParty"
	};

	private static readonly Dictionary<string, bool> SearchCache = new Dictionary<string, bool>(StringComparer.OrdinalIgnoreCase);

	/// <summary>
	/// True when PluginName.uplugin exists under the project's or engine's plugin folders and the
	/// target or .uproject does not disable it.
	/// </summary>
	public static bool DoesPluginExistAndIsEnabled(ReadOnlyTargetRules Target, string EngineDirectory, string PluginName)
	{
		if (Target.DisablePlugins.Contains(PluginName))
		{
			return false;
		}

		List<string> Roots = new List<string>();
		JsonObject ProjectDescriptor = null;
		if (Target.ProjectFile != null)
		{
			string ProjectDirectory = Target.ProjectFile.Directory.FullName;
			Roots.Add(Path.Combine(ProjectDirectory, "Plugins"));

			try
			{
				ProjectDescriptor = JsonObject.Read(Target.ProjectFile);
			}
			catch (Exception)
			{
				ProjectDescriptor = null;
			}

			string[] AdditionalDirectories;
			if (ProjectDescriptor != null && ProjectDescriptor.TryGetStringArrayField("AdditionalPluginDirectories", out AdditionalDirectories))
			{
				foreach (string AdditionalDirectory in AdditionalDirectories)
				{
					Roots.Add(Path.IsPathRooted(AdditionalDirectory) ? AdditionalDirectory : Path.Combine(ProjectDirectory, AdditionalDirectory));
				}
			}
		}
		Roots.Add(Path.Combine(EngineDirectory, "Plugins"));

		if (!Roots.Any(Root => ContainsPlugin(Root, PluginName)))
		{
			return false;
		}

		JsonObject[] ProjectPlugins;
		if (ProjectDescriptor != null && ProjectDescriptor.TryGetObjectArrayField("Plugins", out ProjectPlugins))
		{
			bool bExplicitlyDisabled = ProjectPlugins.Any(Plugin =>
			{
				string Name;
				bool bEnabled;
				return Plugin.TryGetStringField("Name", out Name) && Name == PluginName
					&& Plugin.TryGetBoolField("Enabled", out bEnabled) && !bEnabled;
			});
			if (bExplicitlyDisabled)
			{
				return false;
			}
		}

		return true;
	}

	private static bool ContainsPlugin(string Root, string PluginName)
	{
		string Key = Root + "|" + PluginName;
		lock (SearchCache)
		{
			bool bCached;
			if (SearchCache.TryGetValue(Key, out bCached))
			{
				return bCached;
			}
		}

		bool bFound = Directory.Exists(Root) && SearchFolder(new DirectoryInfo(Root), PluginName + ".uplugin", 0);

		lock (SearchCache)
		{
			SearchCache[Key] = bFound;
		}
		return bFound;
	}

	private static bool SearchFolder(DirectoryInfo Folder, string PluginFileName, int Depth)
	{
		try
		{
			if (File.Exists(Path.Combine(Folder.FullName, PluginFileName)))
			{
				return true;
			}

			// Another plugin's root: plugins do not nest, so nothing below it can match.
			if (Depth >= MaxSearchDepth || Folder.EnumerateFiles("*.uplugin").Any())
			{
				return false;
			}

			foreach (DirectoryInfo Child in Folder.EnumerateDirectories())
			{
				if (SkippedFolders.Contains(Child.Name) || Child.Name.StartsWith("."))
				{
					continue;
				}
				if (SearchFolder(Child, PluginFileName, Depth + 1))
				{
					return true;
				}
			}
		}
		catch (UnauthorizedAccessException)
		{
		}
		catch (IOException)
		{
		}
		return false;
	}
}

/// <summary>
/// Core runtime module of the Extended Framework.
///
/// Public dependencies are the modules whose types appear in this module's public headers, so a
/// dependent module gets exactly what it needs to include them. Everything used only in .cpp files
/// is private. Do not add modules here "for" a dependent plugin: that plugin declares its own.
/// </summary>
public class UnrealExtendedFramework : ModuleRules
{
	public UnrealExtendedFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// The plugin's player-facing text lives in the "ExtendedFramework" string table, a CSV read at
		// module startup. It is not an asset, so a packaged build must stage the file explicitly.
		RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Localization", "ExtendedFramework.csv"), StagedFileType.UFS);

		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"DeveloperSettings",
				"InputCore",
				"EnhancedInput",
				"Slate",
				"SlateCore",
				"UMG",
				"GameplayTags",
				"GameplayTasks",
				"AIModule",
				"NavigationSystem",
				"PhysicsCore",
				"AudioMixer",
				"Json",
				"JsonUtilities",
				"XmlParser",
				// Shared base: logging, HTTP, JSON, results, async nodes, settings, credentials.
				"ExtendedFoundation"
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				// Libraries/Math: UKismetAnimationLibrary.
				"AnimGraphRuntime",
				"ApplicationCore",
				"EngineSettings",
				// Systems/Loading: blocking loading screen during map travel.
				"MoviePlayer",
				"Projects",
				"RHI",
				// ModularSettings/Settings/AudioDevice: voice capture and voice chat device selection.
				"Voice",
				"VoiceChat"
			});

		// ─────────────────────────────────────────────────────────────────────
		//  Compile-time upscaler plugin detection (DLSS, FSR, XeSS)
		//  Every WITH_* macro is defined in every configuration, so code can
		//  test them with #if without tripping undefined-identifier warnings.
		// ─────────────────────────────────────────────────────────────────────

		bool bWithDLSS = EFUpscalerPluginDetection.DoesPluginExistAndIsEnabled(Target, EngineDirectory, "DLSS");
		if (bWithDLSS)
		{
			PublicDependencyModuleNames.Add("DLSSBlueprint");
		}
		PublicDefinitions.Add("WITH_DLSS=" + (bWithDLSS ? "1" : "0"));

		// As of UE 5.7 the newest FSR plugin uses the single name "FSR"; FSR4 and FSR3 are the older names.
		bool bWithFSRGeneric = EFUpscalerPluginDetection.DoesPluginExistAndIsEnabled(Target, EngineDirectory, "FSR");
		bool bWithFSR4 = !bWithFSRGeneric && EFUpscalerPluginDetection.DoesPluginExistAndIsEnabled(Target, EngineDirectory, "FSR4");
		bool bWithFSR3 = !bWithFSRGeneric && !bWithFSR4 && EFUpscalerPluginDetection.DoesPluginExistAndIsEnabled(Target, EngineDirectory, "FSR3");
		if (bWithFSRGeneric)
		{
			PublicDependencyModuleNames.Add("FFXFSRSettings");
		}
		else if (bWithFSR4)
		{
			PublicDependencyModuleNames.Add("FFXFSR4Settings");
		}
		else if (bWithFSR3)
		{
			PublicDependencyModuleNames.Add("FFXFSR3Settings");
		}
		PublicDefinitions.Add("WITH_FSR=" + (bWithFSRGeneric || bWithFSR4 || bWithFSR3 ? "1" : "0"));
		PublicDefinitions.Add("WITH_FSR_GENERIC=" + (bWithFSRGeneric ? "1" : "0"));
		PublicDefinitions.Add("WITH_FSR4=" + (bWithFSR4 ? "1" : "0"));
		PublicDefinitions.Add("WITH_FSR3=" + (bWithFSR3 ? "1" : "0"));

		bool bWithXeSS = EFUpscalerPluginDetection.DoesPluginExistAndIsEnabled(Target, EngineDirectory, "XeSS");
		if (bWithXeSS)
		{
			PublicDependencyModuleNames.Add("XeSSBlueprint");
			PublicDependencyModuleNames.Add("XeFGBlueprint");
		}
		PublicDefinitions.Add("WITH_XESS=" + (bWithXeSS ? "1" : "0"));
	}
}
