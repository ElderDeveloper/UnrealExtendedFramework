// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESteamLog.h"
#include "Shared/ESteamTypes.h"

// The whole UnrealExtendedSteam plugin logs to the single LogExtendedSteam category declared by
// ExtendedSteamShared (Shared/ESteamLog.h). Do NOT declare a module-specific category here.

/** Convenience alias for the platform service name this subsystem registers under ("EXTENDEDSTEAM"). */
#define EXTENDEDSTEAM_SUBSYSTEM ESTEAM_SUBSYSTEM
