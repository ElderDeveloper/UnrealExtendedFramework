// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESQLTypes.h"

enum class EESQLResolvedDatabaseKind : uint8
{
	Local,
	Shared,
	Readonly
};

struct FESQLResolvedPath
{
	FString AbsolutePath;
	EESQLResolvedDatabaseKind Kind = EESQLResolvedDatabaseKind::Local;
	bool bReadOnly = false;
	bool bWasExtracted = false;

	bool IsValid() const
	{
		return !AbsolutePath.IsEmpty();
	}
};

class FESQLPathResolver
{
public:
	static FString ResolveAbsolutePath(const FString& FilePath);
	static bool EnsureParentDirectoryExists(const FString& AbsolutePath);
	static FESQLResolvedPath ResolveDatabasePathInfo(const FString& FileName, EESQLDatabaseScope Scope);
	static FString ResolveDatabasePath(const FString& FileName, EESQLDatabaseScope Scope);
	static FESQLResolvedPath ResolveExistingDatabasePathInfo(const FString& DatabaseName, bool bAllowReadonlyContentFallback = true);
	static FString ResolveExistingDatabasePath(const FString& DatabaseName, bool bAllowReadonlyContentFallback = true);
};