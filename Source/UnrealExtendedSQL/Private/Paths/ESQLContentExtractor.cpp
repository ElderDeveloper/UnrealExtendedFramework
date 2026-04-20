// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Paths/ESQLContentExtractor.h"

#include "HAL/PlatformFileManager.h"
#include "IO/IoHash.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
FString ComputeFileHash(const FString& AbsoluteSourcePath, FString& OutError)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *AbsoluteSourcePath))
	{
		OutError = FString::Printf(TEXT("Failed to read database payload '%s'."), *AbsoluteSourcePath);
		return FString();
	}

	return LexToString(FIoHash::HashBuffer(Bytes.GetData(), Bytes.Num()));
}
}

bool FESQLContentExtractor::EnsureExtracted(const FString& SourcePath, FString& OutExtractedPath, FString& OutError)
{
	OutExtractedPath.Reset();
	OutError.Reset();

	FString AbsoluteSourcePath = FPaths::ConvertRelativePathToFull(SourcePath);
	FPaths::NormalizeFilename(AbsoluteSourcePath);

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.FileExists(*AbsoluteSourcePath))
	{
		OutError = FString::Printf(TEXT("Readonly database payload does not exist: %s"), *AbsoluteSourcePath);
		return false;
	}

	const FString ContentHash = ComputeFileHash(AbsoluteSourcePath, OutError);
	if (ContentHash.IsEmpty())
	{
		return false;
	}

	const FString CacheDirectory = FPaths::Combine(GetContentCacheRoot(), ContentHash);
	const FString ExtractedPath = FPaths::Combine(CacheDirectory, FPaths::GetCleanFilename(AbsoluteSourcePath));
	FString NormalizedExtractedPath = FPaths::ConvertRelativePathToFull(ExtractedPath);
	FPaths::NormalizeFilename(NormalizedExtractedPath);

	if (PlatformFile.FileExists(*NormalizedExtractedPath))
	{
		OutExtractedPath = NormalizedExtractedPath;
		return true;
	}

	if (!PlatformFile.CreateDirectoryTree(*CacheDirectory))
	{
		OutError = FString::Printf(TEXT("Failed to create content cache directory '%s'."), *CacheDirectory);
		return false;
	}

	const FString TempPath = NormalizedExtractedPath + TEXT(".tmp");
	if (PlatformFile.FileExists(*TempPath))
	{
		PlatformFile.DeleteFile(*TempPath);
	}

	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *AbsoluteSourcePath))
	{
		OutError = FString::Printf(TEXT("Failed to load database payload '%s' for extraction."), *AbsoluteSourcePath);
		return false;
	}

	if (!FFileHelper::SaveArrayToFile(Bytes, *TempPath))
	{
		OutError = FString::Printf(TEXT("Failed to stage extracted database '%s'."), *TempPath);
		return false;
	}

	if (PlatformFile.FileExists(*NormalizedExtractedPath))
	{
		PlatformFile.DeleteFile(*TempPath);
		OutExtractedPath = NormalizedExtractedPath;
		return true;
	}

	if (!PlatformFile.MoveFile(*NormalizedExtractedPath, *TempPath))
	{
		if (PlatformFile.FileExists(*NormalizedExtractedPath))
		{
			PlatformFile.DeleteFile(*TempPath);
			OutExtractedPath = NormalizedExtractedPath;
			return true;
		}

		PlatformFile.DeleteFile(*TempPath);
		OutError = FString::Printf(TEXT("Failed to finalize extracted database '%s'."), *NormalizedExtractedPath);
		return false;
	}

	OutExtractedPath = NormalizedExtractedPath;
	return true;
}

FString FESQLContentExtractor::GetContentCacheRoot()
{
	FString ContentCacheRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("ExtendedSQL"), TEXT("ContentCache"));
	ContentCacheRoot = FPaths::ConvertRelativePathToFull(ContentCacheRoot);
	FPaths::NormalizeDirectoryName(ContentCacheRoot);
	return ContentCacheRoot;
}