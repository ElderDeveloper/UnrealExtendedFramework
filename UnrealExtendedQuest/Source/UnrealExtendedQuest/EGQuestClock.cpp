// Copyright Devil of the Plague. All Rights Reserved.
#include "EGQuestClock.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

double FEGQuestWorldClock::GetServerTime() const
{
	// Resolved per call, never cached: a clock built while its world was null must answer correctly
	// once the world exists, and must not latch a stale world across travel.
	const UWorld* ResolvedWorld = World.Get(); if (!ResolvedWorld) return 0.0;
	if (const AGameStateBase* GS = ResolvedWorld->GetGameState()) return GS->GetServerWorldTimeSeconds();
	return ResolvedWorld->GetTimeSeconds();
}
