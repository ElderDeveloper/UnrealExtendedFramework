// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class UnrealExtendedSQL : ModuleRules
{
	public UnrealExtendedSQL(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// ── SQLite — use UE's built-in SQLiteCore module ──────────────────
		// We previously bundled the SQLite amalgamation, but UE ships its own
		// SQLiteCore module with SQLiteEmbedded.c. Compiling both causes
		// duplicate symbol linker errors.
		string SQLitePath = Path.Combine(PluginDirectory, "Thirdparty", "SQLite");
		PrivateIncludePaths.Add(SQLitePath);

		PublicIncludePaths.AddRange(
			new string[]
			{
				ModuleDirectory
			}
			);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
				"DeveloperSettings",
				"SQLiteCore"
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"GameplayTags",
				"Json",
				"JsonUtilities"
			}
			);

		// SQLite compile definitions (still needed for our code that uses sqlite3.h)
		PublicDefinitions.Add("SQLITE_THREADSAFE=1");
		PublicDefinitions.Add("SQLITE_ENABLE_FTS5=1");
		PublicDefinitions.Add("SQLITE_ENABLE_JSON1=1");
		PublicDefinitions.Add("SQLITE_ENABLE_COLUMN_METADATA=1");
	}
}

