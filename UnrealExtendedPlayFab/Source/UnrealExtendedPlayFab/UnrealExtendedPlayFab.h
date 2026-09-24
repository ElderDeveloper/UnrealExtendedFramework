// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EFLog.h"
#include "Modules/ModuleManager.h"

// PlayFab logging goes through EF_LOG(ExtendedPlayFab, ...) into Saved/Logs/Extended/ExtendedPlayFab.log
// (Window > Log > Extended Log), not the Output Log.

class FUnrealExtendedPlayFabModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
