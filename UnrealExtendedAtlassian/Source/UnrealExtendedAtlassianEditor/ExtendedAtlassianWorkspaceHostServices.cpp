// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianWorkspaceHostServices.h"

#include "ExtendedAtlassianEditorTargetService.h"
#include "ExtendedAtlassianScreenshot.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

double FExtendedAtlassianSystemWorkspaceHostServices::NowSeconds() const
{
	return FPlatformTime::Seconds();
}

bool FExtendedAtlassianSystemWorkspaceHostServices::ShouldReduceMotion() const
{
#if PLATFORM_WINDOWS
	BOOL bClientAreaAnimationEnabled = 1;
	if (::SystemParametersInfo(
			SPI_GETCLIENTAREAANIMATION,
			0,
			&bClientAreaAnimationEnabled,
			0))
	{
		return bClientAreaAnimationEnabled == 0;
	}
#endif
	return false;
}

bool FExtendedAtlassianSystemWorkspaceHostServices::ShouldUseHighContrast() const
{
#if PLATFORM_WINDOWS
	HIGHCONTRAST HighContrast = {};
	HighContrast.cbSize = sizeof(HIGHCONTRAST);
	if (::SystemParametersInfo(
			SPI_GETHIGHCONTRAST,
			sizeof(HIGHCONTRAST),
			&HighContrast,
			0))
	{
		return (HighContrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
	}
#endif
	return false;
}

bool FExtendedAtlassianSystemWorkspaceHostServices::CaptureViewport(
	TArray<uint8>& OutPngData,
	FIntPoint& OutSize)
{
	return FExtendedAtlassianScreenshot::CaptureActiveViewport(
		OutPngData,
		OutSize);
}

FExtendedAtlassianCapturedContext
FExtendedAtlassianSystemWorkspaceHostServices::CaptureContext()
{
	return FExtendedAtlassianContextCapture::Capture();
}

void FExtendedAtlassianSystemWorkspaceHostServices::CopyText(
	const FString& Text)
{
	FPlatformApplicationMisc::ClipboardCopy(*Text);
}

void FExtendedAtlassianSystemWorkspaceHostServices::OpenExternal(
	const FString& Url)
{
	if (Url.StartsWith(TEXT("https://"))
		|| Url.StartsWith(TEXT("http://")))
	{
		FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
		return;
	}
	FPlatformProcess::LaunchFileInDefaultExternalApplication(*Url);
}

bool FExtendedAtlassianSystemWorkspaceHostServices::ResolveCurrentTarget(
	EExtendedAtlassianPinKind Kind,
	const FString& SelectedPageId,
	const FString& SelectedPageTitle,
	FExtendedAtlassianPinTarget& OutTarget,
	FText& OutError)
{
	return FExtendedAtlassianEditorTargetService::ResolveCurrentTarget(
		Kind,
		SelectedPageId,
		SelectedPageTitle,
		OutTarget,
		OutError);
}

bool FExtendedAtlassianSystemWorkspaceHostServices::RevealTarget(
	const FExtendedAtlassianPinTarget& Target,
	FText& OutError)
{
	return FExtendedAtlassianEditorTargetService::RevealUnrealTarget(
		Target,
		OutError);
}
