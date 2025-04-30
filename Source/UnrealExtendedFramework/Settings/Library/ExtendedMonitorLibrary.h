// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ExtendedMonitorLibrary.generated.h"

/**
 * Utility library for handling multi-monitor support in the Extended game settings
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UExtendedMonitorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Gets the index of the monitor that the game window is currently on
	 * @return The index of the monitor, or 0 if the primary monitor is used or detection fails
	 */
	UFUNCTION(BlueprintCallable, Category = "Extended|Monitor")
	static int32 GetGameWindowMonitorIndex();

	/**
	 * Gets the resolution of the monitor that the game window is currently on
	 * @param OutWidth The width of the monitor
	 * @param OutHeight The height of the monitor
	 */
	UFUNCTION(BlueprintCallable, Category = "Extended|Monitor")
	static void GetCurrentMonitorResolution(int32& OutWidth, int32& OutHeight);

	/**
	 * Gets all available resolutions for the monitor that the game window is currently on
	 * @return Array of available resolutions as FIntPoint
	 */
	UFUNCTION(BlueprintCallable, Category = "Extended|Monitor")
	static TArray<FIntPoint> GetCurrentMonitorSupportedResolutions();

	/**
	 * Checks if the game window is on the primary monitor
	 * @return True if the game is on the primary monitor, false otherwise
	 */
	UFUNCTION(BlueprintPure, Category = "Extended|Monitor")
	static bool IsGameOnPrimaryMonitor();
};