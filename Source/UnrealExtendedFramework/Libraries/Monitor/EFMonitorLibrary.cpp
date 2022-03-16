// Fill out your copyright notice in the Description page of Project Settings.


#include "EFMonitorLibrary.h"
#include "GenericPlatform/GenericApplication.h"

FDisplayInfo UEFMonitorLibrary::GetMonitorInfo(int32 Index)
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