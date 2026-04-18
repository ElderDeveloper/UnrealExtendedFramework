// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ESQLStructMenuExtension.h"
#include "TableAsset/ESQLTableAsset.h"
#include "TableAsset/ESQLStructValidator.h"
#include "Validation/SESQLValidationDialog.h"
#include "UnrealExtendedSQLEditor.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Engine/UserDefinedStruct.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"


#define LOCTEXT_NAMESPACE "ESQLStructMenuExtension"

FDelegateHandle FESQLStructMenuExtension::ContentBrowserExtenderHandle;


void FESQLStructMenuExtension::Register()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	// Add our extender to the Content Browser's asset context menu
	TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates =
		ContentBrowserModule.GetAllAssetViewContextMenuExtenders();

	CBMenuExtenderDelegates.Add(
		FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&OnExtendContentBrowserAssetMenu)
	);

	// Store the handle so we can remove it later
	ContentBrowserExtenderHandle = CBMenuExtenderDelegates.Last().GetHandle();

	UE_LOG(LogExtendedSQLEditor, Log, TEXT("Registered struct context menu extension"));
}

void FESQLStructMenuExtension::Unregister()
{
	if (ContentBrowserExtenderHandle.IsValid())
	{
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
		TArray<FContentBrowserMenuExtender_SelectedAssets>& CBMenuExtenderDelegates =
			ContentBrowserModule.GetAllAssetViewContextMenuExtenders();

		CBMenuExtenderDelegates.RemoveAll([](const FContentBrowserMenuExtender_SelectedAssets& Delegate)
		{
			return Delegate.GetHandle() == ContentBrowserExtenderHandle;
		});

		ContentBrowserExtenderHandle.Reset();
	}
}

TSharedRef<FExtender> FESQLStructMenuExtension::OnExtendContentBrowserAssetMenu(
	const TArray<FAssetData>& SelectedAssets)
{
	TSharedRef<FExtender> Extender = MakeShareable(new FExtender());

	// Only add our entry if the selection contains UScriptStruct assets (Blueprint structs)
	bool bHasStruct = false;
	for (const FAssetData& Asset : SelectedAssets)
	{
		if (Asset.AssetClassPath == UUserDefinedStruct::StaticClass()->GetClassPathName())
		{
			bHasStruct = true;
			break;
		}
	}

	if (bHasStruct)
	{
		Extender->AddMenuExtension(
			"GetAssetActions",
			EExtensionHook::After,
			nullptr,
			FMenuExtensionDelegate::CreateStatic(&AddMenuEntries, SelectedAssets)
		);
	}

	return Extender;
}

void FESQLStructMenuExtension::AddMenuEntries(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
{
	MenuBuilder.AddMenuEntry(
		LOCTEXT("CreateSQLTable", "Create SQL Table from Struct"),
		LOCTEXT("CreateSQLTableTooltip", "Create a new SQL Table Asset backed by SQLite using this struct as the row type"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([SelectedAssets]()
		{
			for (const FAssetData& Asset : SelectedAssets)
			{
				if (Asset.AssetClassPath == UUserDefinedStruct::StaticClass()->GetClassPathName())
				{
					ExecuteCreateSQLTable(Asset);
					break; // Only create from the first selected struct
				}
			}
		}))
	);
}

void FESQLStructMenuExtension::ExecuteCreateSQLTable(const FAssetData& StructAsset)
{
	// Load the struct
	UObject* LoadedObject = StructAsset.GetAsset();
	const UScriptStruct* Struct = Cast<UScriptStruct>(LoadedObject);

	if (!Struct)
	{
		UE_LOG(LogExtendedSQLEditor, Warning, TEXT("Failed to load struct from asset: %s"), *StructAsset.AssetName.ToString());
		return;
	}

	// Validate the struct
	TArray<FESQLStructValidator::FFieldResult> Results;
	if (!FESQLStructValidator::Validate(Struct, Results))
	{
		// Show validation dialog — user must fix the struct first
		SESQLValidationDialog::Show(Struct, Results);
		return;
	}

	// Create the SQL Table Asset
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	const FString DefaultPath = FPaths::GetPath(StructAsset.GetObjectPathString());
	const FString AssetName = TEXT("SQLT_") + Struct->GetName();

	UESQLTableAsset* NewAsset = Cast<UESQLTableAsset>(
		AssetTools.CreateAsset(AssetName, DefaultPath, UESQLTableAsset::StaticClass(), nullptr)
	);

	if (NewAsset)
	{
		NewAsset->RowStruct = Struct;
		NewAsset->TableName = Struct->GetName();
		NewAsset->MarkPackageDirty();

		UE_LOG(LogExtendedSQLEditor, Log, TEXT("Created SQL Table Asset '%s' from struct '%s'"),
			*AssetName, *Struct->GetName());
	}
}

#undef LOCTEXT_NAMESPACE
