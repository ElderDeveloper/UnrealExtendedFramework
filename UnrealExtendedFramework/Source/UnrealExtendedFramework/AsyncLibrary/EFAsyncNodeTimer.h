// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"

/**
 * The timer behind one of the AsyncLibrary nodes. It remembers the world (or game instance) it was
 * started in, so the node can clear it in CancelWork even after the object that started the node
 * is gone.
 */
struct FEFAsyncNodeTimer
{
	/**
	 * (Re)starts the timer on Context's world, or on its game instance's timer manager when
	 * bInGameInstanceTimers is set. A one-shot timer with Rate <= 0 fires on the next tick; a
	 * looping one needs Rate > 0. False when Context has no world or a loop has no rate.
	 */
	bool Start(const UObject* Context, const FTimerDelegate& Callback, float Rate, bool bLoop, bool bInGameInstanceTimers = false)
	{
		Clear();

		UWorld* ContextWorld = Context ? Context->GetWorld() : nullptr;
		if (!ContextWorld || (bLoop && Rate <= 0.f))
		{
			return false;
		}

		TimerWorld = ContextWorld;
		TimerGameInstance = bInGameInstanceTimers ? ContextWorld->GetGameInstance() : nullptr;
		bUseGameInstanceTimers = bInGameInstanceTimers;
		FTimerManager* Manager = FindTimerManager();
		if (!Manager)
		{
			TimerWorld.Reset();
			TimerGameInstance.Reset();
			return false;
		}

		if (Rate > 0.f)
		{
			Manager->SetTimer(TimerHandle, Callback, Rate, bLoop);
		}
		else
		{
			TimerHandle = Manager->SetTimerForNextTick(Callback);
		}
		return true;
	}

	/** Stops the timer. Safe to call when it is not running or its world is gone. */
	void Clear()
	{
		if (FTimerManager* Manager = FindTimerManager())
		{
			Manager->ClearTimer(TimerHandle);
		}
		TimerHandle.Invalidate();
		TimerWorld.Reset();
		TimerGameInstance.Reset();
	}

	/** The world the timer runs in; null once cleared or when that world is gone. */
	UWorld* GetTimerWorld() const
	{
		return TimerWorld.Get();
	}

private:
	FTimerManager* FindTimerManager() const
	{
		// A game instance timer keeps running after the world it was started in is gone (map
		// travel), so it is found through the game instance, not that world.
		if (UGameInstance* GameInstance = bUseGameInstanceTimers ? TimerGameInstance.Get() : nullptr)
		{
			return &GameInstance->GetTimerManager();
		}
		UWorld* World = TimerWorld.Get();
		return World ? &World->GetTimerManager() : nullptr;
	}

	TWeakObjectPtr<UWorld> TimerWorld;
	TWeakObjectPtr<UGameInstance> TimerGameInstance;
	FTimerHandle TimerHandle;
	bool bUseGameInstanceTimers = false;
};
