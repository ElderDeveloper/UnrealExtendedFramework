// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// The log category now lives in ExtendedEOSShared; re-included here so the many subsystem .cpp
// files that include "UnrealExtendedEOS.h" for LogExtendedEOS keep compiling unchanged.
#include "Shared/EEOSLog.h"

class FUnrealExtendedEOSModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
