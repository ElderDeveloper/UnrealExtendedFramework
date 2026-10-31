// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Debugging and setup checks for the animation systems.
 *
 * eg.Anim.Debug 1 draws melee sweeps and warp targets in game worlds.
 */
namespace EGAnimDebug
{
	UNREALEXTENDEDGAMEPLAY_API bool IsEnabled();

	/**
	 * Logs a setup problem once per source object and message ("this melee window has no
	 * UEGMeleeTraceComponent to open"), instead of every time the notify fires.
	 */
	UNREALEXTENDEDGAMEPLAY_API void WarnOnce(const UObject* Source, const FString& Message);
}
