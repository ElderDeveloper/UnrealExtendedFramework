// Copyright Moon Punch Games. All Rights Reserved.

// UEFFileLibrary: Screenshot capture and loading.

#include "EFFileLibrary.h"

#include "Engine/Texture2D.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

bool UEFFileLibrary::TakeScreenShot(FString Filename, FString& Path, bool PrefixTimestamp, bool ShowUI)
{
	FText ErrorFilename;
	Filename = FPaths::GetCleanFilename(Filename);
	if (!FFileHelper::IsFilenameValidForSaving(Filename, ErrorFilename))
	{
		return false;
	}
	FString FinalFilename = (PrefixTimestamp ? (FDateTime::Now().ToString(TEXT("%Y_%m_%d__%H_%M_%S__"))) : "") +
		Filename;
	FScreenshotRequest::Reset();
	FScreenshotRequest::RequestScreenshot(FinalFilename, ShowUI, false);
	if (FScreenshotRequest::IsScreenshotRequested())
	{
		Path = FScreenshotRequest::GetFilename();
		return true;
	}
	return false;
}

UTexture2D* UEFFileLibrary::LoadScreenshot(FString Path, bool& Success)
{
	IPlatformFile& file = FPlatformFileManager::Get().GetPlatformFile();
	if (file.FileExists(*Path))
	{
		Success = true;
		UTexture2D* Tex = FImageUtils::ImportFileAsTexture2D(Path);
		if (Tex)
		{
			return Tex;
		}
	}
	Success = false;
	return nullptr;
}
