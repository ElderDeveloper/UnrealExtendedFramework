// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EDbgCheatComponent.h"

#include "EDbgSettings.h"
#include "EDbgSubsystem.h"
#include "ExtendedDebugModule.h"
#include "GameFramework/PlayerController.h"

UEDbgCheatComponent::UEDbgCheatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UEDbgCheatComponent::ServerRunCheat_Implementation(FName CheatName, const TArray<FString>& Arguments)
{
#if EXTENDED_DEBUG_ENABLED
	if (!UEDbgSettings::Get()->bAllowRemoteCheats)
	{
		UE_LOG(LogExtendedDebug, Warning, TEXT("A client asked for the cheat %s, but remote cheats are turned off."), *CheatName.ToString());
		return;
	}
	if (UEDbgSubsystem* Debug = UEDbgSubsystem::Get(this))
	{
		Debug->ExecuteCheatHere(Cast<APlayerController>(GetOwner()), CheatName, Arguments);
	}
#endif
}
