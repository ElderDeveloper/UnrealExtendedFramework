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


void UUEExtendedMobileLibrary::RequestExit(bool Force)
{
	FPlatformMisc::RequestExit(Force);
}

