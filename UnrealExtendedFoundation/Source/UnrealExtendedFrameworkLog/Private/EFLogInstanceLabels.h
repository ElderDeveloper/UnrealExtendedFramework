// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * Turns a PIE instance into the name a person reads: "Server", "Client 1", "Standalone".
 *
 * The labels follow the same rules as Print String's "Server: " / "Client N: " prefixes (a client
 * is numbered by its PIE instance), so an EF_LOG line and a Print String from the same world agree.
 * Everything here runs on the game thread only: it reads world contexts. The writer thread only
 * ever sees the resulting FName.
 */
namespace EFLogInstanceLabels
{
	/** Label for the world context running this PIE instance; None when there is no such world (the writer then prints "PIE <n>"). */
	FName ForPIEInstance(int32 PIEInstance);

	/** Label for a specific world; None outside PIE. Exact even off-tick, since it reads the world's own package. */
	FName ForWorld(const UWorld* World, int32& OutPIEInstance);
}
