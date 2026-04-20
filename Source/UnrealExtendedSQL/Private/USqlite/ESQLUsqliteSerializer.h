// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Private/USqlite/ESQLUsqliteTypes.h"

class FESQLDatabase;
class UESQLTableAsset;

class FESQLUsqliteSerializer
{
public:
	static bool IsProjectRootPath(const FString& Path);
	static FString GetProjectRootForDatabasePath(const FString& DatabasePath);
	static FString GetDerivedDatabasePath(const FString& ProjectRoot, const FString& ProjectHash);

	static bool LoadProject(const FString& ProjectRoot, FESQLUsqliteProject& OutProject, FString& OutError);
	static bool SaveProject(const FESQLUsqliteProject& Project, const FString& ProjectRoot, FString& OutError);
	static bool ComputeProjectHash(const FESQLUsqliteProject& Project, FString& OutHash, FString& OutError);

	static bool ExportDatabaseToProject(
		TSharedPtr<FESQLDatabase> Database,
		const FString& ProjectRoot,
		FString& OutError,
		const UESQLTableAsset* MetadataAsset = nullptr);

	static bool SyncDatabaseAndProject(
		const FString& DatabasePath,
		const FString& ProjectRoot,
		FString& OutError,
		const UESQLTableAsset* MetadataAsset = nullptr);
};