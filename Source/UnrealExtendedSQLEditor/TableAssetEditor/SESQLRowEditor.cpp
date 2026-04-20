// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
// Adapted from Epic Games SRowEditor — struct-aware property preview preserved,
// all data reads go through FESQLDatabase.

#include "SESQLRowEditor.h"
#include "TableAsset/ESQLTableAsset.h"
#include "Core/ESQLDatabase.h"
#include "Shared/ESQLPropertySerializer.h"
#include "UnrealExtendedSQLEditor.h"

#include "IStructureDetailsView.h"
#include "PropertyEditorModule.h"
#include "UObject/StructOnScope.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "SESQLRowEditor"


void SESQLRowEditor::Construct(const FArguments& InArgs, UESQLTableAsset* InTableAsset, TSharedPtr<FESQLDatabase> InDatabase)
{
	TableAsset = InTableAsset;
	Database = InDatabase;
	SQLTableEditor = InArgs._SQLTableEditor;

	RebuildStructInstance();

	// Create the details view
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FStructureDetailsViewArgs StructureViewArgs;
	StructureViewArgs.bShowObjects = false;
	StructureViewArgs.bShowAssets = true;
	StructureViewArgs.bShowClasses = false;
	StructureViewArgs.bShowInterfaces = false;

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.bShowScrollBar = true;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.bAllowMultipleTopLevelObjects = false;

	StructureDetailView = PropertyModule.CreateStructureDetailView(DetailsViewArgs, StructureViewArgs, StructInstance);

	ChildSlot
	[
		SNew(SVerticalBox)

		// Row name selector
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4)
		[
			BuildRowNameComboBox()
		]

		// Struct property editor
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SBox)
			.IsEnabled(false)
			[
				StructureDetailView.IsValid()
					? StructureDetailView->GetWidget().ToSharedRef()
					: SNullWidget::NullWidget
			]
		]
	];

	// Build the row names list
	Refresh();
}

SESQLRowEditor::~SESQLRowEditor()
{
	ReleaseStructInstance();
}

bool SESQLRowEditor::RebuildStructInstance()
{
	ReleaseStructInstance();

	if (!TableAsset || !TableAsset->RowStruct)
	{
		return false;
	}

	StructInstance = MakeShareable(new FStructOnScope(TableAsset->RowStruct));

	UPackage* StructPackage = TableAsset->GetPackage();
	if ((!StructPackage || StructPackage == GetTransientPackage()) && TableAsset->RowStruct)
	{
		StructPackage = TableAsset->RowStruct->GetPackage();
	}

	if (StructInstance.IsValid() && StructPackage && StructPackage != GetTransientPackage())
	{
		StructInstance->SetPackage(StructPackage);
	}

	if (StructureDetailView.IsValid())
	{
		StructureDetailView->SetStructureData(StructInstance);
	}

	return StructInstance.IsValid();
}

void SESQLRowEditor::ReleaseStructInstance()
{
	if (StructureDetailView.IsValid())
	{
		StructureDetailView->SetStructureData(TSharedPtr<FStructOnScope>());
	}

	StructInstance.Reset();
}


TSharedRef<SWidget> SESQLRowEditor::BuildRowNameComboBox()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0, 0, 4, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("RowLabel", "Row:"))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SAssignNew(RowNameComboBox,
				SComboBox<TSharedPtr<FName>>)
			.OptionsSource(&RowNames)
			.OnGenerateWidget(this, &SESQLRowEditor::OnGenerateRowNameWidget)
			.OnSelectionChanged(this, &SESQLRowEditor::OnRowNameSelected)
			.Content()
			[
				SNew(STextBlock)
				.Text_Lambda([this]() -> FText
				{
					return FText::FromName(SelectedRowName);
				})
			]
		];
}

TSharedRef<SWidget> SESQLRowEditor::OnGenerateRowNameWidget(TSharedPtr<FName> InItem)
{
	return SNew(STextBlock)
		.Text(FText::FromName(InItem.IsValid() ? *InItem : NAME_None));
}

void SESQLRowEditor::OnRowNameSelected(TSharedPtr<FName> InRowName, ESelectInfo::Type SelectInfo)
{
	if (InRowName.IsValid())
	{
		SelectRow(*InRowName);
	}
}


void SESQLRowEditor::Refresh()
{
	if (!Database || !Database->IsOpen() || !TableAsset) return;

	RowNames.Reset();

	// Query all row names (PKs)
	FString SQL = FString::Printf(TEXT("SELECT \"%s\" FROM \"%s\" ORDER BY \"%s\""),
		*TableAsset->PrimaryKeyColumn, *TableAsset->TableName, *TableAsset->PrimaryKeyColumn);

	FESQLQueryResult Result = Database->Execute(SQL);
	if (Result.bSuccess)
	{
		for (const FESQLRow& Row : Result.Rows)
		{
			const FString* PKValue = Row.Columns.Find(TableAsset->PrimaryKeyColumn);
			if (PKValue)
			{
				RowNames.Add(MakeShareable(new FName(**PKValue)));
			}
		}
	}
}

void SESQLRowEditor::SelectRow(FName RowName)
{
	SelectedRowName = RowName;
	LoadRowDataIntoStruct();
}

bool SESQLRowEditor::LoadRowDataIntoStruct()
{
	if (!Database || !Database->IsOpen() || !TableAsset || !TableAsset->RowStruct) return false;
	if (SelectedRowName == NAME_None) return false;

	// Query the row
	FString SQL = FString::Printf(TEXT("SELECT * FROM \"%s\" WHERE \"%s\" = ?1"),
		*TableAsset->TableName, *TableAsset->PrimaryKeyColumn);
	TArray<FString> Bindings = { SelectedRowName.ToString() };

	FESQLQueryResult Result = Database->Execute(SQL, Bindings);
	if (!Result.bSuccess || Result.Rows.Num() == 0) return false;

	const FESQLRow& SQLRow = Result.Rows[0];

	// Map SQL column values back into the struct
	const UScriptStruct* StructType = TableAsset->RowStruct;
	if (!RebuildStructInstance()) return false;

	void* StructData = StructInstance->GetStructMemory();

	for (TFieldIterator<FProperty> It(StructType); It; ++It)
	{
		FProperty* Property = *It;
		const FString* ValueStr = SQLRow.Columns.Find(Property->GetName());
		if (!ValueStr) continue;

		const bool bIsNull = SQLRow.NullColumns.Contains(Property->GetName());
		FESQLPropertySerializer::DeserializePropertyFromString(Property, StructData, *ValueStr, bIsNull);
	}

	return true;
}


void SESQLRowEditor::HandleTableDataChanged()
{
	Refresh();
	if (SelectedRowName != NAME_None)
	{
		LoadRowDataIntoStruct();
	}
}

void SESQLRowEditor::HandleStructPreChange()
{
	ReleaseStructInstance();
}

void SESQLRowEditor::HandleStructPostChange()
{
	Refresh();

	if (SelectedRowName != NAME_None)
	{
		if (LoadRowDataIntoStruct())
		{
			return;
		}
	}

	RebuildStructInstance();
}

#undef LOCTEXT_NAMESPACE
