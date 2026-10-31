// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

/**
 * UnrealExtendedFoundation version, for compile-time compatibility checks.
 *
 * Bump MINOR when adding API, MAJOR when removing or changing it, PATCH for fixes. Keep it in step
 * with VersionName in UnrealExtendedFoundation.uplugin.
 *
 * A dependent plugin states the oldest foundation it supports once, in its module .cpp:
 *
 *     #include "EFFoundationVersion.h"
 *     EF_REQUIRE_FOUNDATION_VERSION(1, 0, "UnrealExtendedPlayFab");
 *
 * A mismatched copy then fails with a readable message instead of a wall of missing symbols.
 */
#define EF_FOUNDATION_VERSION_MAJOR 1
#define EF_FOUNDATION_VERSION_MINOR 2
#define EF_FOUNDATION_VERSION_PATCH 0

#define EF_FOUNDATION_VERSION ((EF_FOUNDATION_VERSION_MAJOR * 10000) + (EF_FOUNDATION_VERSION_MINOR * 100) + EF_FOUNDATION_VERSION_PATCH)

/** True when the foundation is Major.Minor or newer within the same major version. */
#define EF_FOUNDATION_VERSION_AT_LEAST(Major, Minor) \
	(EF_FOUNDATION_VERSION_MAJOR == (Major) && EF_FOUNDATION_VERSION_MINOR >= (Minor))

#define EF_REQUIRE_FOUNDATION_VERSION(Major, Minor, PluginName) \
	static_assert(EF_FOUNDATION_VERSION_AT_LEAST(Major, Minor), \
		PluginName " needs UnrealExtendedFoundation " #Major "." #Minor " or a newer " #Major ".x release.")
