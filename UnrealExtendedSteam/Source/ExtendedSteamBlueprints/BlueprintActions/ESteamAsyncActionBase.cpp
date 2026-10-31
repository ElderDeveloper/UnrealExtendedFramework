// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "BlueprintActions/ESteamAsyncActionBase.h"

void USteamAsyncActionBase::InitAsyncAction(UObject* WorldContext, float TimeoutSeconds)
{
	WorldContextObject = WorldContext;

	const float EffectiveTimeout = TimeoutSeconds > 0.f ? TimeoutSeconds : DefaultTimeoutCapSeconds;
	Super::InitAsyncAction(WorldContext, EffectiveTimeout);
}
