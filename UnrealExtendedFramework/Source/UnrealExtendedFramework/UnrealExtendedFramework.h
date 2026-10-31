// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/** Plugin-wide log category for UnrealExtendedFramework. */
UNREALEXTENDEDFRAMEWORK_API DECLARE_LOG_CATEGORY_EXTERN(LogExtendedFramework, Log, All);

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
#if WITH_EDITOR
	/**
	 * Adds the modular-settings tags from ExtendedSettingTags.csv to the project's
	 * Config/Tags/ExtendedSettingsTags.ini. Editor only: the CSV is not staged into packaged builds,
	 * and a shipped game must not rewrite its own config. The generated .ini ships with the project.
	 */
	void InitializeGameplayTags();
#endif
};
