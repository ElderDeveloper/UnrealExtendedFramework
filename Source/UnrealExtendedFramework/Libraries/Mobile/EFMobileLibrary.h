// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EFMobileLibrary.generated.h"


UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFMobileLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()


public:

	/** Get the online account ID (as an encoded hex string) associated with the provided player controller's player state. Returns a blank string on failure. */
	UFUNCTION(BlueprintPure, Category = "Generic Platform Misc")
		static FString GetOnlineAccountID(APlayerController* PlayerController);

	
	UFUNCTION(BlueprintPure, Category = "Generic Platform Misc")
		static int GetBatteryLevel() { return FPlatformMisc::GetBatteryLevel(); }
	

	UFUNCTION(BlueprintPure, Category = "Generic Platform Misc")
		static FString GetLanguage() { return FPlatformMisc::GetDefaultLanguage(); }


	UFUNCTION(BlueprintPure, Category = "Generic Platform Misc")
		static FString GetDeviceID() { return FPlatformMisc::GetDeviceId(); }
	

	UFUNCTION(BlueprintPure, Category = "Generic Platform Misc")
		static FString GetTimeZoneId() { return FPlatformMisc::GetTimeZoneId(); }

	
	UFUNCTION(BlueprintPure, Category = "Generic Platform Misc")
		static bool HasActiveWiFiConnection() { return FPlatformMisc::HasActiveWiFiConnection(); }
	

	UFUNCTION(BlueprintPure, Category = "Generic Platform Misc")
		static bool IsRunningOnBattery() { return FPlatformMisc::IsRunningOnBattery(); }

	
	UFUNCTION(BlueprintCallable, Category = "Generic Platform Misc")
		static void RequestExit(bool Force) ;

	
	UFUNCTION(BlueprintPure, Category = "Generic Platform Misc")
		static FString GetEnvironmentVariable(FString VariableName) { return FPlatformMisc::GetEnvironmentVariable(*VariableName); }
	

	UFUNCTION(BlueprintCallable, Category = "Generic Platform Misc")
		static void SetEnvironmentVar(FString VariableName , FString Value) {  FPlatformMisc::SetEnvironmentVar(*VariableName,*Value); }

};
