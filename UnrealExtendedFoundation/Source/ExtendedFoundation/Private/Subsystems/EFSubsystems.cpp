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

	// Walk the outer chain for an owning controller or pawn: components, widgets and actors.
	for (const UObject* Current = WorldContextObject; Current; Current = Current->GetOuter())
	{
		if (const APlayerController* Controller = Cast<APlayerController>(Current))
		{
			if (ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer())
			{
				return LocalPlayer;
			}
		}
		if (const APawn* Pawn = Cast<APawn>(Current))
		{
			if (const APlayerController* Controller = Cast<APlayerController>(Pawn->GetController()))
			{
				if (ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer())
				{
					return LocalPlayer;
				}
			}
		}
		if (const AActor* Actor = Cast<AActor>(Current))
		{
			if (const APlayerController* Controller = Cast<APlayerController>(Actor->GetOwner()))
			{
				if (ULocalPlayer* LocalPlayer = Controller->GetLocalPlayer())
				{
					return LocalPlayer;
				}
			}
		}
	}

	const UGameInstance* GameInstance = GetGameInstance(WorldContextObject);
	return GameInstance ? GameInstance->GetFirstGamePlayer() : nullptr;
}
