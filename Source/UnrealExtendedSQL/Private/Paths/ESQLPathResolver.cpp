// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Paths/ESQLPathResolver.h"

#include "Paths/ESQLContentExtractor.h"
#include "Shared/ESQLSettings.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"

namespace
{
EESQLResolvedDatabaseKind ResolveKind(const EESQLDatabaseScope Scope)
{
	switch (Scope)
	{
	case EESQLDatabaseScope::Shared:
		return EESQLResolvedDatabaseKind::Shared;

	case EESQLDatabaseScope::Readonly:
		return EESQLResolvedDatabaseKind::Readonly;

	case EESQLDatabaseScope::Local:
	default:
		return EESQLResolvedDatabaseKind::Local;
	}
}
}

FString FESQLPathResolver::ResolveAbsolutePath(const FString& FilePath)
{
	FString AbsolutePath = FPaths::ConvertRelativePathToFull(FilePath);
	FPaths::NormalizeFilename(AbsolutePath);
	return AbsolutePath;
}

bool FESQLPathResolver::EnsureParentDirectoryExists(const FString& AbsolutePath)
{
	const FString Directory = FPaths::GetPath(AbsolutePath);
	if (Directory.IsEmpty())
	{
		return true;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (PlatformFile.DirectoryExists(*Directory))
	{
		return true;
	}

	return PlatformFile.CreateDirectoryTree(*Directory);
}

FESQLResolvedPath FESQLPathResolver::ResolveDatabasePathInfo(const FString& FileName, EESQLDatabaseScope Scope)
{
	const UESQLSettings* Settings = UESQLSettings::Get();
	const FString BaseDir = UESQLSettings::ResolveDatabaseDirectoryPath();

	FString FullPath;
	switch (Scope)
	{
	case EESQLDatabaseScope::Local:
		FullPath = FPaths::Combine(BaseDir, FileName);
		break;

	case EESQLDatabaseScope::Shared:
		FullPath = FPaths::Combine(
			BaseDir,
			Settings ? Settings->ServerSubdirectory : TEXT("Server"),
			FileName);
		break;

	case EESQLDatabaseScope::Readonly:
	{
		FESQLResolvedPath ReadonlyResult;
		ReadonlyResult.Kind = EESQLResolvedDatabaseKind::Readonly;
		ReadonlyResult.bReadOnly = true;

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		FString ContentPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Database"), FileName);
		ContentPath = ResolveAbsolutePath(ContentPath);
		if (!PlatformFile.FileExists(*ContentPath))
		{
			return ReadonlyResult;
		}

		FString ExtractedPath;
		FString ExtractionError;
		if (FESQLContentExtractor::EnsureExtracted(ContentPath, ExtractedPath, ExtractionError))
		{
			ReadonlyResult.AbsolutePath = ExtractedPath;
			ReadonlyResult.bWasExtracted = true;
		}
		else
		{
			ReadonlyResult.AbsolutePath = ContentPath;
			ReadonlyResult.bWasExtracted = false;
		}

		return ReadonlyResult;
	}

	default:
		FullPath = FPaths::Combine(BaseDir, FileName);
		break;
	}

	FESQLResolvedPath Result;
	Result.AbsolutePath = ResolveAbsolutePath(FullPath);
	Result.Kind = ResolveKind(Scope);
	return Result;
}

FString FESQLPathResolver::ResolveDatabasePath(const FString& FileName, EESQLDatabaseScope Scope)
{
	return ResolveDatabasePathInfo(FileName, Scope).AbsolutePath;
}

FESQLResolvedPath FESQLPathResolver::ResolveExistingDatabasePathInfo(const FString& DatabaseName, bool bAllowReadonlyContentFallback)
{
	FString FileName = DatabaseName;
	if (!FileName.EndsWith(TEXT(".db"), ESearchCase::IgnoreCase))
	{
		FileName += TEXT(".db");
	}

	FESQLResolvedPath Result = ResolveDatabasePathInfo(FileName, EESQLDatabaseScope::Local);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (PlatformFile.FileExists(*Result.AbsolutePath))
	{
		return Result;
	}

	if (!bAllowReadonlyContentFallback)
	{
		Result.AbsolutePath.Reset();
		return Result;
	}

	FString ContentPath = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Database"), FileName);
	ContentPath = ResolveAbsolutePath(ContentPath);
	if (!PlatformFile.FileExists(*ContentPath))
	{
		Result.AbsolutePath.Reset();
		return Result;
	}

	FString ExtractedPath;
	FString ExtractionError;
	if (FESQLContentExtractor::EnsureExtracted(ContentPath, ExtractedPath, ExtractionError))
	{
		Result.AbsolutePath = ExtractedPath;
		Result.Kind = EESQLResolvedDatabaseKind::Readonly;
		Result.bReadOnly = true;
		Result.bWasExtracted = true;
		return Result;
	}

	Result.AbsolutePath = ContentPath;
	Result.Kind = EESQLResolvedDatabaseKind::Readonly;
	Result.bReadOnly = true;
	Result.bWasExtracted = false;
	return Result;
}

FString FESQLPathResolver::ResolveExistingDatabasePath(const FString& DatabaseName, bool bAllowReadonlyContentFallback)
{
	return ResolveExistingDatabasePathInfo(DatabaseName, bAllowReadonlyContentFallback).AbsolutePath;
}