// Copyright Moon Punch Games. All Rights Reserved.

#include "SEELogFileTab.h"

#if WITH_EDITOR

#include "EFLogViewerSettings.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "SourceCodeNavigation.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/SoftObjectPath.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "EELogFileTab"

namespace EELogFileTabPrivate
{
	static const FName ColumnTime(TEXT("Time"));
	static const FName ColumnCategory(TEXT("Category"));
	static const FName ColumnFrame(TEXT("Frame"));
	static const FName ColumnInstance(TEXT("Instance"));
	static const FName ColumnLevel(TEXT("Level"));
	static const FName ColumnSource(TEXT("Source"));
	static const FName ColumnMessage(TEXT("Message"));

	/** The columns the eye menu can hide, in the order they appear. Message always shows. */
	struct FOptionalColumn
	{
		FName Id;
		FText Label;
		FText ToolTip;
	};

	/** The optional columns of a file tab, or of a merged tab (which adds Category), in display order. */
	static const TArray<FOptionalColumn>& GetOptionalColumns(bool bMerged)
	{
		const FOptionalColumn Time { ColumnTime, NSLOCTEXT("EELogFileTab", "ColumnTimeMenu", "Time"), NSLOCTEXT("EELogFileTab", "ColumnTimeMenuTip", "Local time the line was written, to the millisecond.") };
		const FOptionalColumn Category { ColumnCategory, NSLOCTEXT("EELogFileTab", "ColumnCategoryMenu", "Category"), NSLOCTEXT("EELogFileTab", "ColumnCategoryMenuTip", "The file (category) each row of the merged view came from.") };
		const FOptionalColumn Frame { ColumnFrame, NSLOCTEXT("EELogFileTab", "ColumnFrameMenu", "Frame"), NSLOCTEXT("EELogFileTab", "ColumnFrameMenuTip", "Engine frame counter when the line was written.") };
		const FOptionalColumn Instance { ColumnInstance, NSLOCTEXT("EELogFileTab", "ColumnInstanceMenu", "Instance"), NSLOCTEXT("EELogFileTab", "ColumnInstanceMenuTip", "PIE instance that wrote the line: Server, Client 1, ...") };
		const FOptionalColumn Level { ColumnLevel, NSLOCTEXT("EELogFileTab", "ColumnLevelMenu", "Level"), NSLOCTEXT("EELogFileTab", "ColumnLevelMenuTip", "Error, Warning, Display, Log, Verbose, VeryVerbose.") };
		const FOptionalColumn Source { ColumnSource, NSLOCTEXT("EELogFileTab", "ColumnSourceMenu", "Source"), NSLOCTEXT("EELogFileTab", "ColumnSourceMenuTip", "File and line that wrote the line (double-click a row to open it).") };

		static const TArray<FOptionalColumn> FileColumns = { Time, Frame, Instance, Level, Source };
		static const TArray<FOptionalColumn> MergedColumns = { Time, Category, Frame, Instance, Level, Source };
		return bMerged ? MergedColumns : FileColumns;
	}

	/** A merged tab tells its files apart by colour too. Hues a golden angle apart, so neighbours differ. */
	static FSlateColor MakeSourceColor(int32 SourceIndex)
	{
		const uint8 Hue = static_cast<uint8>((SourceIndex * 158 + 20) % 256);
		return FSlateColor(FLinearColor::MakeFromHSV8(Hue, 110, 235));
	}

	static FSlateFontInfo GetRowFont()
	{
		return FCoreStyle::GetDefaultFontStyle("Mono", 9);
	}

	static FSlateColor GetLevelColor(EEFLogVerbosity Verbosity)
	{
		switch (Verbosity)
		{
		case EEFLogVerbosity::Error:       return FSlateColor(FLinearColor(1.0f, 0.36f, 0.33f));
		case EEFLogVerbosity::Warning:     return FSlateColor(FLinearColor(1.0f, 0.78f, 0.28f));
		case EEFLogVerbosity::Verbose:
		case EEFLogVerbosity::VeryVerbose: return FSlateColor::UseSubduedForeground();
		default:                           return FSlateColor::UseForeground();
		}
	}

	/** The level filters in the eye menu, by SEELogFileTab::GetBucket index. Verbose also covers VeryVerbose. */
	struct FLevelBucket
	{
		int32 Bucket;
		FText Label;
		FText ToolTip;
		FSlateColor Color;
	};

	static const TArray<FLevelBucket>& GetLevelBuckets()
	{
		static const TArray<FLevelBucket> Buckets =
		{
			{ 0, NSLOCTEXT("EELogFileTab", "LevelError", "Error"), NSLOCTEXT("EELogFileTab", "LevelErrorTip", "Rows written at Error."), GetLevelColor(EEFLogVerbosity::Error) },
			{ 1, NSLOCTEXT("EELogFileTab", "LevelWarning", "Warning"), NSLOCTEXT("EELogFileTab", "LevelWarningTip", "Rows written at Warning."), GetLevelColor(EEFLogVerbosity::Warning) },
			{ 2, NSLOCTEXT("EELogFileTab", "LevelDisplay", "Display"), NSLOCTEXT("EELogFileTab", "LevelDisplayTip", "Rows written at Display."), GetLevelColor(EEFLogVerbosity::Display) },
			{ 3, NSLOCTEXT("EELogFileTab", "LevelLog", "Log"), NSLOCTEXT("EELogFileTab", "LevelLogTip", "Rows written at Log, and lines from files that are not EF_LOG's."), GetLevelColor(EEFLogVerbosity::Log) },
			{ 4, NSLOCTEXT("EELogFileTab", "LevelVerbose", "Verbose"), NSLOCTEXT("EELogFileTab", "LevelVerboseTip", "Rows written at Verbose or VeryVerbose. A category writes these only once its threshold is raised (tab right-click > Verbosity)."), GetLevelColor(EEFLogVerbosity::Verbose) },
		};
		return Buckets;
	}
}

/** One Line or Raw row: a cell per column. */
class SEELogRowWidget : public SMultiColumnTableRow<TSharedPtr<FEELogRow>>
{
public:
	SLATE_BEGIN_ARGS(SEELogRowWidget) {}
		SLATE_ARGUMENT(TSharedPtr<FEELogRow>, Row)
		/** Merged tab: the file the row came from. */
		SLATE_ARGUMENT(FString, Category)
		SLATE_ARGUMENT(FSlateColor, CategoryColor)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable)
	{
		Row = InArgs._Row;
		Category = InArgs._Category;
		CategoryColor = InArgs._CategoryColor;
		SMultiColumnTableRow<TSharedPtr<FEELogRow>>::Construct(FSuperRowType::FArguments().Padding(FMargin(0.0f, 1.0f)), InOwnerTable);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
	{
		using namespace EELogFileTabPrivate;

		const FSlateColor Color = GetLevelColor(Row->Verbosity);
		const TSharedPtr<FEELogRow> Captured = Row;

		FText Text;
		FText ToolTip;
		if (ColumnName == ColumnTime)
		{
			Text = FText::FromString(Row->Time);
		}
		else if (ColumnName == ColumnCategory)
		{
			return SNew(STextBlock)
				.Font(GetRowFont())
				.ColorAndOpacity(CategoryColor)
				.Text(FText::FromString(Category));
		}
		else if (ColumnName == ColumnFrame)
		{
			Text = FText::FromString(Row->Frame);
		}
		else if (ColumnName == ColumnInstance)
		{
			Text = FText::FromString(Row->Instance);
		}
		else if (ColumnName == ColumnLevel)
		{
			Text = Row->Kind == EEELogRowKind::Line ? FText::FromString(LexToString(Row->Verbosity)) : FText::GetEmpty();
		}
		else if (ColumnName == ColumnSource)
		{
			if (!Row->SourcePath.IsEmpty())
			{
				Text = FText::FromString(FString::Printf(TEXT("%s:%d"), *FPaths::GetCleanFilename(Row->SourcePath), Row->SourceLine));
				ToolTip = FText::FromString(FString::Printf(TEXT("%s:%d (double-click to open)"), *Row->SourcePath, Row->SourceLine));
			}
		}
		else if (ColumnName == ColumnMessage)
		{
			// Bound, not copied: a continuation line can reach this row after it was first drawn.
			return SNew(STextBlock)
				.Font(GetRowFont())
				.ColorAndOpacity(Color)
				.Text_Lambda([Captured]()
				{
					const FString FirstLine = Captured->GetFirstLine();
					return FText::FromString(Captured->ExtraLineCount > 0
						? FString::Printf(TEXT("%s    (+%d lines)"), *FirstLine, Captured->ExtraLineCount)
						: FirstLine);
				})
				.ToolTipText_Lambda([Captured]()
				{
					return Captured->ExtraLineCount > 0 ? FText::FromString(Captured->Message) : FText::GetEmpty();
				});
		}

		return SNew(STextBlock)
			.Font(GetRowFont())
			.ColorAndOpacity(Color)
			.Text(Text)
			.ToolTipText(ToolTip);
	}

private:
	TSharedPtr<FEELogRow> Row;
	FString Category;
	FSlateColor CategoryColor;
};

void SEELogFileTab::Construct(const FArguments& InArgs)
{
	using namespace EELogFileTabPrivate;

	FilePath = InArgs._FilePath;
	Title = InArgs._Title;
	MaxRows = FMath::Max(1000, InArgs._MaxRows);
	LoadChunkBytes = InArgs._LoadChunkBytes;
	bMerged = InArgs._Merged;

	if (!bMerged)
	{
		TUniquePtr<FSource> Source = MakeUnique<FSource>();
		Source->Path = FilePath;
		Source->Title = Title;
		Source->Tail = MakeUnique<FEELogFileTail>(FilePath, LoadChunkBytes);
		Sources.Add(MoveTemp(Source));
	}

	// The empty option is "All instances".
	InstanceOptions.Add(MakeShared<FString>());

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(4.0f, 4.0f, 4.0f, 2.0f))
		[
			BuildToolbar()
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew(ListView, SListView<FRowPtr>)
			.ListItemsSource(&FilteredRows)
			.SelectionMode(ESelectionMode::Multi)
			.OnGenerateRow(this, &SEELogFileTab::GenerateRow)
			.OnContextMenuOpening(this, &SEELogFileTab::BuildRowContextMenu)
			.OnMouseButtonDoubleClick_Lambda([this](FRowPtr Row)
			{
				if (Row.IsValid())
				{
					OpenSource(*Row);
				}
			})
			.OnListViewScrolled(this, &SEELogFileTab::HandleListScrolled)
			.HeaderRow
			(
				// Right-clicking the header offers the same column choice as the eye menu. Message sets
				// ShouldGenerateWidget, which is what keeps it out of both: it can never be hidden.
				SAssignNew(HeaderRow, SHeaderRow)
				.CanSelectGeneratedColumn(true)
				.HiddenColumnsList(GetDefault<UEFLogViewerSettings>()->HiddenColumns)
				.OnHiddenColumnsListChanged(this, &SEELogFileTab::HandleHiddenColumnsChanged)
				+ SHeaderRow::Column(ColumnTime).DefaultLabel(LOCTEXT("ColumnTime", "Time")).ManualWidth(96.0f)
				+ SHeaderRow::Column(ColumnFrame).DefaultLabel(LOCTEXT("ColumnFrame", "Frame")).ManualWidth(72.0f)
				+ SHeaderRow::Column(ColumnInstance).DefaultLabel(LOCTEXT("ColumnInstance", "Instance")).ManualWidth(84.0f)
				+ SHeaderRow::Column(ColumnLevel).DefaultLabel(LOCTEXT("ColumnLevel", "Level")).ManualWidth(74.0f)
				+ SHeaderRow::Column(ColumnSource).DefaultLabel(LOCTEXT("ColumnSource", "Source")).ManualWidth(200.0f)
				+ SHeaderRow::Column(ColumnMessage).DefaultLabel(LOCTEXT("ColumnMessage", "Message")).FillWidth(1.0f).ShouldGenerateWidget(true)
			)
		]
	];

	// Right after Time: in a merged view, which file a row came from is the first thing to read.
	if (bMerged)
	{
		HeaderRow->InsertColumn(SHeaderRow::Column(ColumnCategory).DefaultLabel(LOCTEXT("ColumnCategory", "Category")).ManualWidth(140.0f), 1);
		ApplyHiddenColumnsSetting();
	}

	// Another tab's eye menu changes the shared choice; every tab follows it.
	HiddenColumnsChangedHandle = UEFLogViewerSettings::OnHiddenColumnsChanged().AddSP(this, &SEELogFileTab::ApplyHiddenColumnsSetting);
}

SEELogFileTab::~SEELogFileTab()
{
	UEFLogViewerSettings::OnHiddenColumnsChanged().Remove(HiddenColumnsChangedHandle);
}

TSharedRef<SWidget> SEELogFileTab::BuildToolbar()
{
	using namespace EELogFileTabPrivate;

	return SNew(SVerticalBox)

		// Search, and the eye: which levels and columns show.
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SSearchBox)
				.HintText(bMerged ? LOCTEXT("SearchHintMerged", "Search messages, sources and categories") : LOCTEXT("SearchHint", "Search messages and sources"))
				.OnTextChanged_Lambda([this](const FText& Text)
				{
					FilterText = Text.ToString();
					RebuildFilter();
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(SComboButton)
				.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("SimpleComboButton"))
				.ToolTipText(LOCTEXT("ViewTooltip", "Choose which levels (this tab) and columns (every tab) show."))
				.HasDownArrow(true)
				.OnGetMenuContent(this, &SEELogFileTab::BuildViewMenu)
				.ButtonContent()
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush("Icons.Visible"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
		]

		// View options and file actions.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.0f, 0.0f, 8.0f, 0.0f))
			[
				SAssignNew(InstanceCombo, SComboBox<TSharedPtr<FString>>)
				.OptionsSource(&InstanceOptions)
				.InitiallySelectedItem(InstanceOptions[0])
				.OnGenerateWidget_Lambda([](TSharedPtr<FString> Option)
				{
					return SNew(STextBlock).Text(Option->IsEmpty() ? LOCTEXT("AllInstances", "All instances") : FText::FromString(*Option));
				})
				.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Option, ESelectInfo::Type)
				{
					SelectedInstance = Option.IsValid() ? *Option : FString();
					RebuildFilter();
				})
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						return SelectedInstance.IsEmpty() ? LOCTEXT("AllInstances", "All instances") : FText::FromString(SelectedInstance);
					})
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.0f, 0.0f, 12.0f, 0.0f))
			[
				SNew(SCheckBox)
				.ToolTipText(LOCTEXT("CurrentRunTooltip", "Show only the rows since the last PIE (or Simulate) start marker."))
				.IsChecked_Lambda([this]() { return bCurrentRunOnly ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
				{
					bCurrentRunOnly = State == ECheckBoxState::Checked;
					RebuildFilter();
				})
				[
					SNew(STextBlock).Text(LOCTEXT("CurrentRunOnly", "Current PIE run only"))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(FMargin(0.0f, 0.0f, 12.0f, 0.0f))
			[
				SNew(SCheckBox)
				.ToolTipText(LOCTEXT("FollowTailTooltip", "Keep the newest row in view. Scrolling up pauses it; scrolling back to the end resumes it."))
				.IsChecked_Lambda([this]() { return bFollowTail ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
				{
					bFollowTail = State == ECheckBoxState::Checked;
					ScrollToEndIfFollowing();
				})
				[
					SNew(STextBlock).Text(LOCTEXT("FollowTail", "Follow tail"))
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Text_Lambda([this]()
				{
					if (bMissing)
					{
						return LOCTEXT("FileMissing", "The file is gone (moved or deleted); these rows are what was read.");
					}
					if (bMerged)
					{
						return FText::Format(LOCTEXT("MergedRowCount", "{0} of {1} rows from {2} files"),
							FText::AsNumber(FilteredRows.Num()), FText::AsNumber(Rows.Num()), FText::AsNumber(Sources.Num()));
					}
					return FText::Format(LOCTEXT("RowCount", "{0} of {1} rows"), FText::AsNumber(FilteredRows.Num()), FText::AsNumber(Rows.Num()));
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(SButton)
				.Text(LOCTEXT("LoadEarlier", "Load earlier"))
				.ToolTipText(LOCTEXT("LoadEarlierTooltip", "This file is larger than what was read when the tab opened; read further back."))
				.Visibility_Lambda([this]()
				{
					// A merged view reads each file from the same window its own tab would; older rows are in that tab.
					return !bMerged && Sources[0]->Tail->HasEarlier() && !bTrimmed ? EVisibility::Visible : EVisibility::Collapsed;
				})
				.OnClicked_Lambda([this]()
				{
					LoadEarlier();
					return FReply::Handled();
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(SButton)
				.Text(LOCTEXT("ClearView", "Clear view"))
				.ToolTipText(LOCTEXT("ClearViewTooltip", "Hide the rows read so far. The file is not touched."))
				.OnClicked_Lambda([this]()
				{
					ClearView();
					return FReply::Handled();
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(SButton)
				.Text(LOCTEXT("OpenFile", "Open file"))
				.ToolTipText(LOCTEXT("OpenFileTooltip", "Open the file in the default external editor."))
				.Visibility(bMerged ? EVisibility::Collapsed : EVisibility::Visible)
				.OnClicked_Lambda([this]()
				{
					FPlatformProcess::LaunchFileInDefaultExternalApplication(*FilePath);
					return FReply::Handled();
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(SButton)
				.Text(LOCTEXT("ShowInFolder", "Show in folder"))
				.OnClicked_Lambda([this]()
				{
					FPlatformProcess::ExploreFolder(*FilePath);
					return FReply::Handled();
				})
			]
		];
}

TSharedRef<SWidget> SEELogFileTab::BuildViewMenu()
{
	using namespace EELogFileTabPrivate;

	// Stays open after each click, like the viewport's Show menu, so several entries can be switched
	// in one visit.
	FMenuBuilder Menu(/*bInShouldCloseWindowAfterMenuSelection*/ false, nullptr);

	Menu.AddMenuEntry(
		LOCTEXT("UseDefaults", "Use Defaults"),
		bMerged ? LOCTEXT("UseDefaultsMergedTooltip", "Show every category, level and column.") : LOCTEXT("UseDefaultsTooltip", "Show every level and every column."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Undo"),
		FUIAction(FExecuteAction::CreateSP(this, &SEELogFileTab::UseDefaults)));

	// The files this view merges, in their row colours, counted live.
	if (bMerged)
	{
		Menu.BeginSection(TEXT("Categories"), LOCTEXT("CategoriesSection", "Categories"));
		for (int32 SourceIndex = 0; SourceIndex < Sources.Num(); ++SourceIndex)
		{
			const FSource& Source = *Sources[SourceIndex];
			Menu.AddMenuEntry(
				FUIAction(
					FExecuteAction::CreateSP(this, &SEELogFileTab::ToggleSource, SourceIndex),
					FCanExecuteAction(),
					FIsActionChecked::CreateSP(this, &SEELogFileTab::IsSourceShownAt, SourceIndex)),
				SNew(STextBlock)
					.ColorAndOpacity(Source.Color)
					.Text_Lambda([this, SourceIndex]()
					{
						const FSource& Shown = *Sources[SourceIndex];
						return FText::Format(LOCTEXT("CategoryEntry", "{0}  {1}"), FText::FromString(Shown.Title), FText::AsNumber(Shown.RowCount));
					}),
				NAME_None,
				FText::FromString(Source.Path),
				EUserInterfaceActionType::ToggleButton);
		}
		Menu.EndSection();
	}

	// Levels filter this tab only: each file has its own noise.
	Menu.BeginSection(TEXT("Levels"), LOCTEXT("LevelsSection", "Levels"));
	for (const FLevelBucket& Level : GetLevelBuckets())
	{
		const int32 Bucket = Level.Bucket;
		const FText Label = Level.Label;

		// Coloured, and counted live: the counts stay visible while the menu is open.
		Menu.AddMenuEntry(
			FUIAction(
				FExecuteAction::CreateSP(this, &SEELogFileTab::ToggleLevel, Bucket),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &SEELogFileTab::IsLevelShown, Bucket)),
			SNew(STextBlock)
				.ColorAndOpacity(Level.Color)
				.Text_Lambda([this, Bucket, Label]()
				{
					return FText::Format(LOCTEXT("LevelEntry", "{0}  {1}"), Label, FText::AsNumber(BucketCounts[Bucket]));
				}),
			NAME_None,
			Level.ToolTip,
			EUserInterfaceActionType::ToggleButton);
	}
	Menu.EndSection();

	// Columns are shared by every tab.
	Menu.BeginSection(TEXT("Columns"), LOCTEXT("ColumnsSection", "Columns"));
	for (const FOptionalColumn& Column : GetOptionalColumns(bMerged))
	{
		Menu.AddMenuEntry(
			Column.Label,
			Column.ToolTip,
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &SEELogFileTab::ToggleColumn, Column.Id),
				FCanExecuteAction(),
				FIsActionChecked::CreateSP(this, &SEELogFileTab::IsColumnShown, Column.Id)),
			NAME_None,
			EUserInterfaceActionType::ToggleButton);
	}
	Menu.EndSection();

	return Menu.MakeWidget();
}

void SEELogFileTab::ToggleColumn(FName ColumnId)
{
	if (HeaderRow.IsValid())
	{
		// The header reports the change through OnHiddenColumnsListChanged, which saves and shares it.
		HeaderRow->SetShowGeneratedColumn(ColumnId, !HeaderRow->IsColumnVisible(ColumnId));
	}
}

bool SEELogFileTab::IsColumnShown(FName ColumnId) const
{
	return HeaderRow.IsValid() && HeaderRow->IsColumnVisible(ColumnId);
}

void SEELogFileTab::ToggleLevel(int32 Bucket)
{
	bShowBucket[Bucket] = !bShowBucket[Bucket];
	RebuildFilter();
}

bool SEELogFileTab::IsLevelShown(int32 Bucket) const
{
	return bShowBucket[Bucket];
}

void SEELogFileTab::ToggleSource(int32 SourceIndex)
{
	if (!Sources.IsValidIndex(SourceIndex))
	{
		return;
	}

	Sources[SourceIndex]->bShown = !Sources[SourceIndex]->bShown;
	SaveHiddenSources();
	RecountBuckets();
	RebuildFilter();
}

bool SEELogFileTab::IsSourceShownAt(int32 SourceIndex) const
{
	return Sources.IsValidIndex(SourceIndex) && Sources[SourceIndex]->bShown;
}

bool SEELogFileTab::IsRowSourceShown(const FEELogRow& Row) const
{
	if (!bMerged || Row.Kind == EEELogRowKind::Separator || Row.Kind == EEELogRowKind::Header)
	{
		return true;
	}
	return !Sources.IsValidIndex(Row.SourceIndex) || Sources[Row.SourceIndex]->bShown;
}

void SEELogFileTab::SaveHiddenSources() const
{
	UEFLogViewerSettings* Settings = GetMutableDefault<UEFLogViewerSettings>();
	for (const TUniquePtr<FSource>& Source : Sources)
	{
		if (Source->bShown)
		{
			Settings->AllTabHiddenFiles.Remove(Source->Key);
		}
		else
		{
			Settings->AllTabHiddenFiles.AddUnique(Source->Key);
		}
	}
	Settings->SaveConfig();
}

void SEELogFileTab::RecountBuckets()
{
	for (int32& Count : BucketCounts)
	{
		Count = 0;
	}

	for (const FRowPtr& Row : Rows)
	{
		if ((Row->Kind == EEELogRowKind::Line || Row->Kind == EEELogRowKind::Raw) && IsRowSourceShown(*Row))
		{
			++BucketCounts[GetBucket(Row->Verbosity)];
		}
	}
}

void SEELogFileTab::UseDefaults()
{
	using namespace EELogFileTabPrivate;

	for (bool& bShow : bShowBucket)
	{
		bShow = true;
	}

	if (bMerged)
	{
		for (const TUniquePtr<FSource>& Source : Sources)
		{
			Source->bShown = true;
		}
		SaveHiddenSources();
		RecountBuckets();
	}
	RebuildFilter();

	if (!HeaderRow.IsValid())
	{
		return;
	}

	// One save and one broadcast for the columns, not one per column.
	{
		TGuardValue<bool> ApplyingGuard(bApplyingColumnSetting, true);
		for (const FOptionalColumn& Column : GetOptionalColumns(bMerged))
		{
			HeaderRow->SetShowGeneratedColumn(Column.Id, true);
		}
	}
	HandleHiddenColumnsChanged();
}

void SEELogFileTab::HandleHiddenColumnsChanged()
{
	using namespace EELogFileTabPrivate;

	if (bApplyingColumnSetting || !HeaderRow.IsValid())
	{
		return;
	}

	UEFLogViewerSettings* Settings = GetMutableDefault<UEFLogViewerSettings>();

	// A column this tab does not have (Category, in a file tab) keeps whatever the merged tab chose.
	TArray<FName> HiddenColumns = HeaderRow->GetHiddenColumnIds();
	const TArray<FOptionalColumn>& Columns = GetOptionalColumns(bMerged);
	for (const FName& Hidden : Settings->HiddenColumns)
	{
		if (!Columns.ContainsByPredicate([&Hidden](const FOptionalColumn& Column) { return Column.Id == Hidden; }))
		{
			HiddenColumns.AddUnique(Hidden);
		}
	}

	Settings->HiddenColumns = MoveTemp(HiddenColumns);
	Settings->SaveConfig();

	// Every tab, this one included, lines its header up with the saved choice.
	UEFLogViewerSettings::OnHiddenColumnsChanged().Broadcast();
}

void SEELogFileTab::ApplyHiddenColumnsSetting()
{
	using namespace EELogFileTabPrivate;

	if (!HeaderRow.IsValid())
	{
		return;
	}

	const TArray<FName>& HiddenColumns = GetDefault<UEFLogViewerSettings>()->HiddenColumns;

	// Applying the shared choice changes this header too; that must not read as a new choice to share.
	TGuardValue<bool> ApplyingGuard(bApplyingColumnSetting, true);
	for (const FOptionalColumn& Column : GetOptionalColumns(bMerged))
	{
		HeaderRow->SetShowGeneratedColumn(Column.Id, !HiddenColumns.Contains(Column.Id));
	}
}

TSharedRef<ITableRow> SEELogFileTab::GenerateRow(FRowPtr Row, const TSharedRef<STableViewBase>& OwnerTable)
{
	using namespace EELogFileTabPrivate;

	if (Row->Kind == EEELogRowKind::Separator || Row->Kind == EEELogRowKind::Header)
	{
		// Structure rather than content: one line across the whole width, with the marker's time at
		// the right when the file recorded one.
		const bool bSeparator = Row->Kind == EEELogRowKind::Separator;
		return SNew(STableRow<FRowPtr>, OwnerTable)
			.Padding(FMargin(4.0f, 3.0f))
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("Brushes.Header"))
				.Padding(FMargin(6.0f, 2.0f))
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(STextBlock)
						.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
						.ColorAndOpacity(bSeparator ? FSlateColor(FLinearColor(0.35f, 0.7f, 1.0f)) : FSlateColor::UseSubduedForeground())
						.Text(FText::FromString(bSeparator ? FString::Printf(TEXT("==== %s ===="), *Row->Message) : Row->Message))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Font(GetRowFont())
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						.Text(FText::FromString(Row->Time))
					]
				]
			];
	}

	const FSource* Source = Sources.IsValidIndex(Row->SourceIndex) ? Sources[Row->SourceIndex].Get() : nullptr;
	return SNew(SEELogRowWidget, OwnerTable)
		.Row(Row)
		.Category(Source ? Source->Title : FString())
		.CategoryColor(Source ? Source->Color : FSlateColor::UseForeground());
}

TSharedPtr<SWidget> SEELogFileTab::BuildRowContextMenu()
{
	const TArray<FRowPtr> Selected = ListView->GetSelectedItems();
	if (Selected.Num() == 0)
	{
		return nullptr;
	}

	FMenuBuilder Menu(true, nullptr);

	Menu.AddMenuEntry(
		LOCTEXT("CopyRows", "Copy"),
		LOCTEXT("CopyRowsTooltip", "Copy the selected rows as they appear in the file (Ctrl+C)."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Copy"),
		FUIAction(FExecuteAction::CreateSP(this, &SEELogFileTab::CopySelection, false)));

	Menu.AddMenuEntry(
		LOCTEXT("CopyMessages", "Copy messages only"),
		LOCTEXT("CopyMessagesTooltip", "Copy only the message text of the selected rows."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &SEELogFileTab::CopySelection, true)));

	const FRowPtr First = Selected[0];
	if (Selected.Num() == 1 && First.IsValid() && !First->SourcePath.IsEmpty())
	{
		Menu.AddMenuEntry(
			LOCTEXT("OpenSource", "Open source"),
			FText::FromString(FString::Printf(TEXT("%s:%d"), *First->SourcePath, First->SourceLine)),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.OpenInExternalEditor"),
			FUIAction(FExecuteAction::CreateLambda([this, First]() { OpenSource(*First); })));
	}

	return Menu.MakeWidget();
}

void SEELogFileTab::SetOwnerTab(const TSharedRef<SDockTab>& InOwnerTab)
{
	OwnerTab = InOwnerTab;
}

void SEELogFileTab::Refresh()
{
	TArray<FRowPtr> NewRows;
	for (int32 SourceIndex = 0; SourceIndex < Sources.Num(); ++SourceIndex)
	{
		ReadSource(SourceIndex, NewRows);
	}
	AddRows(MoveTemp(NewRows));
}

void SEELogFileTab::AddSource(const FString& Key, const FString& Path, const FString& SourceTitle)
{
	using namespace EELogFileTabPrivate;

	if (!bMerged || Sources.ContainsByPredicate([&Key](const TUniquePtr<FSource>& Source) { return Source->Key == Key; }))
	{
		return;
	}

	const int32 SourceIndex = Sources.Num();
	TUniquePtr<FSource> Source = MakeUnique<FSource>();
	Source->Key = Key;
	Source->Path = Path;
	Source->Title = SourceTitle;
	Source->Color = MakeSourceColor(SourceIndex);
	Source->Tail = MakeUnique<FEELogFileTail>(Path, LoadChunkBytes);
	Source->bShown = !GetDefault<UEFLogViewerSettings>()->AllTabHiddenFiles.Contains(Key);
	Sources.Add(MoveTemp(Source));

	TArray<FRowPtr> NewRows;
	ReadSource(SourceIndex, NewRows);

	// A file too long to read whole starts partway through. Rows the other files hold from before
	// that point would sit in a view that silently lacks this file's rows, so the view starts there.
	if (Sources[SourceIndex]->Tail->HasEarlier())
	{
		const FRowPtr* FirstLine = NewRows.FindByPredicate([](const FRowPtr& Row) { return Row->Kind == EEELogRowKind::Line; });
		if (FirstLine && (*FirstLine)->SortTicks > MergedWindowStartTicks)
		{
			MergedWindowStartTicks = (*FirstLine)->SortTicks;
			PruneBeforeMergedWindow();
		}
	}

	AddRows(MoveTemp(NewRows));
}

void SEELogFileTab::RefreshSources(const TArray<FString>& Keys)
{
	TArray<FRowPtr> NewRows;
	for (int32 SourceIndex = 0; SourceIndex < Sources.Num(); ++SourceIndex)
	{
		if (Keys.Contains(Sources[SourceIndex]->Key))
		{
			ReadSource(SourceIndex, NewRows);
		}
	}
	AddRows(MoveTemp(NewRows));
}

void SEELogFileTab::ReadSource(int32 SourceIndex, TArray<FRowPtr>& OutRows)
{
	FSource& Source = *Sources[SourceIndex];

	TArray<FString> Lines;
	const FEELogFileTail::EReadResult Result = Source.Tail->Read(Lines);

	// One file of a merged view going away is not worth a banner: its rows stay, and the rest go on.
	if (Result == FEELogFileTail::EReadResult::Missing)
	{
		if (!bMerged)
		{
			MarkMissing();
		}
		return;
	}

	if (!bMerged)
	{
		bMissing = false;
	}

	if (Result == FEELogFileTail::EReadResult::Restarted)
	{
		Source.Parser.ResetContinuation();
	}

	if (Lines.Num() == 0)
	{
		return;
	}

	const int32 FirstNew = OutRows.Num();
	Source.Parser.Parse(Lines, OutRows);
	for (int32 Index = FirstNew; Index < OutRows.Num(); ++Index)
	{
		OutRows[Index]->SourceIndex = SourceIndex;
	}
}

void SEELogFileTab::PruneBeforeMergedWindow()
{
	const int32 Removed = Rows.RemoveAll([this](const FRowPtr& Row) { return Row->SortTicks < MergedWindowStartTicks; });
	if (Removed == 0)
	{
		return;
	}

	for (const TUniquePtr<FSource>& Source : Sources)
	{
		Source->RowCount = 0;
	}
	for (const FRowPtr& Row : Rows)
	{
		if (Row->Kind == EEELogRowKind::Line || Row->Kind == EEELogRowKind::Raw)
		{
			++Sources[Row->SourceIndex]->RowCount;
		}
	}
	RecountBuckets();
	RebuildFilter();
}

void SEELogFileTab::MarkMissing()
{
	bMissing = true;
}

void SEELogFileTab::ClearView()
{
	ClearedBeforeSequence = NextArrivalSequence;
	MarkViewed();
	RebuildFilter();
}

int32 SEELogFileTab::GetBucket(EEFLogVerbosity Verbosity)
{
	switch (Verbosity)
	{
	case EEFLogVerbosity::Error:   return 0;
	case EEFLogVerbosity::Warning: return 1;
	case EEFLogVerbosity::Display: return 2;
	case EEFLogVerbosity::Log:     return 3;
	default:                       return 4;
	}
}

void SEELogFileTab::AddRows(TArray<FRowPtr>&& NewRows)
{
	if (bMerged)
	{
		// Before the view's start, or a marker another file already showed.
		NewRows.RemoveAll([this](const FRowPtr& Row)
		{
			if (Row->SortTicks < MergedWindowStartTicks)
			{
				return true;
			}
			if (Row->Kind == EEELogRowKind::Separator || Row->Kind == EEELogRowKind::Header)
			{
				bool bAlreadyShown = false;
				SeenStructureKeys.Add(Row->StructureKey, &bAlreadyShown);
				return bAlreadyShown;
			}
			return false;
		});
	}

	if (NewRows.Num() == 0)
	{
		return;
	}

	const bool bViewed = IsViewed();
	const int32 PreviousRunIndex = LatestRunIndex;
	const int64 PreviousRunStartTicks = CurrentRunStartTicks;

	for (const FRowPtr& Row : NewRows)
	{
		Row->Sequence = NextArrivalSequence++;

		if (Row->bRunStart)
		{
			CurrentRunStartTicks = FMath::Max(CurrentRunStartTicks, Row->SortTicks);
		}

		if (Row->Kind == EEELogRowKind::Line || Row->Kind == EEELogRowKind::Raw)
		{
			++Sources[Row->SourceIndex]->RowCount;

			// A file unticked in a merged view is out of its counts, as it is out of its list.
			if (IsRowSourceShown(*Row))
			{
				++BucketCounts[GetBucket(Row->Verbosity)];

				if (!bViewed)
				{
					++UnreadCount;
					UnreadErrors += Row->Verbosity == EEFLogVerbosity::Error ? 1 : 0;
					UnreadWarnings += Row->Verbosity == EEFLogVerbosity::Warning ? 1 : 0;
				}
			}
		}

		AddInstanceOption(Row->Instance);
	}

	// Either way the new rows end up after FirstNew, unless a merge had to place some earlier.
	const int32 FirstNew = Rows.Num();
	bool bPlacedEarlier = false;
	if (bMerged)
	{
		bPlacedEarlier = EELogRows::MergeByTime(Rows, MoveTemp(NewRows));
	}
	else
	{
		Rows.Append(MoveTemp(NewRows));
		LatestRunIndex = Sources[0]->Parser.GetRunIndex();
	}

	const bool bRunChanged = bMerged ? CurrentRunStartTicks != PreviousRunStartTicks : LatestRunIndex != PreviousRunIndex;

	// A trim can remove rows from this very batch (a first read of a long file can hold more rows
	// than the cap), so the filtered list is rebuilt from what survived rather than extended.
	const bool bTrimmedNow = TrimToCap();

	if (bTrimmedNow || bPlacedEarlier || (bCurrentRunOnly && bRunChanged))
	{
		// Rows went, rows landed among those already shown, or a new run began and "current" now
		// means something else for every row.
		RebuildFilter();
	}
	else
	{
		for (int32 Index = FirstNew; Index < Rows.Num(); ++Index)
		{
			if (PassesFilter(*Rows[Index]))
			{
				FilteredRows.Add(Rows[Index]);
			}
		}
		ListView->RequestListRefresh();
	}

	ScrollToEndIfFollowing();
}

bool SEELogFileTab::TrimToCap()
{
	const int32 Cap = MaxRows + LoadedEarlierRows;
	const int32 Excess = Rows.Num() - Cap;
	if (Excess <= 0)
	{
		return false;
	}

	// A merged tab's rows are in time order, so the oldest go here too, whichever file they came from.
	int32 VisibleRowsRemoved = 0;
	for (int32 Index = 0; Index < Excess; ++Index)
	{
		const FEELogRow& Row = *Rows[Index];
		if (Row.Kind == EEELogRowKind::Line || Row.Kind == EEELogRowKind::Raw)
		{
			--Sources[Row.SourceIndex]->RowCount;
			if (IsRowSourceShown(Row))
			{
				--BucketCounts[GetBucket(Row.Verbosity)];
			}
		}
		if (PassesFilter(Row))
		{
			++VisibleRowsRemoved;
		}
	}

	Rows.RemoveAt(0, Excess, EAllowShrinking::No);

	// Removing rows above the view moves the scroll offset up by as many; that is not the user scrolling.
	PendingTrimCompensation += VisibleRowsRemoved;
	bTrimmed = true;
	return true;
}

void SEELogFileTab::RebuildFilter()
{
	FilteredRows.Reset();
	for (const FRowPtr& Row : Rows)
	{
		if (PassesFilter(*Row))
		{
			FilteredRows.Add(Row);
		}
	}

	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
	ScrollToEndIfFollowing();
}

bool SEELogFileTab::PassesFilter(const FEELogRow& Row) const
{
	if (Row.Sequence < ClearedBeforeSequence)
	{
		return false;
	}

	if (bCurrentRunOnly)
	{
		// A merged tab's files count their runs from different starting points; time is shared.
		const bool bOutsideRun = bMerged
			? CurrentRunStartTicks > 0 && Row.SortTicks < CurrentRunStartTicks
			: LatestRunIndex > 0 && Row.RunIndex != LatestRunIndex;
		if (bOutsideRun)
		{
			return false;
		}
	}

	// Separators and session headers are structure: shown unless a search is narrowing the view.
	if (Row.Kind == EEELogRowKind::Separator || Row.Kind == EEELogRowKind::Header)
	{
		return FilterText.IsEmpty();
	}

	if (!IsRowSourceShown(Row))
	{
		return false;
	}

	if (!bShowBucket[GetBucket(Row.Verbosity)])
	{
		return false;
	}

	if (!SelectedInstance.IsEmpty() && Row.Instance != SelectedInstance)
	{
		return false;
	}

	if (FilterText.IsEmpty() || Row.Message.Contains(FilterText) || Row.SourcePath.Contains(FilterText))
	{
		return true;
	}
	return bMerged && Sources.IsValidIndex(Row.SourceIndex) && Sources[Row.SourceIndex]->Title.Contains(FilterText);
}

void SEELogFileTab::AddInstanceOption(const FString& Instance)
{
	if (Instance.IsEmpty() || KnownInstances.Contains(Instance))
	{
		return;
	}

	KnownInstances.Add(Instance);
	InstanceOptions.Add(MakeShared<FString>(Instance));
	if (InstanceCombo.IsValid())
	{
		InstanceCombo->RefreshOptions();
	}
}

void SEELogFileTab::ScrollToEndIfFollowing()
{
	if (bFollowTail && ListView.IsValid() && FilteredRows.Num() > 0)
	{
		ListView->ScrollToBottom();
	}
}

void SEELogFileTab::HandleListScrolled(double Offset)
{
	// Back at the end resumes following; moving up (other than by trimmed rows) pauses it.
	if (ListView->GetScrollDistanceRemaining().Y <= KINDA_SMALL_NUMBER)
	{
		bFollowTail = true;
	}
	else if (Offset + PendingTrimCompensation < LastScrollOffset - 0.01)
	{
		bFollowTail = false;
	}

	LastScrollOffset = Offset;
	PendingTrimCompensation = 0.0;
}

void SEELogFileTab::LoadEarlier()
{
	TArray<FString> Lines;
	if (bMerged || !Sources[0]->Tail->ReadEarlier(LoadChunkBytes, Lines) || Lines.Num() == 0)
	{
		return;
	}

	// A separate parser: continuation lines inside the earlier chunk belong to it alone.
	FEELogLineParser EarlierParser;
	TArray<FRowPtr> Earlier;
	EarlierParser.Parse(Lines, Earlier);

	for (const FRowPtr& Row : Earlier)
	{
		// Older than anything shown: hidden by an earlier "Clear view", never part of the current run.
		Row->Sequence = 0;
		Row->RunIndex = -1;

		if (Row->Kind == EEELogRowKind::Line || Row->Kind == EEELogRowKind::Raw)
		{
			++BucketCounts[GetBucket(Row->Verbosity)];
			++Sources[0]->RowCount;
		}
		AddInstanceOption(Row->Instance);
	}

	Rows.Insert(Earlier, 0);
	LoadedEarlierRows += Earlier.Num();

	// The user asked for older rows, so stay where they are instead of jumping back to the end.
	bFollowTail = false;
	RebuildFilter();
}

bool SEELogFileTab::IsViewed() const
{
	const TSharedPtr<SDockTab> Tab = OwnerTab.Pin();
	return Tab.IsValid() && Tab->IsForeground();
}

void SEELogFileTab::MarkViewed()
{
	UnreadCount = 0;
	UnreadWarnings = 0;
	UnreadErrors = 0;
}

void SEELogFileTab::HandleTabActivated(TSharedRef<SDockTab> Tab, ETabActivationCause Cause)
{
	MarkViewed();
}

FText SEELogFileTab::GetTabLabel() const
{
	FString Label = Title;
	if (UnreadCount > 0)
	{
		Label += FString::Printf(TEXT(" (%d)"), UnreadCount);
	}
	if (bMissing)
	{
		Label += TEXT(" (gone)");
	}
	return FText::FromString(Label);
}

const FSlateBrush* SEELogFileTab::GetTabIcon() const
{
	if (UnreadErrors > 0)
	{
		return FAppStyle::GetBrush("Icons.ErrorWithColor");
	}
	if (UnreadWarnings > 0)
	{
		return FAppStyle::GetBrush("Icons.WarningWithColor");
	}
	return FAppStyle::GetBrush("Icons.Documentation");
}

FReply SEELogFileTab::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.IsControlDown() && InKeyEvent.GetKey() == EKeys::C)
	{
		CopySelection(false);
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SEELogFileTab::OpenSource(const FEELogRow& Row) const
{
	if (Row.SourcePath.IsEmpty() || Row.SourcePath == TEXT("EFLog") || Row.SourcePath == TEXT("Blueprint"))
	{
		return;
	}

	// The Blueprint node writes its caller's class path ("/Game/.../BP_Door.BP_Door_C"): open that
	// Blueprint. A native caller ("/Script/Module.Class") has no asset to open.
	if (Row.SourcePath.StartsWith(TEXT("/")) && !FPaths::FileExists(Row.SourcePath))
	{
		const UClass* Class = Cast<UClass>(FSoftObjectPath(Row.SourcePath).TryLoad());
		UBlueprint* Blueprint = Class ? UBlueprint::GetBlueprintFromClass(Class) : nullptr;
		if (Blueprint && GEditor)
		{
			GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(Blueprint);
		}
		return;
	}

	// C++ lines are written relative to the project folder.
	FString Path = Row.SourcePath;
	if (FPaths::IsRelative(Path))
	{
		Path = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / Path);
	}

	if (FPaths::FileExists(Path))
	{
		FSourceCodeNavigation::OpenSourceFile(Path, Row.SourceLine);
	}
}

void SEELogFileTab::CopySelection(bool bMessagesOnly) const
{
	const TArray<FRowPtr> Selected = ListView->GetSelectedItems();
	if (Selected.Num() == 0)
	{
		return;
	}

	// Selection order is click order; copy in file order, in one pass over the rows.
	TSet<const FEELogRow*> SelectedRows;
	for (const FRowPtr& Row : Selected)
	{
		SelectedRows.Add(Row.Get());
	}

	FString Text;
	for (const FRowPtr& Row : Rows)
	{
		if (SelectedRows.Contains(Row.Get()))
		{
			// Rows merged from several files say which one they came from.
			if (bMerged && !bMessagesOnly && (Row->Kind == EEELogRowKind::Line || Row->Kind == EEELogRowKind::Raw) && Sources.IsValidIndex(Row->SourceIndex))
			{
				Text += FString::Printf(TEXT("[%s]"), *Sources[Row->SourceIndex]->Title);
			}
			Text += bMessagesOnly ? Row->Message : Row->RawText;
			Text += LINE_TERMINATOR;
		}
	}

	FPlatformApplicationMisc::ClipboardCopy(*Text);
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
