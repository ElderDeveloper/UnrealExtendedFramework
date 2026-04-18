// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"


/**
 * Asset type actions for UESQLTableAsset.
 * Controls: thumbnail color, category, right-click actions, double-click behavior.
 *
 * Registered in UnrealExtendedSQLEditor module startup.
 */
class FESQLTableAssetActions : public FAssetTypeActions_Base
{
public:

	// ── FAssetTypeActions_Base interface ──────────────────────────────────

	virtual UClass* GetSupportedClass() const override;
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual uint32 GetCategories() override;

	virtual void OpenAssetEditor(
		const TArray<UObject*>& InObjects,
		TSharedPtr<IToolkitHost> EditWithinLevelEditor
	) override;

	/** Custom "SQL" category for the Content Browser "Add New" menu. */
	static EAssetTypeCategories::Type GetSQLAssetCategory();

private:
	static EAssetTypeCategories::Type SQLAssetCategory;
};
