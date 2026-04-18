// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "ESQLTableAssetFactory.generated.h"


/**
 * UFactory for creating UESQLTableAsset from the Content Browser.
 *
 * Appears under: Add New → SQL → SQL Table
 *
 * On creation, shows a struct picker dialog (just like DataTable creation)
 * that lists ALL registered UScriptStructs — including C++ ones.
 * After picking, validates the struct via FESQLStructValidator.
 */
UCLASS()
class UESQLTableAssetFactory : public UFactory
{
	GENERATED_BODY()

public:

	UESQLTableAssetFactory();

	// ── UFactory interface ───────────────────────────────────────────────

	virtual UObject* FactoryCreateNew(
		UClass* InClass,
		UObject* InParent,
		FName InName,
		EObjectFlags Flags,
		UObject* Context,
		FFeedbackContext* Warn
	) override;

	virtual bool ShouldShowInNewMenu() const override { return true; }

	/** Called before the "name your asset" step.
	    Opens the struct picker + validation. Returns false to cancel. */
	virtual bool ConfigureProperties() override;

	virtual FText GetDisplayName() const override;
	virtual uint32 GetMenuCategories() const override;

private:

	/** The struct the user picked in the struct picker dialog.
	    Set by ConfigureProperties(), used by FactoryCreateNew(). */
	UPROPERTY()
	const UScriptStruct* SelectedStruct = nullptr;

	/** Opens the UE struct picker (same widget DataTable uses).
	    Filters to show all UScriptStructs and validates selection. */
	bool ShowStructPicker();
};
