// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ESQLTableAssetActions.h"
#include "TableAsset/ESQLTableAsset.h"
#include "TableAssetEditor/FESQLTableEditorToolkit.h"
#include "UnrealExtendedSQLEditor.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"


#define LOCTEXT_NAMESPACE "ESQLTableAssetActions"

EAssetTypeCategories::Type FESQLTableAssetActions::SQLAssetCategory = EAssetTypeCategories::None;

UClass* FESQLTableAssetActions::GetSupportedClass() const
{
	return UESQLTableAsset::StaticClass();
}

FText FESQLTableAssetActions::GetName() const
{
	return LOCTEXT("AssetName", "SQL Table");
}

FColor FESQLTableAssetActions::GetTypeColor() const
{
	return FColor(56, 178, 172);  // Teal
}

uint32 FESQLTableAssetActions::GetCategories()
{
	return GetSQLAssetCategory();
}

EAssetTypeCategories::Type FESQLTableAssetActions::GetSQLAssetCategory()
{
	if (SQLAssetCategory == EAssetTypeCategories::None)
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		SQLAssetCategory = AssetTools.RegisterAdvancedAssetCategory(
			FName(TEXT("SQL")),
			LOCTEXT("SQLCategory", "SQL")
		);
	}
	return SQLAssetCategory;
}

void FESQLTableAssetActions::OpenAssetEditor(
	const TArray<UObject*>& InObjects,
	TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid()
		? EToolkitMode::WorldCentric
		: EToolkitMode::Standalone;

	for (UObject* Obj : InObjects)
	{
		if (UESQLTableAsset* TableAsset = Cast<UESQLTableAsset>(Obj))
		{
			UE_LOG(LogExtendedSQLEditor, Log, TEXT("Opening SQL Table Editor: %s"), *TableAsset->GetName());

			TSharedRef<FESQLTableEditorToolkit> Editor = MakeShareable(new FESQLTableEditorToolkit());
			Editor->InitSQLTableEditor(Mode, EditWithinLevelEditor, TableAsset);
		}
	}
}

#undef LOCTEXT_NAMESPACE
