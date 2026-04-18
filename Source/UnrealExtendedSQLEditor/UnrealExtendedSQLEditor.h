// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogExtendedSQLEditor, Log, All);

/**
 * Editor module for UnrealExtendedSQL.
 * Provides: asset factory, type actions, struct context menu, validation dialog,
 * and the DataTable-like editor toolkit for UESQLTableAsset.
 */
class FUnrealExtendedSQLEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	/** Registered asset type actions handles. */
	TArray<TSharedRef<class IAssetTypeActions>> RegisteredAssetTypeActions;
	TSharedPtr<struct FGraphPanelPinFactory> SQLVisualPinFactory;
	TSharedPtr<struct FGraphPanelNodeFactory> SQLVisualNodeFactory;
};
