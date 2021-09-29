// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UEExtendedMobileLibrary.generated.h"

/**
 * 
 */
UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedMobileLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	public:

		/** Get the online account ID (as an encoded hex string) associated with the provided player controller's player state. Returns a blank string on failure. */
	UFUNCTION(BlueprintCallable, Category = "Super Attack Gameplay")
		static FString GetOnlineAccountID(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Generic Platform Misc")
		static void GetBatteryLevel(int& baterylevel);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Generic Platform Misc")
		static void GetBatteryLevelPure(int& baterylevel);

	UFUNCTION(BlueprintCallable, Category = "Generic Platform Misc")
		static void GetLanguage(FString& DeviceLanguage);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Generic Platform Misc")
		static void GetLanguagePure(FString& DeviceLanguage);

	UFUNCTION(BlueprintCallable, Category = "Generic Platform Misc")
		static void GetDeviceID(FString& DeviceID);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Generic Platform Misc")
		static void GetDeviceIDPure(FString& DeviceID);

	UFUNCTION(BlueprintCallable, Category = "Generic Platform Misc")
		static void GetTimeZoneId(FString& TimeZoneId);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Generic Platform Misc")
		static void GetTimeZoneIdPure(FString& TimeZoneId);

	UFUNCTION(BlueprintCallable, Category = "Generic Platform Misc")
		static void HasActiveWiFiConnection(bool& ReturnValue);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Generic Platform Misc")
		static void HasActiveWiFiConnectionPure(bool& ReturnValue);

	UFUNCTION(BlueprintCallable, Category = "Generic Platform Misc")
		static void IsRunningOnBattery(bool& ReturnValue);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Generic Platform Misc")
		static void IsRunningOnBatteryPure(bool& ReturnValue);

	UFUNCTION(BlueprintCallable, Category = "Generic Platform Misc")
		static void RequestExit(bool Force);

	UFUNCTION(BlueprintCallable, Category = "Generic Platform Misc")
		static void GetEnvironmentVariable(FString VariableName, FString& Value);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Generic Platform Misc")
		static void GetEnvironmentVariablePure(FString VariableName, FString& Value);

	UFUNCTION(BlueprintCallable, Category = "Generic Platform Misc")
		static void SetEnvironmentVar(FString VariableName, FString Value);
};
