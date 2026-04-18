// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "UnrealExtendedSQLEditor.h"
#include "AssetFactory/ESQLTableAssetActions.h"
#include "EdGraphUtilities.h"
#include "K2Node/ESQLK2NodePinFactory.h"
#include "K2Node/ESQLK2NodeVisualFactory.h"
#include "StructContextMenu/ESQLStructMenuExtension.h"
#include "SqlId/ESQLIdCustomization.h"
#include "Shared/ESQLId.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "PropertyEditorModule.h"

DEFINE_LOG_CATEGORY(LogExtendedSQLEditor);

#define LOCTEXT_NAMESPACE "UnrealExtendedSQLEditor"

void FUnrealExtendedSQLEditorModule::StartupModule()
{
	UE_LOG(LogExtendedSQLEditor, Log, TEXT("UnrealExtendedSQLEditor module starting up"));

	// Register asset type actions
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	TSharedRef<FESQLTableAssetActions> TableAssetActions = MakeShareable(new FESQLTableAssetActions());
	AssetTools.RegisterAssetTypeActions(TableAssetActions);
	RegisteredAssetTypeActions.Add(TableAssetActions);

	// Register struct context menu extender
	FESQLStructMenuExtension::Register();

	SQLVisualPinFactory = MakeShared<FESQLK2NodePinFactory>();
	FEdGraphUtilities::RegisterVisualPinFactory(SQLVisualPinFactory);
	SQLVisualNodeFactory = MakeShared<FESQLK2NodeVisualFactory>();
	FEdGraphUtilities::RegisterVisualNodeFactory(SQLVisualNodeFactory);

	// Register FESQLId property customization
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomPropertyTypeLayout(
		FESQLId::StaticStruct()->GetFName(),
		FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FESQLIdCustomization::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();

	UE_LOG(LogExtendedSQLEditor, Log, TEXT("UnrealExtendedSQLEditor module started"));
}

void FUnrealExtendedSQLEditorModule::ShutdownModule()
{
	// Unregister FESQLId property customization
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout(FESQLId::StaticStruct()->GetFName());
		PropertyModule.NotifyCustomizationModuleChanged();
	}

	// Unregister struct context menu extender
	FESQLStructMenuExtension::Unregister();

	if (SQLVisualPinFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualPinFactory(SQLVisualPinFactory);
		SQLVisualPinFactory.Reset();
	}

	if (SQLVisualNodeFactory.IsValid())
	{
		FEdGraphUtilities::UnregisterVisualNodeFactory(SQLVisualNodeFactory);
		SQLVisualNodeFactory.Reset();
	}

	// Unregister asset type actions
	if (FModuleManager::Get().IsModuleLoaded("AssetTools"))
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		for (const TSharedRef<IAssetTypeActions>& Action : RegisteredAssetTypeActions)
		{
			AssetTools.UnregisterAssetTypeActions(Action);
		}
	}
	RegisteredAssetTypeActions.Empty();

	UE_LOG(LogExtendedSQLEditor, Log, TEXT("UnrealExtendedSQLEditor module shut down"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealExtendedSQLEditorModule, UnrealExtendedSQLEditor)
