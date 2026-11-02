// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/** Editor-only tools for UnrealExtendedGameplay (animation modifiers, placement visualizers and fitting). */
class FUnrealExtendedGameplayEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterVisualizers();

	TArray<FName> RegisteredVisualizers;
	FDelegateHandle PostEngineInitHandle;
};
