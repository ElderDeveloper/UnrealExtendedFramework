// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"


UENUM(BlueprintType,Blueprintable)
enum EHitDirection
{
	No , Front , Back , Left , Right
};

UENUM()
enum EConditionOutput
{
	OutTrue,
	OutIsFalse
};

UENUM()
enum EButtonAction
{
	Press,
	Release
};

#define PRINT_STRING(Time , Color , String) 	GEngine->AddOnScreenDebugMessage(-1, Time , FColor::Color , String);

class FUEExpandedFrameworkModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
