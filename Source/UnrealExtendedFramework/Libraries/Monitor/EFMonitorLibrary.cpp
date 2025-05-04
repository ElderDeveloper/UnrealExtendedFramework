// Fill out your copyright notice in the Description page of Project Settings.


#include "EFMonitorLibrary.h"
#include "GenericPlatform/GenericApplication.h"
#include "Kismet/KismetSystemLibrary.h"

FDisplayInfo UEFMonitorLibrary::GetMonitorInfoByIndex(const int32 Index)
{
	FDisplayMetrics* DisplayMetrics;
	DisplayMetrics = new(FDisplayMetrics);
	DisplayMetrics->RebuildDisplayMetrics(*DisplayMetrics);
	FDisplayInfo mi;
	mi.Name = DisplayMetrics->MonitorInfo[Index].Name;
	mi.ID = DisplayMetrics->MonitorInfo[Index].ID;
	mi.NativeWidth = DisplayMetrics->MonitorInfo[Index].NativeWidth;
	mi.NativeHeight = DisplayMetrics->MonitorInfo[Index].NativeHeight;
	mi.MaxResolution = DisplayMetrics->MonitorInfo[Index].MaxResolution;
	if (mi.MaxResolution.X == 0 || mi.MaxResolution.Y == 0)
	{
		mi.MaxResolution.X = mi.NativeWidth;
		mi.MaxResolution.Y = mi.NativeHeight;
	}
	mi.DisplayRect.Left = DisplayMetrics->MonitorInfo[Index].DisplayRect.Left;
	mi.DisplayRect.Top = DisplayMetrics->MonitorInfo[Index].DisplayRect.Top;
	mi.DisplayRect.Right = DisplayMetrics->MonitorInfo[Index].DisplayRect.Right;
	mi.DisplayRect.Bottom = DisplayMetrics->MonitorInfo[Index].DisplayRect.Bottom;
	mi.WorkArea.Left = DisplayMetrics->MonitorInfo[Index].WorkArea.Left;
	mi.WorkArea.Top = DisplayMetrics->MonitorInfo[Index].WorkArea.Top;
	mi.WorkArea.Right = DisplayMetrics->MonitorInfo[Index].WorkArea.Right;
	mi.WorkArea.Bottom = DisplayMetrics->MonitorInfo[Index].WorkArea.Bottom;
	mi.bIsPrimary = DisplayMetrics->MonitorInfo[Index].bIsPrimary;
	mi.DPI = DisplayMetrics->MonitorInfo[Index].DPI;
	delete DisplayMetrics;
	return mi;
}

int32 UEFMonitorLibrary::GetNAttachedMonitors()
{
	FDisplayMetrics* DisplayMetrics;
	DisplayMetrics = new(FDisplayMetrics);
	DisplayMetrics->RebuildDisplayMetrics(*DisplayMetrics);
	int32 Num = DisplayMetrics->MonitorInfo.Num();
	delete DisplayMetrics;
	return Num;
}

EMonitorAspectRatio UEFMonitorLibrary::GetMonitorAspectRatio(int32 Index, float& Ratio, bool& IsWide)
{
	FDisplayMetrics* DisplayMetrics;
	DisplayMetrics = new(FDisplayMetrics);
	DisplayMetrics->RebuildDisplayMetrics(*DisplayMetrics);
	int32 width = DisplayMetrics->MonitorInfo[Index].NativeWidth;
	int32 height = DisplayMetrics->MonitorInfo[Index].NativeHeight;
	delete DisplayMetrics;
	EMonitorAspectRatio aspRatio = EMonitorAspectRatio::Custom;
	Ratio = (float)width / height;
	float asp = ((float)width / 4.0f) * 3.0f;
	if ((int32)asp == height)
	{
		aspRatio = EMonitorAspectRatio::Aspect4_3;
		IsWide = false;
	}
	asp = ((float)width / 5.0f) * 4.0f;
	if ((int32)asp == height)
	{
		aspRatio = EMonitorAspectRatio::Aspect5_4;
		IsWide = false;
	}
	asp = ((float)width / 16.0f) * 9.0f;
	if ((int32)asp == height)
	{
		aspRatio = EMonitorAspectRatio::Aspect16_9;
		IsWide = true;
	}
	asp = ((float)width / 16.0f) * 10.0f;
	if ((int32)asp == height)
	{
		aspRatio = EMonitorAspectRatio::Aspect16_10;
		IsWide = true;
	}
	if (aspRatio == EMonitorAspectRatio::Custom)
	{
		if (Ratio > 1.334f)
		{
			IsWide = true;
		}
		else
		{
			IsWide = false;
		}
	}
	return aspRatio;
}

int32 UEFMonitorLibrary::GetPrimaryMonitorIndex()
{
	FDisplayMetrics* DisplayMetrics;
	DisplayMetrics = new(FDisplayMetrics);
	DisplayMetrics->RebuildDisplayMetrics(*DisplayMetrics);
	int32 Primary = -1;
	for (int32 i = 0; i < DisplayMetrics->MonitorInfo.Num(); i++)
	{
		if (DisplayMetrics->MonitorInfo[i].bIsPrimary)
		{
			Primary = i;
			break;
		}
	}
	delete DisplayMetrics;
	return Primary;
}

FIntPoint UEFMonitorLibrary::GetMaxMonitorResolution(int32 Index)
{
	FDisplayMetrics* DisplayMetrics;
	DisplayMetrics = new(FDisplayMetrics);
	DisplayMetrics->RebuildDisplayMetrics(*DisplayMetrics);
	FIntPoint maxRes = DisplayMetrics->MonitorInfo[Index].MaxResolution;
	if (maxRes.X == 0 || maxRes.Y == 0)
	{
		maxRes.X = DisplayMetrics->MonitorInfo[Index].NativeWidth;
		maxRes.Y = DisplayMetrics->MonitorInfo[Index].NativeHeight;
	}
	delete DisplayMetrics;
	return maxRes;
}

void UEFMonitorLibrary::GetMonitorSafeAreas(FRect& TitleSafeArea, float& TitleSafeRatio, FRect& ActionSafeArea)
{
	FDisplayMetrics* DisplayMetrics;
	DisplayMetrics = new(FDisplayMetrics);
	DisplayMetrics->RebuildDisplayMetrics(*DisplayMetrics);
	TitleSafeArea.Left = DisplayMetrics->TitleSafePaddingSize.X;
	TitleSafeArea.Top = DisplayMetrics->TitleSafePaddingSize.Y;
	TitleSafeArea.Right = DisplayMetrics->TitleSafePaddingSize.Z;
	TitleSafeArea.Bottom = DisplayMetrics->TitleSafePaddingSize.W;
	TitleSafeRatio = DisplayMetrics->GetDebugTitleSafeZoneRatio();
	ActionSafeArea.Left = DisplayMetrics->ActionSafePaddingSize.X;
	ActionSafeArea.Top = DisplayMetrics->ActionSafePaddingSize.Y;
	ActionSafeArea.Right = DisplayMetrics->ActionSafePaddingSize.Z;
	ActionSafeArea.Bottom = DisplayMetrics->ActionSafePaddingSize.W;
	delete DisplayMetrics;
}

void UEFMonitorLibrary::GetPrimaryDisplayResolution(int32& Width, int32& Height)
{
	FDisplayMetrics* DisplayMetrics;
	DisplayMetrics = new(FDisplayMetrics);
	DisplayMetrics->RebuildDisplayMetrics(*DisplayMetrics);
	Width = DisplayMetrics->PrimaryDisplayWidth;
	Height = DisplayMetrics->PrimaryDisplayHeight;
	delete DisplayMetrics;
}

void UEFMonitorLibrary::PrintDisplayInfoToLog()
{
	FDisplayMetrics* DisplayMetrics;
	DisplayMetrics = new(FDisplayMetrics);
	DisplayMetrics->RebuildDisplayMetrics(*DisplayMetrics);
	DisplayMetrics->PrintToLog();
	delete DisplayMetrics;
}

EMonitorAspectRatio UEFMonitorLibrary::GetResolutionAspectRatio(FIntPoint Resolution, float& Ratio, bool& IsWide)
{
	int width = Resolution.X;
	int height = Resolution.Y;
	EMonitorAspectRatio aspRatio = EMonitorAspectRatio::Custom;
	Ratio = (float)width / height;
	float asp = ((float)width / 4.0f) * 3.0f;
	if ((int32)asp == height)
	{
		aspRatio = EMonitorAspectRatio::Aspect4_3;
		IsWide = false;
	}
	asp = ((float)width / 5.0f) * 4.0f;
	if ((int32)asp == height)
	{
		aspRatio = EMonitorAspectRatio::Aspect5_4;
		IsWide = false;
	}
	asp = ((float)width / 16.0f) * 9.0f;
	if ((int32)asp == height)
	{
		aspRatio = EMonitorAspectRatio::Aspect16_9;
		IsWide = true;
	}
	asp = ((float)width / 16.0f) * 10.0f;
	if ((int32)asp == height)
	{
		aspRatio = EMonitorAspectRatio::Aspect16_10;
		IsWide = true;
	}
	if (aspRatio == EMonitorAspectRatio::Custom)
	{
		if (Ratio > 1.334f)
		{
			IsWide = true;
		}
		else
		{
			IsWide = false;
		}
	}
	return aspRatio;
}

int32 UEFMonitorLibrary::GetGameWindowMonitorIndex()
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

void UEFMonitorLibrary::GetCurrentMonitorResolution(int32& OutWidth, int32& OutHeight)
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

TArray<FIntPoint> UEFMonitorLibrary::GetCurrentMonitorSupportedResolutions()
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

bool UEFMonitorLibrary::IsGameOnPrimaryMonitor()
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