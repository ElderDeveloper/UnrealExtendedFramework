// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EFMobileLibrary.generated.h"

/**
 * Blueprint function library exposing platform/device information including
 * battery status, network connectivity, device identifiers, and application lifecycle.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFMobileLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()


public:

	/**
	 * Returns the online account ID (hex-encoded) for a player controller's player state.
	 * @param PlayerController The player controller to query
	 * @return Hex-encoded unique ID, or empty string on failure
	 */
	UFUNCTION(BlueprintPure, Category = "Generic Platform Misc")
	static FString GetOnlineAccountID(APlayerController* PlayerController);

	/** Returns the current battery level (0-100), or -1 if unavailable. */
	UFUNCTION(BlueprintPure, Category = "Generic Platform Misc")
	static int GetBatteryLevel() { return FPlatformMisc::GetBatteryLevel(); }
	
	/** Returns the system default language code (e.g., "en", "ja"). */
	UFUNCTION(BlueprintPure, Category = "Generic Platform Misc")
	static FString GetLanguage() { return FPlatformMisc::GetDefaultLanguage(); }

	/** Returns a platform-specific device identifier string. */
	UFUNCTION(BlueprintPure, Category = "Generic Platform Misc")
	static FString GetDeviceID() { return FPlatformMisc::GetDeviceId(); }
	
	/** Returns the IANA time zone identifier (e.g., "America/New_York"). */
	UFUNCTION(BlueprintPure, Category = "Generic Platform Misc")
	static FString GetTimeZoneId() { return FPlatformMisc::GetTimeZoneId(); }

	/** Returns true if the device has an active WiFi connection. */
	UFUNCTION(BlueprintPure, Category = "Generic Platform Misc")
	static bool HasActiveWiFiConnection() { return FPlatformMisc::HasActiveWiFiConnection(); }
	
	/** Returns true if the device is currently running on battery power. */
	UFUNCTION(BlueprintPure, Category = "Generic Platform Misc")
	static bool IsRunningOnBattery() { return FPlatformMisc::IsRunningOnBattery(); }

	/**
	 * Requests application exit.
	 * @param Force If true, forces immediate termination without cleanup
	 */
	UFUNCTION(BlueprintCallable, Category = "Generic Platform Misc")
	static void RequestExit(bool Force);

	/**
	 * Returns the value of a system environment variable.
	 * @param VariableName Name of the environment variable
	 */
	UFUNCTION(BlueprintPure, Category = "Generic Platform Misc")
	static FString GetEnvironmentVariable(FString VariableName) { return FPlatformMisc::GetEnvironmentVariable(*VariableName); }
	
	/**
	 * Sets a system environment variable.
	 * @param VariableName Name of the environment variable
	 * @param Value Value to set
	 */
	UFUNCTION(BlueprintCallable, Category = "Generic Platform Misc")
	static void SetEnvironmentVar(FString VariableName, FString Value) { FPlatformMisc::SetEnvironmentVar(*VariableName, *Value); }

	/** Returns the human-readable device model/make name. */
	UFUNCTION(BlueprintPure, Category = "Generic Platform Misc")
	static FString GetDeviceName();

	/** Returns the operating system version string (e.g., "Windows 10 (Build 19045)"). */
	UFUNCTION(BlueprintPure, Category = "Generic Platform Misc")
	static FString GetOperatingSystemVersion();
};
