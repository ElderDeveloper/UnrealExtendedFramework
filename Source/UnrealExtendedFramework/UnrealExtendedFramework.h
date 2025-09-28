// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"


#define SET_TIMER_MACRO(Handle,Object,Function,Time,Loop) if(GetWorld()) GetWorld()->GetTimerManager().SetTimer(Handle,Object,Function,Time,Loop)

#define SET_CLEAR_TIMER_MACRO(Handle) if(GetWorld()) GetWorld()->GetTimerManager().ClearTimer(Handle)

#define GET_IS_VALID(Object) if(IsValid(Object))


class FUnrealExtendedFrameworkModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** Initialize gameplay tags from CSV file */
	void InitializeGameplayTags();
};
