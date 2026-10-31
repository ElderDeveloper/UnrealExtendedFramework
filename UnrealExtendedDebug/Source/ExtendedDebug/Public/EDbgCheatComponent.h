// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EDbgCheatComponent.generated.h"

/**
 * Carries cheats marked Run On Server from a client to the server. The debug subsystem adds it to
 * every player controller on the server when the player logs in; there is no need to add it by hand.
 */
UCLASS(ClassGroup = "Extended", NotBlueprintable, HideDropdown)
class EXTENDEDDEBUG_API UEDbgCheatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEDbgCheatComponent();

	/** Runs the cheat on the server for this component's player. Ignored in Shipping and when remote cheats are off. */
	UFUNCTION(Server, Reliable)
	void ServerRunCheat(FName CheatName, const TArray<FString>& Arguments);
};
