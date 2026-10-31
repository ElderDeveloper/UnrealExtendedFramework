// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

EXTENDEDSAVE_API DECLARE_LOG_CATEGORY_EXTERN(LogExtendedSave, Log, All);

class FExtendedSaveModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
