// Copyright Devil of the Plague. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

class UWorld;

/**
 * The one time source the quest runtime reads.
 *
 * Everything that stamps or compares a time - snapshot timestamps, the timer tracker, the
 * simulator - goes through this, so a test can advance time without ticking a world.
 *
 * Not a UObject and not reflected: implementations are owned by whoever injected them (a test, the
 * simulator), never by the asset graph or the replication path, and nothing serializes one.
 */
class UNREALEXTENDEDQUEST_API IEGQuestClock
{
public:
	virtual ~IEGQuestClock() {}

	/** Server world seconds. Must be monotonic within one run and callable on the authority only. */
	virtual double GetServerTime() const = 0;
};

/**
 * The production clock: the game state's replicated server time, falling back to world time.
 *
 * Mirrors UEGQuestComponent::GetServerTime exactly, including re-reading the world on every call and
 * returning 0.0 while there is none - a component whose world is briefly null must recover on the
 * next call rather than latch.
 *
 * NOTE: UEGQuestComponent does NOT construct one of these. It has no clock until something injects
 * one, and falls through to its own inline copy of this logic otherwise; see SetQuestClock for why
 * caching a default is a defect rather than an optimisation. This class exists so a caller that
 * needs the default behaviour behind the interface (the simulator wrapping a real world, a test
 * comparing against production timing) can build one explicitly.
 */
class UNREALEXTENDEDQUEST_API FEGQuestWorldClock : public IEGQuestClock
{
public:
	explicit FEGQuestWorldClock(UWorld* InWorld) : World(InWorld) {}

	//~ Begin IEGQuestClock interface
	double GetServerTime() const override;
	//~ End IEGQuestClock interface

private:
	// Weak: the clock outliving its world is normal (travel, PIE teardown) and reads 0.0 after.
	TWeakObjectPtr<UWorld> World;
};
