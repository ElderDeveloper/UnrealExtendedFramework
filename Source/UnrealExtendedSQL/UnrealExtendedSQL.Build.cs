// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class UnrealExtendedSQL : ModuleRules
{
	public UnrealExtendedSQL(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		string SQLitePath = Path.Combine(PluginDirectory, "Thirdparty", "SQLite");
		PrivateIncludePaths.Add(SQLitePath);
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "Private"));

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
				"DeveloperSettings"
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

		PrivateDefinitions.Add("SQLITE_THREADSAFE=2");
		PrivateDefinitions.Add("SQLITE_ENABLE_FTS5=1");
		PrivateDefinitions.Add("SQLITE_ENABLE_JSON1=1");
		PrivateDefinitions.Add("SQLITE_ENABLE_RTREE=1");
		PrivateDefinitions.Add("SQLITE_ENABLE_DBSTAT_VTAB=1");
		PrivateDefinitions.Add("SQLITE_DEFAULT_FOREIGN_KEYS=1");
		PrivateDefinitions.Add("SQLITE_DEFAULT_WAL_SYNCHRONOUS=1");
		PrivateDefinitions.Add("SQLITE_OMIT_DEPRECATED=1");
		PrivateDefinitions.Add("SQLITE_OMIT_LOAD_EXTENSION=1");
		PrivateDefinitions.Add("SQLITE_API=");
	}
}

