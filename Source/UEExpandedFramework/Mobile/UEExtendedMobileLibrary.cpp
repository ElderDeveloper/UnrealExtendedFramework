// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedMobileLibrary.h"


#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

FString UUEExtendedMobileLibrary::GetOnlineAccountID(APlayerController* PlayerController)
{
	if (PlayerController && PlayerController->PlayerState && PlayerController->PlayerState->GetUniqueId().IsValid())
	{
		return PlayerController->PlayerState->GetUniqueId()->GetHexEncodedString();
	}
	return FString();
}

void UUEExtendedMobileLibrary::GetBatteryLevel(int & baterylevel)
{
	baterylevel = FPlatformMisc::GetBatteryLevel();
}

void UUEExtendedMobileLibrary::GetBatteryLevelPure(int & baterylevel)
{
	baterylevel = FPlatformMisc::GetBatteryLevel();
}

void UUEExtendedMobileLibrary::GetLanguage(FString& DeviceLanguage)
{
	DeviceLanguage = FPlatformMisc::GetDefaultLanguage();
}

void UUEExtendedMobileLibrary::GetLanguagePure(FString& DeviceLanguage)
{
	DeviceLanguage = FPlatformMisc::GetDefaultLanguage();
}

void UUEExtendedMobileLibrary::GetDeviceID(FString& DeviceID)
{
	DeviceID = FPlatformMisc::GetDeviceId();
}

void UUEExtendedMobileLibrary::GetDeviceIDPure(FString& DeviceID)
{
	DeviceID = FPlatformMisc::GetDeviceId();
}

void UUEExtendedMobileLibrary::GetTimeZoneId(FString& TimeZoneId)
{
	TimeZoneId = FPlatformMisc::GetTimeZoneId();
}

void UUEExtendedMobileLibrary::GetTimeZoneIdPure(FString& TimeZoneId)
{
	TimeZoneId = FPlatformMisc::GetTimeZoneId();
}

void UUEExtendedMobileLibrary::HasActiveWiFiConnection(bool& ReturnValue)
{
	ReturnValue = FPlatformMisc::HasActiveWiFiConnection();
}

void UUEExtendedMobileLibrary::HasActiveWiFiConnectionPure(bool& ReturnValue)
{
	ReturnValue = FPlatformMisc::HasActiveWiFiConnection();
}

void UUEExtendedMobileLibrary::IsRunningOnBattery(bool& ReturnValue)
{
	ReturnValue = FPlatformMisc::IsRunningOnBattery();
}

void UUEExtendedMobileLibrary::IsRunningOnBatteryPure(bool& ReturnValue)
{
	ReturnValue = FPlatformMisc::IsRunningOnBattery();
}

void UUEExtendedMobileLibrary::RequestExit(bool Force)
{
	FPlatformMisc::RequestExit(Force);
}

void UUEExtendedMobileLibrary::GetEnvironmentVariable(FString VariableName, FString& Value)
{
	Value = FPlatformMisc::GetEnvironmentVariable(*VariableName);
}

void UUEExtendedMobileLibrary::GetEnvironmentVariablePure(FString VariableName, FString& Value)
{
	Value = FPlatformMisc::GetEnvironmentVariable(*VariableName);
}

void UUEExtendedMobileLibrary::SetEnvironmentVar(FString VariableName, FString Value)
{
	FPlatformMisc::SetEnvironmentVar(*VariableName, *Value);
}
