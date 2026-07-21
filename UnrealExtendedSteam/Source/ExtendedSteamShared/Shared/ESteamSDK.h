// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

// SDK availability macros, defined per-module by ExtendedSteamLibrary.Apply() in the Build.cs.
// Modules that never call Apply() fall back to "no SDK" here, which is only safe as long as
// no PUBLIC header changes layout based on these macros — keep SDK-gated code private.

#ifndef WITH_EXTENDEDSTEAM_SDK
	#define WITH_EXTENDEDSTEAM_SDK 0
#endif

#ifndef ESTEAM_SDK_VERSION
	#define ESTEAM_SDK_VERSION 0
#endif

/** True when the Steamworks SDK is available and at least the given version, e.g. ESTEAM_SDK_AT_LEAST(161). */
#define ESTEAM_SDK_AT_LEAST(RequiredVersion) (WITH_EXTENDEDSTEAM_SDK && ESTEAM_SDK_VERSION >= (RequiredVersion))
