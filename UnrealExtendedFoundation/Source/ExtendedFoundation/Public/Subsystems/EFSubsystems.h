// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"

/**
 * Subsystem lookup from any world context: an actor, component, widget, controller, game instance,
 * local player or subsystem. Every function returns null instead of asserting when there is no
 * world or game instance (editor utilities, CDOs, shutdown).
 *
 *     if (UEPFAuthSubsystem* Auth = EF::GetGameInstanceSubsystem<UEPFAuthSubsystem>(this)) { ... }
 */
namespace EF
{
	/** The world WorldContextObject belongs to, or null. */
	EXTENDEDFOUNDATION_API UWorld* GetWorld(const UObject* WorldContextObject);

	/** The game instance WorldContextObject belongs to, or null. */
	EXTENDEDFOUNDATION_API UGameInstance* GetGameInstance(const UObject* WorldContextObject);

	/**
	 * The local player WorldContextObject belongs to: the object itself, its owning player
	 * controller, or the controller of its owning pawn. Falls back to the game instance's first
	 * local player when the object has no owner of its own.
	 */
	EXTENDEDFOUNDATION_API ULocalPlayer* GetLocalPlayer(const UObject* WorldContextObject);

	template <typename TSubsystem>
	TSubsystem* GetGameInstanceSubsystem(const UObject* WorldContextObject)
	{
		UGameInstance* GameInstance = GetGameInstance(WorldContextObject);
		return GameInstance ? GameInstance->GetSubsystem<TSubsystem>() : nullptr;
	}

	template <typename TSubsystem>
	TSubsystem* GetWorldSubsystem(const UObject* WorldContextObject)
	{
		UWorld* World = GetWorld(WorldContextObject);
		return World ? World->GetSubsystem<TSubsystem>() : nullptr;
	}

	template <typename TSubsystem>
	TSubsystem* GetLocalPlayerSubsystem(const UObject* WorldContextObject)
	{
		ULocalPlayer* LocalPlayer = GetLocalPlayer(WorldContextObject);
		return LocalPlayer ? LocalPlayer->GetSubsystem<TSubsystem>() : nullptr;
	}

	template <typename TSubsystem>
	TSubsystem* GetEngineSubsystem()
	{
		return GEngine ? GEngine->GetEngineSubsystem<TSubsystem>() : nullptr;
	}
}
