// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Application/IInputProcessor.h"
#include "Modules/ModuleManager.h"


#if WITH_EDITOR
class UNREALEXTENDEDFRAMEWORKEDITOR_API FDoubleClickCastInputProcessor final
	: public TSharedFromThis<FDoubleClickCastInputProcessor>
	  , public IInputProcessor
{
public:
	static FDoubleClickCastInputProcessor& Get();

	//~ Begin IInputProcessor Interface
	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {};
	//~ End IInputProcessor Interface

private:
	TSet<FKey> KeysDown;
	TSharedPtr<FUICommandList> CommandList;
};
#endif


#if WITH_EDITOR
class FEEUILabFeature;
class FEELocalizationWorkbenchFeature;
#endif

class FUnrealExtendedFrameworkEditorModule : public IModuleInterface
{
public:

#if WITH_EDITOR
	TSharedPtr<FDoubleClickCastInputProcessor> InputProcessor;
#endif

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
#if WITH_EDITOR
	/** Registers the Extended Framework editor feature tabs (UI Lab, Localization Workbench). */
	void RegisterEditorFeatures();
	void UnregisterEditorFeatures();

	TUniquePtr<FEEUILabFeature> UILabFeature;
	TUniquePtr<FEELocalizationWorkbenchFeature> LocalizationWorkbenchFeature;
#endif
};
