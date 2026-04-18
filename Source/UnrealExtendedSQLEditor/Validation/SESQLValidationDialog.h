// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "TableAsset/ESQLStructValidator.h"


/**
 * Modal Slate dialog that shows a field-by-field SQLite compatibility report.
 *
 * Displays each field with:
 *   ✅ FieldName   UETypeName   → SQLiteType
 *   ❌ FieldName   UETypeName   → NOT SUPPORTED (reason)
 *
 * Shown when a user tries to create a SQL Table from a struct that has
 * unsupported field types. The user must fix the struct and try again.
 */
class SESQLValidationDialog : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SESQLValidationDialog) {}
		SLATE_ARGUMENT(const UScriptStruct*, Struct)
		SLATE_ARGUMENT(TArray<FESQLStructValidator::FFieldResult>, Results)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Static convenience — opens the dialog as a modal window. */
	static void Show(
		const UScriptStruct* Struct,
		const TArray<FESQLStructValidator::FFieldResult>& Results
	);

private:

	TArray<TSharedPtr<FESQLStructValidator::FFieldResult>> ResultItems;

	TSharedRef<ITableRow> OnGenerateRow(
		TSharedPtr<FESQLStructValidator::FFieldResult> Item,
		const TSharedRef<STableViewBase>& OwnerTable
	);

	/** Count of invalid fields. */
	int32 InvalidCount = 0;
	int32 TotalCount = 0;

	/** Reference to the owning window (for closing). */
	TWeakPtr<SWindow> OwningWindow;
};
