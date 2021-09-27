// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"


UENUM(BlueprintType,Blueprintable)
enum EHitDirection
{
	No , Front , Back , Left , Right
};



class FUEExpandedFrameworkModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
