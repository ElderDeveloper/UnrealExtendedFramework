// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ESQLTableAssetFactory.h"
#include "ESQLTableAssetActions.h"
#include "TableAsset/ESQLTableAsset.h"
#include "TableAsset/ESQLStructValidator.h"
#include "Validation/SESQLValidationDialog.h"
#include "UnrealExtendedSQLEditor.h"
#include "Kismet2/SClassPickerDialog.h"
#include "Engine/UserDefinedStruct.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/Application/SlateApplication.h"


#define LOCTEXT_NAMESPACE "ESQLTableAssetFactory"


UESQLTableAssetFactory::UESQLTableAssetFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UESQLTableAsset::StaticClass();
}

FText UESQLTableAssetFactory::GetDisplayName() const
{
	return LOCTEXT("DisplayName", "SQL Table");
}

uint32 UESQLTableAssetFactory::GetMenuCategories() const
{
	return FESQLTableAssetActions::GetSQLAssetCategory();
}

bool UESQLTableAssetFactory::ConfigureProperties()
{
	SelectedStruct = nullptr;

	if (!ShowStructPicker())
	{
		return false;
	}

	if (!SelectedStruct)
	{
		return false;
	}

	// Validate the selected struct
	TArray<FESQLStructValidator::FFieldResult> Results;
	if (!FESQLStructValidator::Validate(SelectedStruct, Results))
	{
		// Show validation dialog
		SESQLValidationDialog::Show(SelectedStruct, Results);
		SelectedStruct = nullptr;
		return false;
	}

	return true;
}

bool UESQLTableAssetFactory::ShowStructPicker()
{
	// Use the same struct picker approach as UDataTableFactory
	// Create a class viewer filter that only shows UScriptStruct types

	bool bPicked = false;
	const UScriptStruct* PickedStruct = nullptr;

	// Build a struct picker window
	TSharedRef<SWindow> PickerWindow = SNew(SWindow)
		.Title(LOCTEXT("StructPickerTitle", "Pick Row Struct for SQL Table"))
		.ClientSize(FVector2D(400, 500))
		.SupportsMinimize(false)
		.SupportsMaximize(false);

	// We use the class picker approach — SClassPickerDialog with UScriptStruct filter
	// However, UE's struct picker for DataTable uses a custom approach.
	// We'll use a simplified struct enumeration with a list view.

	TArray<const UScriptStruct*> AllStructs;

	// Gather all UScriptStruct types
	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		const UScriptStruct* Struct = *It;
		if (!Struct) continue;

		// Skip internal/engine structs that aren't useful as row types
		// Include all user structs and Blueprint structs
		const FString StructPath = Struct->GetPathName();

		// Include structs from game modules and blueprints
		if (StructPath.StartsWith(TEXT("/Script/Engine")) ||
			StructPath.StartsWith(TEXT("/Script/CoreUObject")))
		{
			continue;
		}

		AllStructs.Add(Struct);
	}

	// Sort alphabetically
	AllStructs.Sort([](const UScriptStruct& A, const UScriptStruct& B)
	{
		return A.GetName() < B.GetName();
	});

	// Build the list
	TSharedPtr<SListView<const UScriptStruct*>> ListView;
	const UScriptStruct* CurrentSelection = nullptr;
	TSharedRef<SWidget> PickerContent = SNew(SVerticalBox)

		// Search filter (simplified — future: add SSearchBox integration)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4)
		[
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("StructCount", "Available structs: {0}"), FText::AsNumber(AllStructs.Num())))
		]

		// List view
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(4)
		[
			SAssignNew(ListView, SListView<const UScriptStruct*>)
			.ListItemsSource(&AllStructs)
			.OnGenerateRow_Lambda([](const UScriptStruct* Item, const TSharedRef<STableViewBase>& Owner)
			{
				return SNew(STableRow<const UScriptStruct*>, Owner)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Item->GetName()))
					.ToolTipText(FText::FromString(Item->GetPathName()))
					.Margin(FMargin(4, 2))
				];
			})
			.OnSelectionChanged_Lambda([&CurrentSelection](const UScriptStruct* Item, ESelectInfo::Type)
			{
				CurrentSelection = Item;
			})
			.OnMouseButtonDoubleClick_Lambda([&bPicked, &PickedStruct, &PickerWindow](const UScriptStruct* Item)
			{
				PickedStruct = Item;
				bPicked = true;
				PickerWindow->RequestDestroyWindow();
			})
		]

		// Buttons
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4)
		.HAlign(HAlign_Right)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("Select", "Select"))
				.OnClicked_Lambda([&bPicked, &PickedStruct, &CurrentSelection, &PickerWindow]()
				{
					if (CurrentSelection)
					{
						PickedStruct = CurrentSelection;
						bPicked = true;
						PickerWindow->RequestDestroyWindow();
					}
					return FReply::Handled();
				})
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4, 0)
			[
				SNew(SButton)
				.Text(LOCTEXT("Cancel", "Cancel"))
				.OnClicked_Lambda([&PickerWindow]()
				{
					PickerWindow->RequestDestroyWindow();
					return FReply::Handled();
				})
			]
		];

	PickerWindow->SetContent(PickerContent);

	// Show as modal
	GEditor->EditorAddModalWindow(PickerWindow);

	if (bPicked && PickedStruct)
	{
		SelectedStruct = PickedStruct;
		return true;
	}

	return false;
}

UObject* UESQLTableAssetFactory::FactoryCreateNew(
	UClass* InClass,
	UObject* InParent,
	FName InName,
	EObjectFlags Flags,
	UObject* Context,
	FFeedbackContext* Warn)
{
	check(SelectedStruct);

	UESQLTableAsset* NewAsset = NewObject<UESQLTableAsset>(InParent, InClass, InName, Flags);
	if (NewAsset)
	{
		NewAsset->RowStruct = SelectedStruct;
		NewAsset->TableName = SelectedStruct->GetName();
		// DatabaseName and other properties keep their defaults
		// The actual CREATE TABLE happens when the editor or subsystem prepares the table context

		UE_LOG(LogExtendedSQLEditor, Log, TEXT("Created SQL Table Asset '%s' with struct '%s'"),
			*InName.ToString(), *SelectedStruct->GetName());
	}

	return NewAsset;
}

#undef LOCTEXT_NAMESPACE
