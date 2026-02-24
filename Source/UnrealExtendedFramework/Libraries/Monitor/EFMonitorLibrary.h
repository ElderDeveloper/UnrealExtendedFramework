// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EFMonitorLibrary.generated.h"

/** Simple rectangle struct for screen coordinates (Left, Top, Right, Bottom). */
USTRUCT(BlueprintType)
struct FRect
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Rectangle")
	float Left = 0;
	UPROPERTY(BlueprintReadWrite, Category = "Rectangle")
	float Top = 0;
	UPROPERTY(BlueprintReadWrite, Category = "Rectangle")
	float Right = 0;
	UPROPERTY(BlueprintReadWrite, Category = "Rectangle")
	float Bottom = 0;
};

/** Information about a connected display/monitor. */
USTRUCT(BlueprintType)
struct FDisplayInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "DisplayInfo")
	FString Name = "";
	UPROPERTY(BlueprintReadOnly, Category = "DisplayInfo")
	FString ID = "";
	UPROPERTY(BlueprintReadOnly, Category = "DisplayInfo")
	int32 NativeWidth = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DisplayInfo")
	int32 NativeHeight = 0;
	UPROPERTY(BlueprintReadOnly, Category = "DisplayInfo")
	FIntPoint MaxResolution = FIntPoint(ForceInitToZero);
	UPROPERTY(BlueprintReadOnly, Category = "DisplayInfo")
	FRect DisplayRect = FRect();
	UPROPERTY(BlueprintReadOnly, Category = "DisplayInfo")
	FRect WorkArea = FRect();
	UPROPERTY(BlueprintReadOnly, Category = "DisplayInfo")
	bool bIsPrimary = false;
	UPROPERTY(BlueprintReadOnly, Category = "DisplayInfo")
	int32 DPI = 0;
};

/** Standard monitor aspect ratios. */
UENUM(BlueprintType)
enum class EMonitorAspectRatio : uint8
{
	Aspect4_3		UMETA(DisplayName = "4:3"),
	Aspect5_4		UMETA(DisplayName = "5:4"),
	Aspect16_9		UMETA(DisplayName = "16:9"),
	Aspect16_10		UMETA(DisplayName = "16:10"),
	Custom			UMETA(DisplayName = "Custom"),
};

/**
 * Blueprint function library for querying monitor/display information including
 * resolution, aspect ratio, DPI, safe areas, and multi-monitor configuration.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFMonitorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * Returns detailed information about a monitor by index.
	 * @param Index Zero-based monitor index (default 0 = primary)
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Monitor Info", Keywords = "Display Device Info Monitor"), Category = "Display Info")
	static UPARAM(DisplayName = "DisplayInfo") FDisplayInfo GetMonitorInfoByIndex(const int32 Index = 0);

	/** Returns the number of attached monitors/displays. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Number Attached Monitors", Keywords = "Display Device Monitor"), Category = "Display Info")
	static UPARAM(DisplayName = "Number") int32 GetNAttachedMonitors();

	/**
	 * Returns the aspect ratio classification for a monitor by index.
	 * @param Ratio Output: the numeric aspect ratio (width/height)
	 * @param IsWide Output: true if the aspect ratio is wider than 4:3
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Monitor Aspect Ratio", Keywords = "Display Device Native Aspect Ratio Monitor"), Category = "Display Info")
	static UPARAM(DisplayName = "AspectRatio") EMonitorAspectRatio GetMonitorAspectRatio(int32 Index, float& Ratio, bool& IsWide);

	/** Returns the index of the primary monitor, or -1 if not found. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Primary Monitor Index", Keywords = "Display Device Primary Monitor"), Category = "Display Info")
	static UPARAM(DisplayName = "Index") int32 GetPrimaryMonitorIndex();

	/** Returns the maximum resolution supported by the monitor at the given index. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Max Monitor Resolution", Keywords = "Display Device Resolution Monitor"), Category = "Display Info")
	static UPARAM(DisplayName = "MaxResolution") FIntPoint GetMaxMonitorResolution(int32 Index = 0);

	/** Returns the title-safe and action-safe areas and the title-safe ratio. */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Monitor Safe Areas", Keywords = "Display Device Safe Monitor"), Category = "Display Info")
	static void GetMonitorSafeAreas(FRect& TitleSafeArea, float& TitleSafeRatio, FRect& ActionSafeArea);

	/** Returns the primary display resolution (width and height). */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Primary Display Resolution", Keywords = "Display Device Primary Resolution Monitor"), Category = "Display Info")
	static void GetPrimaryDisplayResolution(int32& Width, int32& Height);

	/** Prints all display information to the output log. */
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Print Display Info To Log", Keywords = "Display Device Info Log Print Monitor"), Category = "Display Info")
	static void PrintDisplayInfoToLog();

	/**
	 * Classifies a resolution into a standard aspect ratio.
	 * @param Ratio Output: the numeric aspect ratio (width/height)
	 * @param IsWide Output: true if wider than 4:3
	 */
	UFUNCTION(BlueprintPure, meta = (DisplayName = "Get Resolution Aspect Ratio", Keywords = "Display Device Resolution Aspect Ratio"), Category = "Display Info")
	static UPARAM(DisplayName = "AspectRatio") EMonitorAspectRatio GetResolutionAspectRatio(FIntPoint Resolution, float& Ratio, bool& IsWide);

	/** Returns the index of the monitor the game window is currently on (0 if detection fails). */
	UFUNCTION(BlueprintCallable, Category = "Extended|Monitor")
	static int32 GetGameWindowMonitorIndex();

	/**
	 * Returns the native resolution of the monitor the game window is currently on.
	 * Falls back to primary display resolution if detection fails.
	 */
	UFUNCTION(BlueprintCallable, Category = "Extended|Monitor")
	static void GetCurrentMonitorResolution(int32& OutWidth, int32& OutHeight);

	/** Returns all supported resolutions that fit within the current monitor's native resolution. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Monitor")
	static TArray<FIntPoint> GetCurrentMonitorSupportedResolutions();

	/** Returns true if the game window is on the primary monitor. */
	UFUNCTION(BlueprintPure, Category = "Extended|Monitor")
	static bool IsGameOnPrimaryMonitor();

	/**
	 * Returns the maximum refresh rate supported by the monitor at the given index.
	 * Returns 0 if the index is invalid or the refresh rate cannot be determined.
	 * @param Index Zero-based monitor index
	 */
	UFUNCTION(BlueprintPure, Category = "Extended|Monitor")
	static int32 GetMonitorRefreshRate(int32 Index = 0);

private:

	/**
	 * Shared helper to classify a width/height pair into an aspect ratio enum.
	 * @param Width Display width in pixels
	 * @param Height Display height in pixels
	 * @param Ratio Output: numeric ratio (width/height)
	 * @param IsWide Output: true if wider than 4:3
	 */
	static EMonitorAspectRatio ClassifyAspectRatio(int32 Width, int32 Height, float& Ratio, bool& IsWide);
};
