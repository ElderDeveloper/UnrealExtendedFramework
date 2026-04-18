// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "SESQLValidationDialog.h"
#include "UnrealExtendedSQLEditor.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"


#define LOCTEXT_NAMESPACE "SESQLValidationDialog"


void SESQLValidationDialog::Construct(const FArguments& InArgs)
{
	const UScriptStruct* Struct = InArgs._Struct;
	TArray<FESQLStructValidator::FFieldResult> Results = InArgs._Results;

	TotalCount = Results.Num();
	InvalidCount = 0;

	// Convert to shared pointers for the list view and count invalids
	for (FESQLStructValidator::FFieldResult& Result : Results)
	{
		if (!Result.bIsValid) ++InvalidCount;
		ResultItems.Add(MakeShareable(new FESQLStructValidator::FFieldResult(Result)));
	}

	const FString StructName = Struct ? Struct->GetName() : TEXT("Unknown");

	ChildSlot
	[
		SNew(SVerticalBox)

		// Title
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8, 8, 8, 4)
		[
			SNew(STextBlock)
			.Text(FText::Format(
				LOCTEXT("Title", "⚠  SQL Table Validation — {0}"),
				FText::FromString(StructName)))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
		]

		// Summary
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8, 4)
		[
			SNew(STextBlock)
			.Text(FText::Format(
				LOCTEXT("Summary", "{0} of {1} fields are not compatible with SQLite."),
				FText::AsNumber(InvalidCount),
				FText::AsNumber(TotalCount)))
			.ColorAndOpacity(FLinearColor(1.0f, 0.4f, 0.4f))
		]

		// Field list
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(8, 4)
		[
			SNew(SBox)
			.MinDesiredHeight(200)
			[
				SNew(SListView<TSharedPtr<FESQLStructValidator::FFieldResult>>)
				.ListItemsSource(&ResultItems)
				.OnGenerateRow(this, &SESQLValidationDialog::OnGenerateRow)
				.HeaderRow(
					SNew(SHeaderRow)
					+ SHeaderRow::Column("Status")
					.DefaultLabel(LOCTEXT("Status", ""))
					.FixedWidth(30)
					+ SHeaderRow::Column("Field")
					.DefaultLabel(LOCTEXT("Field", "Field"))
					.FillWidth(0.3f)
					+ SHeaderRow::Column("UEType")
					.DefaultLabel(LOCTEXT("UEType", "UE Type"))
					.FillWidth(0.25f)
					+ SHeaderRow::Column("SQLType")
					.DefaultLabel(LOCTEXT("SQLType", "SQLite Type"))
					.FillWidth(0.2f)
					+ SHeaderRow::Column("Error")
					.DefaultLabel(LOCTEXT("Error", "Error"))
					.FillWidth(0.4f)
				)
			]
		]

		// Help text
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8, 4)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("HelpText", "Fix the struct and try again. Unsupported types include: TArray, TMap, TSet, nested UStruct, UObject*, FVector, FRotator, Delegates."))
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		]

		// OK button
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8)
		.HAlign(HAlign_Right)
		[
			SNew(SButton)
			.Text(LOCTEXT("OK", "OK"))
			.OnClicked_Lambda([this]()
			{
				if (OwningWindow.IsValid())
				{
					OwningWindow.Pin()->RequestDestroyWindow();
				}
				return FReply::Handled();
			})
		]
	];
}

TSharedRef<ITableRow> SESQLValidationDialog::OnGenerateRow(
	TSharedPtr<FESQLStructValidator::FFieldResult> Item,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	const bool bValid = Item->bIsValid;

	return SNew(STableRow<TSharedPtr<FESQLStructValidator::FFieldResult>>, OwnerTable)
	[
		SNew(SHorizontalBox)

		// Status icon
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4, 2)
		[
			SNew(STextBlock)
			.Text(FText::FromString(bValid ? TEXT("✅") : TEXT("❌")))
		]

		// Field name
		+ SHorizontalBox::Slot()
		.FillWidth(0.3f)
		.VAlign(VAlign_Center)
		.Padding(4, 2)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Item->FieldName))
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
		]

		// UE type
		+ SHorizontalBox::Slot()
		.FillWidth(0.25f)
		.VAlign(VAlign_Center)
		.Padding(4, 2)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Item->UETypeName))
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		]

		// SQLite type
		+ SHorizontalBox::Slot()
		.FillWidth(0.2f)
		.VAlign(VAlign_Center)
		.Padding(4, 2)
		[
			SNew(STextBlock)
			.Text(FText::FromString(bValid ? Item->SQLiteType : TEXT("—")))
			.ColorAndOpacity(bValid
				? FLinearColor(0.3f, 0.9f, 0.3f)
				: FLinearColor(0.6f, 0.6f, 0.6f))
		]

		// Error reason
		+ SHorizontalBox::Slot()
		.FillWidth(0.4f)
		.VAlign(VAlign_Center)
		.Padding(4, 2)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Item->ErrorReason))
			.AutoWrapText(true)
			.ColorAndOpacity(FLinearColor(1.0f, 0.4f, 0.4f))
		]
	];
}


// ── Static Show ──────────────────────────────────────────────────────────────

void SESQLValidationDialog::Show(
	const UScriptStruct* Struct,
	const TArray<FESQLStructValidator::FFieldResult>& Results)
{
	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "SQL Table Struct Validation"))
		.ClientSize(FVector2D(700, 450))
		.SupportsMinimize(false)
		.SupportsMaximize(false);

	TSharedRef<SESQLValidationDialog> Dialog = SNew(SESQLValidationDialog)
		.Struct(Struct)
		.Results(Results);

	Dialog->OwningWindow = Window;
	Window->SetContent(Dialog);

	// Show as modal
	GEditor->EditorAddModalWindow(Window);
}

#undef LOCTEXT_NAMESPACE
