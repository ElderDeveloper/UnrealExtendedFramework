// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// The one EOS log category, owned by the base module so every sibling module (main subsystems,
// Blueprints, Testing) logs to it. Exported so cross-module references link.
EXTENDEDEOSSHARED_API DECLARE_LOG_CATEGORY_EXTERN(LogExtendedEOS, Log, All);
