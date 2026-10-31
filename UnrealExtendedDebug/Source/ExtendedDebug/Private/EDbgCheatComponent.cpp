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
		// Only cheats marked Run On Server run for a client; the rest belong to the client's own
		// machine (e.g. UnlockAchievement would unlock it for a listen server's host).
		const bool bServerCheat = Debug->GetCheats().ContainsByPredicate([CheatName](const FEDbgCheatInfo& Info)
		{
			return Info.Name == CheatName && Info.bRunOnServer;
		});
		if (!bServerCheat)
		{
			UE_LOG(LogExtendedDebug, Warning, TEXT("A client asked for the cheat %s, but no cheat of that name runs on the server."), *CheatName.ToString());
			return;
		}
		Debug->ExecuteCheatHere(Cast<APlayerController>(GetOwner()), CheatName, Arguments);
	}
#endif
}
