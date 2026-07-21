// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

using System;
using System.IO;
using EpicGames.Core;
using UnrealBuildTool;

/// <summary>
/// Steamworks SDK locator for UnrealExtendedSteam.
///
/// Two SDK sources, in priority order:
///  1. Drop-in SDK: an official Steamworks SDK placed at Source/ThirdParty/ExtendedSteamLibrary/SDK
///     (the contents of the "sdk" folder of the official zip, so that SDK/public/steam/steam_api.h
///     exists). Never committed to the repository - see SDK/.gitignore.
///  2. Engine SDK: the engine's own "Steamworks" third-party module (Engine/Source/ThirdParty/Steamworks).
///
/// Sibling modules do not reference this class' module directly; they call
/// ExtendedSteamLibrary.Apply(this, Target) from their Build.cs, which wires whichever SDK source
/// is active and adds two private definitions:
///   WITH_EXTENDEDSTEAM_SDK  (0/1)  - master switch, 0 on unsupported platforms
///   ESTEAM_SDK_VERSION      (int)  - e.g. 164 for Steamworks 1.64, 0 when no SDK
/// Any module whose PUBLIC headers use these macros must itself call Apply so dependents agree
/// on the values (in early phases all SDK-gated code stays private).
/// </summary>
public class ExtendedSteamLibrary : ModuleRules
{
	public ExtendedSteamLibrary(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string SdkDir = Path.Combine(ModuleDirectory, "SDK");
		if (!HasDropInSDK(SdkDir))
		{
			return;
		}

		PublicSystemIncludePaths.Add(Path.Combine(SdkDir, "public"));

		string RedistDir = Path.Combine(SdkDir, "redistributable_bin");
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add(Path.Combine(RedistDir, "win64", "steam_api64.lib"));
			PublicDelayLoadDLLs.Add("steam_api64.dll");
			RuntimeDependencies.Add(Path.Combine(RedistDir, "win64", "steam_api64.dll"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string Dylib = Path.Combine(RedistDir, "osx", "libsteam_api.dylib");
			PublicDelayLoadDLLs.Add(Dylib);
			RuntimeDependencies.Add("$(BinaryOutputDir)/libsteam_api.dylib", Dylib);
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			string So = Path.Combine(RedistDir, "linux64", "libsteam_api.so");
			PublicAdditionalLibraries.Add(So);
			RuntimeDependencies.Add(Path.Combine("$(TargetOutputDir)", "libsteam_api.so"), So);
		}
	}

	/** Guards the "no SDK found" build warning so it prints once per UBT invocation, not once per module. */
	private static bool bWarnedNoSteamworksSDK = false;

	public static void Apply(ModuleRules Rules, ReadOnlyTargetRules Target)
	{
		int SdkVersion = 0;

		if (IsSupportedPlatform(Target))
		{
			string SourceDir = Directory.GetParent(Rules.ModuleDirectory).FullName;
			string SdkDir = Path.Combine(SourceDir, "ThirdParty", "ExtendedSteamLibrary", "SDK");

			if (HasDropInSDK(SdkDir))
			{
				Rules.PublicDependencyModuleNames.Add("ExtendedSteamLibrary");
				SdkVersion = ReadDropInVersion(SdkDir);
			}
			else
			{
				Rules.PrivateDependencyModuleNames.Add("Steamworks");
				SdkVersion = DetectEngineSDKVersion(Target);
			}

			// No drop-in SDK and no engine Steamworks module: the plugin still compiles, but with
			// WITH_EXTENDEDSTEAM_SDK=0 every Steam client/server call becomes a silent no-op. Surface
			// that at build time so a consumer never ships a "working" build where Steam does nothing.
			if (SdkVersion == 0 && !bWarnedNoSteamworksSDK)
			{
				bWarnedNoSteamworksSDK = true;
				Log.TraceWarning(
					"UnrealExtendedSteam: no Steamworks SDK found for platform " + Target.Platform.ToString() + ". " +
					"The plugin will compile with WITH_EXTENDEDSTEAM_SDK=0, which makes every Steam client/server " +
					"call a silent no-op. Drop an official Steamworks SDK into " +
					"Source/ThirdParty/ExtendedSteamLibrary/SDK (so that SDK/public/steam/steam_api.h exists), " +
					"or use an engine build that ships the Steamworks third-party module.");
			}
		}

		Rules.PrivateDefinitions.Add("WITH_EXTENDEDSTEAM_SDK=" + (SdkVersion > 0 ? "1" : "0"));
		Rules.PrivateDefinitions.Add("ESTEAM_SDK_VERSION=" + SdkVersion);
	}

	public static bool IsSupportedPlatform(ReadOnlyTargetRules Target)
	{
		return Target.Platform == UnrealTargetPlatform.Win64
			|| Target.Platform == UnrealTargetPlatform.Mac
			|| Target.Platform == UnrealTargetPlatform.Linux;
	}

	private static bool HasDropInSDK(string SdkDir)
	{
		return File.Exists(Path.Combine(SdkDir, "public", "steam", "steam_api.h"));
	}

	/// Reads SDK/VERSION.txt ("1.64" or "164"); a drop-in without one is assumed current (164).
	private static int ReadDropInVersion(string SdkDir)
	{
		try
		{
			string VersionFile = Path.Combine(SdkDir, "VERSION.txt");
			if (File.Exists(VersionFile))
			{
				string Digits = "";
				foreach (char c in File.ReadAllText(VersionFile))
				{
					if (char.IsDigit(c))
					{
						Digits += c;
					}
				}
				if (Digits.Length > 0)
				{
					return int.Parse(Digits);
				}
			}
		}
		catch (Exception)
		{
		}
		return 164;
	}

	/// Parses the engine's Steamworks SDK folder name, e.g. ".../ThirdParty/Steamworks/Steamv164" -> 164.
	private static int DetectEngineSDKVersion(ReadOnlyTargetRules Target)
	{
		try
		{
			string SteamworksDir = Path.Combine(Target.UEThirdPartySourceDirectory, "Steamworks");
			int Best = 0;
			foreach (string Dir in Directory.GetDirectories(SteamworksDir, "Steamv*"))
			{
				string Digits = "";
				foreach (char c in Path.GetFileName(Dir))
				{
					if (char.IsDigit(c))
					{
						Digits += c;
					}
				}
				int Parsed;
				if (Digits.Length > 0 && int.TryParse(Digits, out Parsed) && Parsed > Best)
				{
					Best = Parsed;
				}
			}
			return Best;
		}
		catch (Exception)
		{
			return 0;
		}
	}
}
