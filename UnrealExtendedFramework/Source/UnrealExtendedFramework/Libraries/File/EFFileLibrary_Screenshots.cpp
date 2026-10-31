// Copyright Moon Punch Games. All Rights Reserved.

// UEFFileLibrary: Screenshot capture and loading.

#include "EFFileLibrary.h"

#include "EFFileLibraryPrivate.h"
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
	// The file name carries no directory, so the screenshot lands in the screenshot directory (under
	// Saved). Check that destination before the request: refusing afterwards would leave the request
	// state (and the hidden screen messages of a capture without UI) behind.
	if (!EFFileLibraryAccess::CheckWrite(TEXT("UEFFileLibrary::TakeScreenShot"), FPaths::Combine(FPaths::ScreenShotDir(), FinalFilename)))
	{
		return false;
	}
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
	if (!EFFileLibraryAccess::CheckRead(TEXT("UEFFileLibrary::LoadScreenshot"), Path))
	{
		Success = false;
		return nullptr;
	}
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
