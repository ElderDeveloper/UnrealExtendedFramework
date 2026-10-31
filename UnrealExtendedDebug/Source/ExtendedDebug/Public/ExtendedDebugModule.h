// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

EXTENDEDDEBUG_API DECLARE_LOG_CATEGORY_EXTERN(LogExtendedDebug, Log, All);

class FExtendedDebugModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
