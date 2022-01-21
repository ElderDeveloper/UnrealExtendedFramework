// Fill out your copyright notice in the Description page of Project Settings.


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

