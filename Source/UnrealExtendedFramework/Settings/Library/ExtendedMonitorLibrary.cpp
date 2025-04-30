// Fill out your copyright notice in the Description page of Project Settings.

#include "ExtendedMonitorLibrary.h"
#include "GenericPlatform/GenericApplication.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"

int32 UExtendedMonitorLibrary::GetGameWindowMonitorIndex()
{
	if (!FSlateApplication::IsInitialized())
	{
		return 0; // Default to primary monitor if Slate is not initialized
	}

	// Get the game window
	TSharedPtr<SWindow> GameWindow = nullptr;
	if (GEngine && GEngine->GameViewport)
	{
		GameWindow = GEngine->GameViewport->GetWindow();
	}

	if (!GameWindow.IsValid())
	{
		return 0; // Default to primary monitor if window is not valid
	}

	// Get the window's position
	FVector2D WindowPosition = GameWindow->GetPositionInScreen();

	// Get display metrics
	FDisplayMetrics DisplayMetrics;
	FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);

	// Find which monitor contains this window
	for (int32 MonitorIndex = 0; MonitorIndex < DisplayMetrics.MonitorInfo.Num(); ++MonitorIndex)
	{
		const FMonitorInfo& Monitor = DisplayMetrics.MonitorInfo[MonitorIndex];
		
		// Check if window position is within this monitor's bounds
		if (WindowPosition.X >= Monitor.DisplayRect.Left && 
			WindowPosition.X < Monitor.DisplayRect.Right &&
			WindowPosition.Y >= Monitor.DisplayRect.Top && 
			WindowPosition.Y < Monitor.DisplayRect.Bottom)
		{
			return MonitorIndex;
		}
	}

	// Default to primary monitor if no match found
	for (int32 MonitorIndex = 0; MonitorIndex < DisplayMetrics.MonitorInfo.Num(); ++MonitorIndex)
	{
		if (DisplayMetrics.MonitorInfo[MonitorIndex].bIsPrimary)
		{
			return MonitorIndex;
		}
	}

	return 0;
}

void UExtendedMonitorLibrary::GetCurrentMonitorResolution(int32& OutWidth, int32& OutHeight)
{
	// Get the monitor index that the game window is on
	int32 MonitorIndex = GetGameWindowMonitorIndex();

	// Get display metrics
	FDisplayMetrics DisplayMetrics;
	FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);

	// Make sure the monitor index is valid
	if (MonitorIndex >= 0 && MonitorIndex < DisplayMetrics.MonitorInfo.Num())
	{
		OutWidth = DisplayMetrics.MonitorInfo[MonitorIndex].NativeWidth;
		OutHeight = DisplayMetrics.MonitorInfo[MonitorIndex].NativeHeight;
	}
	else
	{
		// Default to primary display resolution if monitor index is invalid
		OutWidth = DisplayMetrics.PrimaryDisplayWidth;
		OutHeight = DisplayMetrics.PrimaryDisplayHeight;
	}
}

TArray<FIntPoint> UExtendedMonitorLibrary::GetCurrentMonitorSupportedResolutions()
{
	TArray<FIntPoint> SupportedResolutions;

	// Get the monitor index that the game window is on
	int32 MonitorIndex = GetGameWindowMonitorIndex();

	// Get all supported resolutions
	UKismetSystemLibrary::GetSupportedFullscreenResolutions(SupportedResolutions);

	// Get the current monitor's resolution
	int32 MonitorWidth, MonitorHeight;
	GetCurrentMonitorResolution(MonitorWidth, MonitorHeight);

	// Filter resolutions that exceed the current monitor's capabilities
	TArray<FIntPoint> FilteredResolutions;
	for (const FIntPoint& Resolution : SupportedResolutions)
	{
		if (Resolution.X <= MonitorWidth && Resolution.Y <= MonitorHeight)
		{
			FilteredResolutions.Add(Resolution);
		}
	}

	return FilteredResolutions;
}

bool UExtendedMonitorLibrary::IsGameOnPrimaryMonitor()
{
	// Get the monitor index that the game window is on
	int32 MonitorIndex = GetGameWindowMonitorIndex();

	// Get display metrics
	FDisplayMetrics DisplayMetrics;
	FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);

	// Check if the monitor index is valid and if it's the primary monitor
	if (MonitorIndex >= 0 && MonitorIndex < DisplayMetrics.MonitorInfo.Num())
	{
		return DisplayMetrics.MonitorInfo[MonitorIndex].bIsPrimary;
	}

	return true; // Default to true if we can't determine
}