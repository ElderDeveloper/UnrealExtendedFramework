// Copyright Moon Punch Games. All Rights Reserved.

#include "Subsystems/EFSubsystems.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Subsystems/LocalPlayerSubsystem.h"

UWorld* EF::GetWorld(const UObject* WorldContextObject)
{
	if (!WorldContextObject || !GEngine)
	{
		return nullptr;
	}
	return GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
}

UGameInstance* EF::GetGameInstance(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	// Objects that know their game instance without a world.
	if (const UGameInstance* GameInstance = Cast<UGameInstance>(WorldContextObject))
	{
		return const_cast<UGameInstance*>(GameInstance);
	}
	if (const UGameInstanceSubsystem* Subsystem = Cast<UGameInstanceSubsystem>(WorldContextObject))
	{
		return Subsystem->GetGameInstance();
	}
	if (const ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(WorldContextObject))
	{
		return LocalPlayer->GetGameInstance();
	}

	const UWorld* World = GetWorld(WorldContextObject);
	return World ? World->GetGameInstance() : nullptr;
}

ULocalPlayer* EF::GetLocalPlayer(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	if (const ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(WorldContextObject))
	{
		return const_cast<ULocalPlayer*>(LocalPlayer);
	}
	if (const ULocalPlayerSubsystem* Subsystem = Cast<ULocalPlayerSubsystem>(WorldContextObject))
	{
		return Subsystem->GetLocalPlayer();
	}

	// Walk the outer chain for an actor (a component lives inside one), then that actor's owner
	// chain: a pawn's controller, a weapon's pawn. The first player controller found decides: a
	// remote player's has no local player, and the fallback below must not hand back the host's.
	for (const UObject* Current = WorldContextObject; Current; Current = Current->GetOuter())
	{
		for (const AActor* Actor = Cast<AActor>(Current); Actor; Actor = Actor->GetOwner())
		{
			const APlayerController* Controller = Cast<APlayerController>(Actor);
			if (!Controller)
			{
				if (const APawn* Pawn = Cast<APawn>(Actor))
				{
					Controller = Cast<APlayerController>(Pawn->GetController());
				}
			}
			if (Controller)
			{
				return Controller->GetLocalPlayer();
			}
		}
	}

	const UGameInstance* GameInstance = GetGameInstance(WorldContextObject);
	return GameInstance ? GameInstance->GetFirstGamePlayer() : nullptr;
}
