// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

EXTENDEDFOUNDATION_API DECLARE_LOG_CATEGORY_EXTERN(LogExtendedFoundation, Log, All);

class FExtendedFoundationModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	int32 HttpStatusHandle = 0;
};
