// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"

/**
 * The timer behind one of the AsyncLibrary nodes. It remembers the world it was started in, so the
 * node can clear it in CancelWork even after the object that started the node is gone.
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
		bUseGameInstanceTimers = bInGameInstanceTimers;
		FTimerManager* Manager = FindTimerManager();
		if (!Manager)
		{
			TimerWorld.Reset();
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
	}

	/** The world the timer runs in; null once cleared or when that world is gone. */
	UWorld* GetTimerWorld() const
	{
		return TimerWorld.Get();
	}

private:
	FTimerManager* FindTimerManager() const
	{
		UWorld* World = TimerWorld.Get();
		if (!World)
		{
			return nullptr;
		}
		if (bUseGameInstanceTimers)
		{
			if (UGameInstance* GameInstance = World->GetGameInstance())
			{
				return &GameInstance->GetTimerManager();
			}
		}
		return &World->GetTimerManager();
	}

	TWeakObjectPtr<UWorld> TimerWorld;
	FTimerHandle TimerHandle;
	bool bUseGameInstanceTimers = false;
};
