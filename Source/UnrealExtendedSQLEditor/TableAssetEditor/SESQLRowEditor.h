// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
// Adapted from Epic Games SRowEditor — struct-aware row preview preserved,
// data reads go through FESQLDatabase.

#pragma once

#include "CoreMinimal.h"
#include "FESQLTableEditorToolkit.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class IDetailsView;
class IStructureDetailsView;
class UESQLTableAsset;
class FESQLDatabase;
class FStructOnScope;


/**
 * Inline row preview for SQL Table Assets.
 * Shows the currently selected row's fields as a struct details view
 * (using IStructureDetailsView) without mutating the preview database.
 *
 * Adapted from SRowEditor — the struct-aware property editing, field layout,
 * and change notification are preserved. UDataTable is replaced with
 * UESQLTableAsset + FESQLDatabase.
 */
class SESQLRowEditor : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SESQLRowEditor) {}
		SLATE_ARGUMENT(TWeakPtr<FESQLTableEditorToolkit>, SQLTableEditor)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UESQLTableAsset* InTableAsset, TSharedPtr<FESQLDatabase> InDatabase);

	/** Select a row by name for editing. Loads data from the database. */
	void SelectRow(FName RowName);

	/** Get the currently selected row name. */
	FName GetSelectedRowName() const { return SelectedRowName; }

	/** Refresh the details view from the database. */
	void Refresh();

	/** Notification: the table asset data has changed externally. */
	void HandleTableDataChanged();

	/** Release the live struct scope before a user-defined struct recompiles. */
	void HandleStructPreChange();

	/** Rebuild the struct scope after a user-defined struct recompiles. */
	void HandleStructPostChange();

	virtual ~SESQLRowEditor();

private:

	/** Build the row name combo box. */
	TSharedRef<SWidget> BuildRowNameComboBox();

	/** Callback for row name combo box selection. */
	void OnRowNameSelected(TSharedPtr<FName> InRowName, ESelectInfo::Type SelectInfo);

	/** Generate a combo box entry. */
	TSharedRef<SWidget> OnGenerateRowNameWidget(TSharedPtr<FName> InItem);

	/** Load the selected row's data from the database into the struct instance. */
	bool LoadRowDataIntoStruct();

	bool RebuildStructInstance();
	void ReleaseStructInstance();


	/** The SQL Table Asset being edited. */
	UESQLTableAsset* TableAsset = nullptr;

	/** Database connection. */
	TSharedPtr<FESQLDatabase> Database;

	/** Owning SQL table editor. Used to refresh the grid after preview updates. */
	TWeakPtr<FESQLTableEditorToolkit> SQLTableEditor;

	/** Currently selected row name. */
	FName SelectedRowName;

	/** Struct instance that mirrors the selected row's data. */
	TSharedPtr<FStructOnScope> StructInstance;

	/** Details view showing the struct's properties. */
	TSharedPtr<IStructureDetailsView> StructureDetailView;

	/** Available row names for the combo box. */
	TArray<TSharedPtr<FName>> RowNames;

	/** Combo box for selecting the current row. */
	TSharedPtr<SWidget> RowNameComboBox;

};
