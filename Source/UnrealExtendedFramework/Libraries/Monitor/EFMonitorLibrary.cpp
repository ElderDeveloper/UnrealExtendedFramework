// Fill out your copyright notice in the Description page of Project Settings.


#include "EFMonitorLibrary.h"
#include "GenericPlatform/GenericApplication.h"
#include "Kismet/KismetSystemLibrary.h"


// ================================ SHARED HELPER ================================

EMonitorAspectRatio UEFMonitorLibrary::ClassifyAspectRatio(int32 Width, int32 Height, float& Ratio, bool& IsWide)
{
	// BUG FIX: Guard against division by zero
	if (Height <= 0)
	{
		Ratio = 0.f;
		IsWide = false;
		return EMonitorAspectRatio::Custom;
	}

	Ratio = (float)Width / (float)Height;
	EMonitorAspectRatio AspRatio = EMonitorAspectRatio::Custom;

	float asp = ((float)Width / 4.0f) * 3.0f;
	if ((int32)asp == Height)
	{
		AspRatio = EMonitorAspectRatio::Aspect4_3;
		IsWide = false;
	}
	asp = ((float)Width / 5.0f) * 4.0f;
	if ((int32)asp == Height)
	{
		AspRatio = EMonitorAspectRatio::Aspect5_4;
		IsWide = false;
	}
	asp = ((float)Width / 16.0f) * 9.0f;
	if ((int32)asp == Height)
	{
		AspRatio = EMonitorAspectRatio::Aspect16_9;
		IsWide = true;
	}
	asp = ((float)Width / 16.0f) * 10.0f;
	if ((int32)asp == Height)
	{
		AspRatio = EMonitorAspectRatio::Aspect16_10;
		IsWide = true;
	}
	if (AspRatio == EMonitorAspectRatio::Custom)
	{
		IsWide = Ratio > 1.334f;
	}
	return AspRatio;
}


// ================================ MONITOR INFO ================================

FDisplayInfo UEFMonitorLibrary::GetMonitorInfoByIndex(const int32 Index)
{
	// BUG FIX: Previously allocated FDisplayMetrics on heap with new/delete.
	// Stack allocation is sufficient and avoids leak risk.
	FDisplayMetrics DisplayMetrics;
	FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);

	FDisplayInfo mi;

	// BUG FIX: Added bounds check to prevent crash on invalid index
	if (!DisplayMetrics.MonitorInfo.IsValidIndex(Index))
	{
		UE_LOG(LogTemp, Warning, TEXT("GetMonitorInfoByIndex: Index %d out of range (have %d monitors)"), Index, DisplayMetrics.MonitorInfo.Num());
		return mi;
	}

	const auto& Monitor = DisplayMetrics.MonitorInfo[Index];
	mi.Name = Monitor.Name;
	mi.ID = Monitor.ID;
	mi.NativeWidth = Monitor.NativeWidth;
	mi.NativeHeight = Monitor.NativeHeight;
	mi.MaxResolution = Monitor.MaxResolution;
	if (mi.MaxResolution.X == 0 || mi.MaxResolution.Y == 0)
	{
		mi.MaxResolution.X = mi.NativeWidth;
		mi.MaxResolution.Y = mi.NativeHeight;
	}
	mi.DisplayRect.Left = Monitor.DisplayRect.Left;
	mi.DisplayRect.Top = Monitor.DisplayRect.Top;
	mi.DisplayRect.Right = Monitor.DisplayRect.Right;
	mi.DisplayRect.Bottom = Monitor.DisplayRect.Bottom;
	mi.WorkArea.Left = Monitor.WorkArea.Left;
	mi.WorkArea.Top = Monitor.WorkArea.Top;
	mi.WorkArea.Right = Monitor.WorkArea.Right;
	mi.WorkArea.Bottom = Monitor.WorkArea.Bottom;
	mi.bIsPrimary = Monitor.bIsPrimary;
	mi.DPI = Monitor.DPI;
	return mi;
}

int32 UEFMonitorLibrary::GetNAttachedMonitors()
{
	FDisplayMetrics DisplayMetrics;
	FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);
	return DisplayMetrics.MonitorInfo.Num();
}

EMonitorAspectRatio UEFMonitorLibrary::GetMonitorAspectRatio(int32 Index, float& Ratio, bool& IsWide)
{
	FDisplayMetrics DisplayMetrics;
	FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);

	if (!DisplayMetrics.MonitorInfo.IsValidIndex(Index))
	{
		Ratio = 0.f;
		IsWide = false;
		return EMonitorAspectRatio::Custom;
	}

	return ClassifyAspectRatio(
		DisplayMetrics.MonitorInfo[Index].NativeWidth,
		DisplayMetrics.MonitorInfo[Index].NativeHeight,
		Ratio, IsWide);
}

int32 UEFMonitorLibrary::GetPrimaryMonitorIndex()
{
	FDisplayMetrics DisplayMetrics;
	FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);
	for (int32 i = 0; i < DisplayMetrics.MonitorInfo.Num(); i++)
	{
		if (DisplayMetrics.MonitorInfo[i].bIsPrimary)
		{
			return i;
		}
	}
	return -1;
}

FIntPoint UEFMonitorLibrary::GetMaxMonitorResolution(int32 Index)
{
	FDisplayMetrics DisplayMetrics;
	FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);

	if (!DisplayMetrics.MonitorInfo.IsValidIndex(Index))
	{
		return FIntPoint(ForceInitToZero);
	}

	FIntPoint maxRes = DisplayMetrics.MonitorInfo[Index].MaxResolution;
	if (maxRes.X == 0 || maxRes.Y == 0)
	{
		maxRes.X = DisplayMetrics.MonitorInfo[Index].NativeWidth;
		maxRes.Y = DisplayMetrics.MonitorInfo[Index].NativeHeight;
	}
	return maxRes;
}

void UEFMonitorLibrary::GetMonitorSafeAreas(FRect& TitleSafeArea, float& TitleSafeRatio, FRect& ActionSafeArea)
{
	FDisplayMetrics DisplayMetrics;
	FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);
	TitleSafeArea.Left = DisplayMetrics.TitleSafePaddingSize.X;
	TitleSafeArea.Top = DisplayMetrics.TitleSafePaddingSize.Y;
	TitleSafeArea.Right = DisplayMetrics.TitleSafePaddingSize.Z;
	TitleSafeArea.Bottom = DisplayMetrics.TitleSafePaddingSize.W;
	TitleSafeRatio = DisplayMetrics.GetDebugTitleSafeZoneRatio();
	ActionSafeArea.Left = DisplayMetrics.ActionSafePaddingSize.X;
	ActionSafeArea.Top = DisplayMetrics.ActionSafePaddingSize.Y;
	ActionSafeArea.Right = DisplayMetrics.ActionSafePaddingSize.Z;
	ActionSafeArea.Bottom = DisplayMetrics.ActionSafePaddingSize.W;
}

void UEFMonitorLibrary::GetPrimaryDisplayResolution(int32& Width, int32& Height)
{
	FDisplayMetrics DisplayMetrics;
	FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);
	Width = DisplayMetrics.PrimaryDisplayWidth;
	Height = DisplayMetrics.PrimaryDisplayHeight;
}

void UEFMonitorLibrary::PrintDisplayInfoToLog()
{
	FDisplayMetrics DisplayMetrics;
	FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);
	DisplayMetrics.PrintToLog();
}

EMonitorAspectRatio UEFMonitorLibrary::GetResolutionAspectRatio(FIntPoint Resolution, float& Ratio, bool& IsWide)
{
	return ClassifyAspectRatio(Resolution.X, Resolution.Y, Ratio, IsWide);
}


// ================================ GAME WINDOW MONITOR ================================

int32 UEFMonitorLibrary::GetGameWindowMonitorIndex()
{
	if (!FSlateApplication::IsInitialized())
	{
		return 0;
	}

	TSharedPtr<SWindow> GameWindow = nullptr;
	if (GEngine && GEngine->GameViewport)
	{
		GameWindow = GEngine->GameViewport->GetWindow();
	}

	if (!GameWindow.IsValid())
	{
		return 0;
	}

	FVector2D WindowPosition = GameWindow->GetPositionInScreen();

	FDisplayMetrics DisplayMetrics;
	FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);

	for (int32 MonitorIndex = 0; MonitorIndex < DisplayMetrics.MonitorInfo.Num(); ++MonitorIndex)
	{
		const FMonitorInfo& Monitor = DisplayMetrics.MonitorInfo[MonitorIndex];
		
		if (WindowPosition.X >= Monitor.DisplayRect.Left && 
			WindowPosition.X < Monitor.DisplayRect.Right &&
			WindowPosition.Y >= Monitor.DisplayRect.Top && 
			WindowPosition.Y < Monitor.DisplayRect.Bottom)
		{
			return MonitorIndex;
		}
	}

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
	int32 MonitorIndex = GetGameWindowMonitorIndex();

	FDisplayMetrics DisplayMetrics;
	FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);

	if (MonitorIndex >= 0 && MonitorIndex < DisplayMetrics.MonitorInfo.Num())
	{
		OutWidth = DisplayMetrics.MonitorInfo[MonitorIndex].NativeWidth;
		OutHeight = DisplayMetrics.MonitorInfo[MonitorIndex].NativeHeight;
	}
	else
	{
		OutWidth = DisplayMetrics.PrimaryDisplayWidth;
		OutHeight = DisplayMetrics.PrimaryDisplayHeight;
	}
}

TArray<FIntPoint> UEFMonitorLibrary::GetCurrentMonitorSupportedResolutions()
{
	TArray<FIntPoint> SupportedResolutions;

	int32 MonitorIndex = GetGameWindowMonitorIndex();

	UKismetSystemLibrary::GetSupportedFullscreenResolutions(SupportedResolutions);

	int32 MonitorWidth, MonitorHeight;
	GetCurrentMonitorResolution(MonitorWidth, MonitorHeight);

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
	int32 MonitorIndex = GetGameWindowMonitorIndex();

	FDisplayMetrics DisplayMetrics;
	FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);

	if (MonitorIndex >= 0 && MonitorIndex < DisplayMetrics.MonitorInfo.Num())
	{
		return DisplayMetrics.MonitorInfo[MonitorIndex].bIsPrimary;
	}

	return true;
}


int32 UEFMonitorLibrary::GetMonitorRefreshRate(int32 Index)
{
	FDisplayMetrics DisplayMetrics;
	FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);

	if (Index < 0 || Index >= DisplayMetrics.MonitorInfo.Num())
	{
		return 0;
	}

	// Get all supported resolutions and find the max refresh rate
	// that fits within this monitor's native resolution
	TArray<FScreenResolutionRHI> Resolutions;
	if (RHIGetAvailableResolutions(Resolutions, true))
	{
		const FMonitorInfo& Monitor = DisplayMetrics.MonitorInfo[Index];
		int32 MaxRefreshRate = 0;

		for (const FScreenResolutionRHI& Res : Resolutions)
		{
			if (static_cast<int32>(Res.Width) <= Monitor.NativeWidth &&
				static_cast<int32>(Res.Height) <= Monitor.NativeHeight)
			{
				MaxRefreshRate = FMath::Max(MaxRefreshRate, static_cast<int32>(Res.RefreshRate));
			}
		}
		return MaxRefreshRate;
	}

	return 0;
}