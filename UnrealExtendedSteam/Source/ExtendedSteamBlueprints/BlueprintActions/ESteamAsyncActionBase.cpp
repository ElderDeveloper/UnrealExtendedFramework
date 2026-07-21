// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "BlueprintActions/ESteamAsyncActionBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TimerManager.h"

void USteamAsyncActionBase::ArmTimeout(float TimeoutSeconds)
{
	const float Effective = TimeoutSeconds > 0.f ? TimeoutSeconds : DefaultTimeoutCapSeconds;
	if (UWorld* World = GetWorldSafe())
	{
		World->GetTimerManager().SetTimer(TimeoutHandle, this, &USteamAsyncActionBase::HandleTimeoutInternal, Effective, false);
	}
}

bool USteamAsyncActionBase::BeginComplete()
{
	if (bCompleted)
	{
		return false;
	}
	bCompleted = true;

	if (UWorld* World = GetWorldSafe())
	{
		World->GetTimerManager().ClearTimer(TimeoutHandle);
	}
	return true;
}

UWorld* USteamAsyncActionBase::GetWorldSafe() const
{
	return GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
}

void USteamAsyncActionBase::HandleTimeoutInternal()
{
	OnTimeoutFailure();
}
