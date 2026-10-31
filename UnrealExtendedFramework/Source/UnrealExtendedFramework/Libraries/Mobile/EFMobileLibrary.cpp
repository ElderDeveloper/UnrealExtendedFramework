// Copyright Moon Punch Games. All Rights Reserved.


#include "EFMobileLibrary.h"

#include "GameFramework/PlayerState.h"

FString UEFMobileLibrary::GetOnlineAccountID(APlayerController* PlayerController)
{
	if (PlayerController && PlayerController->PlayerState && PlayerController->PlayerState->GetUniqueId().IsValid())
	{
		return PlayerController->PlayerState->GetUniqueId()->GetHexEncodedString();
	}
	return FString();
}


void UEFMobileLibrary::RequestExit(bool Force)
{
	FPlatformMisc::RequestExit(Force);
}


FString UEFMobileLibrary::GetDeviceName()
{
	return FPlatformMisc::GetDefaultDeviceProfileName();
}


FString UEFMobileLibrary::GetOperatingSystemVersion()
{
	return FPlatformMisc::GetOSVersion();
}
