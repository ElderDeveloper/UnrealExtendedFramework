// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"


/**
 * Extends the Content Browser context menu for UScriptStruct ASSETS.
 * Adds: "Create SQL Table from Struct"
 *
 * This is a CONVENIENCE SHORTCUT for Blueprint structs that are visible
 * in the Content Browser. For C++ structs (not CB assets),
 * use the UESQLTableAssetFactory pathway: Add New → SQL → SQL Table.
 *
 * Registered during UnrealExtendedSQLEditor module startup via
 * FContentBrowserModule::GetAllAssetContextMenuExtenders().
 */
class FESQLStructMenuExtension
{
public:

	/** Register the context menu extender with the Content Browser. */
	static void Register();

	/** Unregister on module shutdown. */
	static void Unregister();

private:

	/** Called by the Content Browser to extend the asset context menu. */
	static TSharedRef<FExtender> OnExtendContentBrowserAssetMenu(
		const TArray<FAssetData>& SelectedAssets
	);

	/** Builds the actual menu entries. */
	static void AddMenuEntries(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets);

	/** The actual menu entry callback — creates a SQL Table from the selected struct. */
	static void ExecuteCreateSQLTable(const FAssetData& StructAsset);

	static FDelegateHandle ContentBrowserExtenderHandle;
};
