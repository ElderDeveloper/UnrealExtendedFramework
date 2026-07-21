// Copyright Moon Punch Games. All Rights Reserved.

#include "SEELocalizationWorkbenchPanel.h"

#if WITH_EDITOR

#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Localization/Context/EELocalizationContextProvider.h"
#include "Localization/Pipeline/EELocPipeline.h"
#include "Localization/Providers/EELocTranslationProvider.h"
#include "Misc/ConfigCacheIni.h"
#include "UILab/EEUILabFeature.h"
#include "UObject/SoftObjectPath.h"
#include "WidgetBlueprint.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SEELocalizationWorkbenchPanel"

namespace
{
	const FName ColumnId(TEXT("Id"));
	const FName ColumnSource(TEXT("Source"));
	const FName ColumnTranslation(TEXT("Translation"));
	const FName ColumnCompare(TEXT("Compare"));
	const FName ColumnState(TEXT("State"));
	const FName ColumnSourceType(TEXT("SourceType"));
	const FName ColumnRatio(TEXT("Ratio"));
	const FName ColumnReview(TEXT("Review"));

	FText StateToText(const EEELocEntryState State)
	{
		switch (State)
		{
		case EEELocEntryState::Missing: return LOCTEXT("StateMissing", "Missing");
		case EEELocEntryState::Stale: return LOCTEXT("StateStale", "Stale");
		case EEELocEntryState::Identical: return LOCTEXT("StateIdentical", "Identical");
		default: return LOCTEXT("StateTranslated", "Translated");
		}
	}

	FLinearColor StateToColor(const EEELocEntryState State)
	{
		switch (State)
		{
		case EEELocEntryState::Missing: return FLinearColor(1.0f, 0.35f, 0.35f);
		case EEELocEntryState::Stale: return FLinearColor(1.0f, 0.65f, 0.25f);
		case EEELocEntryState::Identical: return FLinearColor(1.0f, 0.9f, 0.4f);
		default: return FLinearColor(0.4f, 1.0f, 0.4f);
		}
	}

	/** Multi-column grid row: id, source, editable translation, comparison culture, state. */
	class SEELocGridRow : public SMultiColumnTableRow<TSharedPtr<FEELocEntry>>
	{
	public:
		SLATE_BEGIN_ARGS(SEELocGridRow) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable,
			const TSharedPtr<FEELocEntry>& InEntry, SEELocalizationWorkbenchPanel* InPanel)
		{
			Entry = InEntry;
			Panel = InPanel;
			SMultiColumnTableRow<TSharedPtr<FEELocEntry>>::Construct(FSuperRowType::FArguments(), OwnerTable);
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override
		{
			const FEELocTranslation* Translation = Entry->Translations.Find(Panel->GetSelectedCulture());

			if (InColumnName == ColumnId)
			{
				return SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(TEXT("%s , %s"), *Entry->Namespace, *Entry->Key)))
					.ToolTipText(FText::FromString(Entry->SourceLocation));
			}
			if (InColumnName == ColumnSource)
			{
				return SNew(STextBlock)
					.Text(FText::FromString(Entry->SourceText))
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					.ToolTipText(FText::FromString(Entry->SourceText));
			}
			if (InColumnName == ColumnTranslation)
			{
				return SNew(SEditableTextBox)
					.Text(FText::FromString(Translation ? Translation->Text : FString()))
					.ToolTipText(LOCTEXT("EditTooltip", "Edit and press Enter to stage the translation."))
					.OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type CommitType)
					{
						if (CommitType == ETextCommit::OnEnter)
						{
							Panel->CommitTranslation(Entry, NewText.ToString());
						}
					});
			}
			if (InColumnName == ColumnCompare)
			{
				// Pseudo preview layer takes over the compare column when active.
				if (Panel->GetPseudoType() != EEELocPseudoType::None)
				{
					return SNew(STextBlock)
						.Text(FText::FromString(EELocPseudo::Transform(Entry->SourceText, Panel->GetPseudoType())))
						.ColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.9f, 1.0f)))
						.OverflowPolicy(ETextOverflowPolicy::Ellipsis);
				}
				const FEELocTranslation* Compare = Entry->Translations.Find(Panel->GetCompareCulture());
				return SNew(STextBlock)
					.Text(FText::FromString(Compare ? Compare->Text : FString()))
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis);
			}
			if (InColumnName == ColumnState)
			{
				const EEELocEntryState State = Translation ? Translation->State : EEELocEntryState::Missing;
				return SNew(STextBlock)
					.Text(StateToText(State))
					.ColorAndOpacity(FSlateColor(StateToColor(State)));
			}
			if (InColumnName == ColumnSourceType)
			{
				return SNew(STextBlock).Text(FText::FromString(Entry->SourceType));
			}
			if (InColumnName == ColumnRatio)
			{
				const float Ratio = Translation && !Translation->Text.IsEmpty() && Entry->SourceText.Len() > 0
					? static_cast<float>(Translation->Text.Len()) / Entry->SourceText.Len()
					: 0.0f;
				return SNew(STextBlock)
					.Text(Ratio > 0.0f ? FText::FromString(FString::Printf(TEXT("%.2f"), Ratio)) : FText::GetEmpty())
					.ColorAndOpacity(FSlateColor(Ratio > 1.6f ? FLinearColor(1.0f, 0.4f, 0.4f) : FLinearColor::White));
			}
			if (InColumnName == ColumnReview)
			{
				const FEELocReviewRecord Record = Panel->GetReviewStore().GetRecord(Entry->Namespace, Entry->Key, Panel->GetSelectedCulture());
				return SNew(STextBlock)
					.Text(Record.State == EEELocReviewState::None
						? FText::GetEmpty()
						: FText::FromString(FEELocReviewStore::ReviewStateToString(Record.State)));
			}

			return SNullWidget::NullWidget;
		}

	private:
		TSharedPtr<FEELocEntry> Entry;
		SEELocalizationWorkbenchPanel* Panel = nullptr;
	};
}

void SEELocalizationWorkbenchPanel::Construct(const FArguments& InArgs)
{
	Session = MakeShared<FEELocalizationSession>();
	ReviewStore = MakeShared<FEELocReviewStore>();

	for (const TCHAR* State : { TEXT("None"), TEXT("Draft"), TEXT("MachineDraft"), TEXT("HumanReviewed"),
		TEXT("Locked"), TEXT("NeedsContext"), TEXT("NeedsNativeRewrite") })
	{
		ReviewStateOptions.Add(MakeShared<FString>(State));
	}

	TArray<FString> TargetNames;
	FEELocalizationSession::GetAvailableTargets(TargetNames);
	for (const FString& Name : TargetNames)
	{
		TargetOptions.Add(MakeShared<FString>(Name));
	}

	for (const TCHAR* State : { TEXT("All"), TEXT("Missing"), TEXT("Stale"), TEXT("Identical"), TEXT("Translated") })
	{
		StateFilterOptions.Add(MakeShared<FString>(State));
	}

	for (const TCHAR* SourceType : { TEXT("All"), TEXT("C++"), TEXT("Asset"), TEXT("Other") })
	{
		SourceTypeOptions.Add(MakeShared<FString>(SourceType));
	}

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(4.0f)
		[
			MakeToolbar()
		]

		// B-2 overview: per-culture progress, clickable to focus that culture's problems.
		+ SVerticalBox::Slot().AutoHeight().Padding(4.0f, 0.0f, 4.0f, 4.0f)
		[
			SAssignNew(OverviewBox, SHorizontalBox)
			.Visibility_Lambda([this]() { return SetupState == EELocSetup::EEESetupState::Ready && Session->IsOpen() ? EVisibility::Visible : EVisibility::Collapsed; })
		]

		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(4.0f)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return SetupState != EELocSetup::EEESetupState::Ready ? EVisibility::Visible : EVisibility::Collapsed; })
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				MakeSetupPanel()
			]
		]

		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(4.0f)
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([this]() { return SetupState == EELocSetup::EEESetupState::Ready ? EVisibility::Visible : EVisibility::Collapsed; })

			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SAssignNew(EntryList, SListView<TSharedPtr<FEELocEntry>>)
				.ListItemsSource(&FilteredEntries)
				.OnGenerateRow(this, &SEELocalizationWorkbenchPanel::MakeEntryRow)
				.SelectionMode(ESelectionMode::Multi)
				.OnSelectionChanged(this, &SEELocalizationWorkbenchPanel::HandleSelectionChanged)
				.OnContextMenuOpening_Lambda([this]() { return MakeGridContextMenu(); })
				.HeaderRow(
					SAssignNew(HeaderRow, SHeaderRow)
					.CanSelectGeneratedColumn(true)
					+ SHeaderRow::Column(ColumnId).DefaultLabel(LOCTEXT("ColId", "Namespace , Key")).FillWidth(0.20f)
					+ SHeaderRow::Column(ColumnSource).DefaultLabel(LOCTEXT("ColSource", "Native Source")).FillWidth(0.26f)
					+ SHeaderRow::Column(ColumnTranslation).DefaultLabel(LOCTEXT("ColTranslation", "Translation")).FillWidth(0.26f)
					+ SHeaderRow::Column(ColumnCompare).DefaultLabel(LOCTEXT("ColCompare", "Compare")).FillWidth(0.12f)
					+ SHeaderRow::Column(ColumnState).DefaultLabel(LOCTEXT("ColState", "State")).FillWidth(0.06f)
					+ SHeaderRow::Column(ColumnSourceType).DefaultLabel(LOCTEXT("ColSourceType", "Origin")).FillWidth(0.05f)
					+ SHeaderRow::Column(ColumnRatio).DefaultLabel(LOCTEXT("ColRatio", "Ratio")).FillWidth(0.04f)
					+ SHeaderRow::Column(ColumnReview).DefaultLabel(LOCTEXT("ColReview", "Review")).FillWidth(0.07f)
				)
			]

			+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				MakeContextInspector()
			]
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(4.0f, 4.0f, 4.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([this]() { return bShowIssues ? EVisibility::Visible : EVisibility::Collapsed; })

			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return IssuesHeading; })
				.Font(FAppStyle::GetFontStyle("BoldFont"))
			]

			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("FixAll", "Fix All"))
				.ToolTipText(LOCTEXT("FixAllTooltip", "Apply every safe automatic fix in the list: Keep for stale, Mark Reviewed for identical, Normalize for whitespace. Locked entries are never touched."))
				.OnClicked_Lambda([this]() { FixAllShownIssues(); return FReply::Handled(); })
			]
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(4.0f).MaxHeight(180.0f)
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return bShowIssues ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SAssignNew(IssueList, SListView<TSharedPtr<FEELocIssue>>)
				.ListItemsSource(&Issues)
				.OnGenerateRow(this, &SEELocalizationWorkbenchPanel::MakeIssueRow)
				.OnSelectionChanged_Lambda([this](TSharedPtr<FEELocIssue> Issue, ESelectInfo::Type)
				{
					// Jump-to-entry: select and scroll the grid to the issue's entry.
					if (Issue.IsValid() && Issue->Entry.IsValid() && EntryList.IsValid())
					{
						EntryList->SetSelection(Issue->Entry);
						EntryList->RequestScrollIntoView(Issue->Entry);
					}
				})
			]
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(4.0f)
		[
			SNew(STextBlock)
			.Text(this, &SEELocalizationWorkbenchPanel::GetStatusText)
			.AutoWrapText(true)
		]
	];

	// B-4: extra columns start hidden; the header's right-click menu toggles them.
	if (HeaderRow.IsValid())
	{
		HeaderRow->SetShowGeneratedColumn(ColumnSourceType, false);
		HeaderRow->SetShowGeneratedColumn(ColumnRatio, false);
		HeaderRow->SetShowGeneratedColumn(ColumnReview, false);
	}
	UpdateCompareColumnVisibility();

	// B-2: restore the last target for this project, else auto-open a single target;
	// zero targets routes to the setup wizard.
	FString LastTarget;
	GConfig->GetString(TEXT("ExtendedLocalizationWorkbench"), TEXT("LastTarget"), LastTarget, GEditorPerProjectIni);

	if (!LastTarget.IsEmpty() && TargetOptions.ContainsByPredicate([&LastTarget](const TSharedPtr<FString>& Option) { return *Option == LastTarget; }))
	{
		OpenTarget(LastTarget);
	}
	else if (TargetOptions.Num() == 1)
	{
		OpenTarget(*TargetOptions[0]);
	}
	else if (TargetOptions.Num() == 0)
	{
		SetupState = EELocSetup::EEESetupState::NoTargets;
	}
}

void SEELocalizationWorkbenchPanel::SaveUIState() const
{
	GConfig->SetString(TEXT("ExtendedLocalizationWorkbench"), TEXT("LastTarget"), *SelectedTarget, GEditorPerProjectIni);
	GConfig->SetString(TEXT("ExtendedLocalizationWorkbench"), TEXT("LastCulture"), *SelectedCulture, GEditorPerProjectIni);
}

void SEELocalizationWorkbenchPanel::RebuildCultureStats()
{
	CultureStats.Reset();

	if (!Session->IsOpen())
	{
		return;
	}

	for (const FString& Culture : Session->GetForeignCultures())
	{
		CultureStats.Add(Culture);
	}

	for (const TSharedPtr<FEELocEntry>& Entry : Session->GetEntries())
	{
		for (const TPair<FString, FEELocTranslation>& Pair : Entry->Translations)
		{
			FEECultureStats* Stats = CultureStats.Find(Pair.Key);
			if (!Stats)
			{
				continue;
			}
			switch (Pair.Value.State)
			{
			case EEELocEntryState::Missing: ++Stats->Missing; break;
			case EEELocEntryState::Stale: ++Stats->Stale; break;
			case EEELocEntryState::Identical: ++Stats->Identical; break;
			default: ++Stats->Translated; break;
			}
		}
	}
}

void SEELocalizationWorkbenchPanel::RebuildOverview()
{
	if (!OverviewBox.IsValid())
	{
		return;
	}
	OverviewBox->ClearChildren();

	if (!Session->IsOpen())
	{
		return;
	}

	// Entries total.
	OverviewBox->AddSlot()
	.AutoWidth()
	.VAlign(VAlign_Center)
	.Padding(2.0f, 0.0f, 10.0f, 0.0f)
	[
		SNew(STextBlock)
		.Text_Lambda([this]()
		{
			return FText::Format(LOCTEXT("OverviewEntries", "{0} entries"), FText::AsNumber(Session->GetEntries().Num()));
		})
		.Font(FAppStyle::GetFontStyle("BoldFont"))
	];

	for (const FString& Culture : Session->GetForeignCultures())
	{
		OverviewBox->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("OverviewCultureTooltip", "Select this culture and filter to what needs work (Missing)."))
			.OnClicked_Lambda([this, Culture]()
			{
				SelectedCulture = Culture;
				StateFilter = TEXT("Missing");
				SaveUIState();
				RefreshFilteredEntries();
				return FReply::Handled();
			})
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text_Lambda([this, Culture]()
					{
						const FEECultureStats* Stats = CultureStats.Find(Culture);
						const int32 Total = Stats ? Stats->Total() : 0;
						const float Percent = Total > 0 ? 100.0f * Stats->Translated / Total : 0.0f;
						return FText::FromString(FString::Printf(TEXT("%s  %.0f%%"), *Culture, Percent));
					})
					.Font(FAppStyle::GetFontStyle("BoldFont"))
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
				[
					SNew(SBox).WidthOverride(140.0f).HeightOverride(6.0f)
					[
						SNew(SProgressBar)
						.Percent_Lambda([this, Culture]() -> TOptional<float>
						{
							const FEECultureStats* Stats = CultureStats.Find(Culture);
							const int32 Total = Stats ? Stats->Total() : 0;
							return Total > 0 ? static_cast<float>(Stats->Translated) / Total : 0.0f;
						})
					]
				]

				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text_Lambda([this, Culture]()
					{
						const FEECultureStats* Stats = CultureStats.Find(Culture);
						return Stats
							? FText::FromString(FString::Printf(TEXT("%d missing  %d stale"), Stats->Missing, Stats->Stale + Stats->Identical))
							: FText::GetEmpty();
					})
					.Font(FAppStyle::GetFontStyle("SmallFont"))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.75f, 0.75f)))
				]
			]
		];
	}
}

void SEELocalizationWorkbenchPanel::UpdateCompareColumnVisibility()
{
	if (HeaderRow.IsValid())
	{
		const bool bShowCompare = !CompareCulture.IsEmpty() || PseudoType != EEELocPseudoType::None;
		HeaderRow->SetShowGeneratedColumn(ColumnCompare, bShowCompare);
	}
}

TSharedRef<ITableRow> SEELocalizationWorkbenchPanel::MakeIssueRow(TSharedPtr<FEELocIssue> Issue, const TSharedRef<STableViewBase>& OwnerTable)
{
	const FLinearColor Color = Issue->Severity == EEELocIssueSeverity::Error
		? FLinearColor(1.0f, 0.35f, 0.35f)
		: FLinearColor(1.0f, 0.75f, 0.3f);

	const FString Text = FString::Printf(TEXT("[%s] %s%s%s , %s — %s"),
		Issue->Severity == EEELocIssueSeverity::Error ? TEXT("Error") : TEXT("Warning"),
		Issue->Culture.IsEmpty() ? TEXT("") : *Issue->Culture,
		Issue->Culture.IsEmpty() ? TEXT("") : TEXT("  "),
		Issue->Entry.IsValid() ? *Issue->Entry->Namespace : TEXT("?"),
		Issue->Entry.IsValid() ? *Issue->Entry->Key : TEXT("?"),
		*Issue->Message);

	// B-3: rule-specific one-click fixes.
	const TSharedRef<SHorizontalBox> FixButtons = SNew(SHorizontalBox);
	EEELocEntryState EntryState = EEELocEntryState::Translated;
	FString StaleTooltip;
	if (Issue->Entry.IsValid() && !Issue->Culture.IsEmpty())
	{
		if (const FEELocTranslation* Translation = Issue->Entry->Translations.Find(Issue->Culture))
		{
			EntryState = Translation->State;
			if (EntryState == EEELocEntryState::Stale)
			{
				StaleTooltip = FString::Printf(TEXT("Translated against:\n\"%s\"\n\nCurrent source:\n\"%s\""),
					*Translation->StaleSourceText, *Issue->Entry->SourceText);
			}
		}
	}

	auto AddFixButton = [this, &FixButtons, Issue](const FText& Label, const FText& Tooltip, const bool bAlternate)
	{
		FixButtons->AddSlot().AutoWidth().Padding(2.0f, 0.0f)
		[
			SNew(SButton)
			.Text(Label)
			.ToolTipText(Tooltip)
			.ContentPadding(FMargin(6.0f, 0.0f))
			.OnClicked_Lambda([this, Issue, bAlternate]()
			{
				if (ApplyIssueFix(*Issue, bAlternate))
				{
					RefreshFilteredEntries();
					RunValidation();
				}
				return FReply::Handled();
			})
		];
	};

	if (Issue->RuleId == TEXT("Whitespace"))
	{
		AddFixButton(LOCTEXT("FixNormalize", "Normalize"),
			LOCTEXT("FixNormalizeTooltip", "Apply the source's leading/trailing whitespace to the translation."), false);
	}
	else if (Issue->RuleId == TEXT("TranslationState") && EntryState == EEELocEntryState::Stale)
	{
		AddFixButton(LOCTEXT("FixKeep", "Keep"),
			FText::FromString(TEXT("Confirm the current translation against the new source (un-stales).\n\n") + StaleTooltip), false);
		AddFixButton(LOCTEXT("FixClear", "Clear"),
			LOCTEXT("FixClearTooltip", "Clear the outdated translation back to Missing so it gets retranslated."), true);
	}
	else if (Issue->RuleId == TEXT("TranslationState") && EntryState == EEELocEntryState::Identical)
	{
		AddFixButton(LOCTEXT("FixMarkReviewed", "Mark Reviewed"),
			LOCTEXT("FixMarkReviewedTooltip", "The text is intentionally the same as the source; mark Human Reviewed so it stops being flagged."), false);
	}

	return SNew(STableRow<TSharedPtr<FEELocIssue>>, OwnerTable)
	[
		SNew(SHorizontalBox)

		+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Text))
			.ToolTipText(FText::FromString(StaleTooltip))
			.ColorAndOpacity(FSlateColor(Color))
			.AutoWrapText(true)
		]

		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			FixButtons
		]
	];
}

bool SEELocalizationWorkbenchPanel::ApplyIssueFix(const FEELocIssue& Issue, const bool bAlternate)
{
	if (!Issue.Entry.IsValid() || Issue.Culture.IsEmpty())
	{
		return false;
	}

	const FEELocTranslation* Translation = Issue.Entry->Translations.Find(Issue.Culture);
	if (!Translation)
	{
		return false;
	}

	// Locked entries stay untouchable, exactly like manual edits.
	if (ReviewStore->IsLocked(Issue.Entry->Namespace, Issue.Entry->Key, Issue.Culture))
	{
		return false;
	}

	FString Error;

	if (Issue.RuleId == TEXT("Whitespace"))
	{
		const FString& Source = Issue.Entry->SourceText;
		FString Normalized = Translation->Text.TrimStartAndEnd();

		int32 LeadingCount = 0;
		while (LeadingCount < Source.Len() && FChar::IsWhitespace(Source[LeadingCount])) { ++LeadingCount; }
		int32 TrailingCount = 0;
		while (TrailingCount < Source.Len() - LeadingCount && FChar::IsWhitespace(Source[Source.Len() - 1 - TrailingCount])) { ++TrailingCount; }

		Normalized = Source.Left(LeadingCount) + Normalized + Source.Right(TrailingCount);
		return Session->SetTranslation(Issue.Entry, Issue.Culture, Normalized, Error);
	}

	if (Issue.RuleId == TEXT("TranslationState"))
	{
		switch (Translation->State)
		{
		case EEELocEntryState::Stale:
			// Keep: rewriting the same text re-stamps the recorded source. Clear: back to Missing.
			return Session->SetTranslation(Issue.Entry, Issue.Culture, bAlternate ? FString() : Translation->Text, Error);

		case EEELocEntryState::Identical:
			ReviewStore->SetState(Issue.Entry->Namespace, Issue.Entry->Key, Issue.Culture,
				EEELocReviewState::HumanReviewed, FPlatformProcess::UserName(), Issue.Entry->SourceText);
			return true;

		default:
			return false;
		}
	}

	return false;
}

void SEELocalizationWorkbenchPanel::FixAllShownIssues()
{
	int32 FixedCount = 0;
	for (const TSharedPtr<FEELocIssue>& Issue : Issues)
	{
		if (Issue.IsValid() && ApplyIssueFix(*Issue, /*bAlternate*/ false))
		{
			++FixedCount;
		}
	}

	StatusNote = FString::Printf(TEXT("Fix All applied %d automatic fixes (Keep for stale, Mark Reviewed for identical, Normalize for whitespace)."), FixedCount);
	RefreshFilteredEntries();
	RunValidation();
}

void SEELocalizationWorkbenchPanel::RunValidation()
{
	Issues.Reset();

	if (Session->IsOpen())
	{
		TArray<FEELocIssue> RawIssues;
		FEELocValidator::Get().ValidateAll(*Session, RawIssues);

		// Errors first.
		RawIssues.Sort([](const FEELocIssue& A, const FEELocIssue& B)
		{
			return A.Severity == EEELocIssueSeverity::Error && B.Severity != EEELocIssueSeverity::Error;
		});

		for (FEELocIssue& Issue : RawIssues)
		{
			// B-3: intentionally-identical entries that a human reviewed stop being flagged.
			if (Issue.RuleId == TEXT("TranslationState") && Issue.Entry.IsValid() && !Issue.Culture.IsEmpty())
			{
				const FEELocTranslation* Translation = Issue.Entry->Translations.Find(Issue.Culture);
				if (Translation && Translation->State == EEELocEntryState::Identical)
				{
					const EEELocReviewState ReviewState = ReviewStore->GetRecord(Issue.Entry->Namespace, Issue.Entry->Key, Issue.Culture).State;
					if (ReviewState == EEELocReviewState::HumanReviewed || ReviewState == EEELocReviewState::Locked)
					{
						continue;
					}
				}
			}

			Issues.Add(MakeShared<FEELocIssue>(MoveTemp(Issue)));
		}
	}

	IssuesHeading = LOCTEXT("ValidationHeading", "Validation results");
	bShowIssues = Issues.Num() > 0;
	if (IssueList.IsValid())
	{
		IssueList->RequestListRefresh();
	}
}

TSharedRef<SWidget> SEELocalizationWorkbenchPanel::MakeToolbar()
{
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(LOCTEXT("TargetLabel", "Target"))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f).VAlign(VAlign_Center)
		[
			MakeTargetCombo()
		]

		+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f, 2.0f, 0.0f).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(LOCTEXT("CultureLabel", "Culture"))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f).VAlign(VAlign_Center)
		[
			MakeCultureCombo(false)
		]

		+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f, 2.0f, 0.0f).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(LOCTEXT("CompareLabel", "Compare"))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f).VAlign(VAlign_Center)
		[
			MakeCultureCombo(true)
		]

		+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f, 2.0f, 0.0f).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(LOCTEXT("StateLabel", "State"))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 2.0f, 0.0f).VAlign(VAlign_Center)
		[
			MakeStateFilterCombo()
		]

		+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f, 2.0f, 0.0f).VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(LOCTEXT("OriginLabel", "Origin"))
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 2.0f, 0.0f).VAlign(VAlign_Center)
		[
			SNew(SComboBox<TSharedPtr<FString>>)
			.OptionsSource(&SourceTypeOptions)
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> Option)
			{
				return SNew(STextBlock).Text(FText::FromString(*Option));
			})
			.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Option, ESelectInfo::Type)
			{
				if (Option.IsValid())
				{
					SourceTypeFilter = *Option;
					RefreshFilteredEntries();
				}
			})
			[
				SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(SourceTypeFilter); })
			]
		]

		+ SHorizontalBox::Slot().AutoWidth().Padding(6.0f, 0.0f, 2.0f, 0.0f).VAlign(VAlign_Center)
		[
			MakePseudoCombo()
		]

		+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(6.0f, 0.0f).VAlign(VAlign_Center)
		[
			SNew(SSearchBox)
			.HintText(LOCTEXT("FilterHint", "Filter namespace, key, source, or translation..."))
			.OnTextChanged_Lambda([this](const FText& NewText)
			{
				FilterText = NewText.ToString();
				RefreshFilteredEntries();
			})
		]

		// B-5: the two most-used pipeline actions are first-class buttons.
		+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f, 2.0f, 0.0f).VAlign(VAlign_Center)
		[
			SNew(SButton)
			.Text(LOCTEXT("GatherButton", "Gather"))
			.ToolTipText(LOCTEXT("GatherButtonTooltip", "Scan code and assets for localizable text, then show the change set."))
			.IsEnabled_Lambda([this]() { return !SelectedTarget.IsEmpty(); })
			.OnClicked_Lambda([this]() { RunGatherWithChangeSet(); return FReply::Handled(); })
		]

		+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f, 0.0f).VAlign(VAlign_Center)
		[
			SNew(SButton)
			.Text(LOCTEXT("CompileButton", "Compile"))
			.ToolTipText(LOCTEXT("CompileButtonTooltip", "Compile localization resources and hot-reload them into the editor."))
			.IsEnabled_Lambda([this]() { return !SelectedTarget.IsEmpty(); })
			.OnClicked_Lambda([this]()
			{
				if (const TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared()))
				{
					if (EELocPipeline::Compile(SelectedTarget, Window.ToSharedRef()))
					{
						EELocPipeline::RefreshLiveResources();
						StatusNote = TEXT("Compiled and hot-reloaded localization resources.");
					}
				}
				return FReply::Handled();
			})
		]

		+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f).VAlign(VAlign_Center)
		[
			MakePipelineMenu()
		]

		+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f).VAlign(VAlign_Center)
		[
			SNew(SButton)
			.Text_Lambda([this]()
			{
				return Issues.Num() > 0
					? FText::Format(LOCTEXT("ValidateCountFmt", "Validate ({0})"), FText::AsNumber(Issues.Num()))
					: LOCTEXT("Validate", "Validate");
			})
			.ToolTipText(LOCTEXT("ValidateTooltip", "Run identity, placeholder, rich-text, whitespace, and state validation."))
			.OnClicked_Lambda([this]() { RunValidation(); return FReply::Handled(); })
		]

		+ SHorizontalBox::Slot().AutoWidth().Padding(4.0f, 0.0f).VAlign(VAlign_Center)
		[
			SNew(SButton)
			.Text_Lambda([this]()
			{
				return Session->HasUnsavedChanges() ? LOCTEXT("SaveDirty", "Save *") : LOCTEXT("Save", "Save");
			})
			.ToolTipText(LOCTEXT("SaveTooltip", "Write staged translations to the culture archives."))
			.OnClicked_Lambda([this]()
			{
				FString Error;
				if (!Session->Save(Error) || !ReviewStore->Save(Error))
				{
					LastError = Error;
				}
				return FReply::Handled();
			})
		];
}

TSharedRef<SWidget> SEELocalizationWorkbenchPanel::MakeTargetCombo()
{
	return SNew(SComboBox<TSharedPtr<FString>>)
		.OptionsSource(&TargetOptions)
		.OnGenerateWidget_Lambda([](TSharedPtr<FString> Option)
		{
			return SNew(STextBlock).Text(FText::FromString(*Option));
		})
		.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Option, ESelectInfo::Type)
		{
			if (Option.IsValid())
			{
				OpenTarget(*Option);
			}
		})
		[
			SNew(STextBlock)
			.Text_Lambda([this]()
			{
				return SelectedTarget.IsEmpty()
					? LOCTEXT("PickTarget", "Pick target...")
					: FText::FromString(SelectedTarget);
			})
		];
}

TSharedRef<SWidget> SEELocalizationWorkbenchPanel::MakeCultureCombo(const bool bCompare)
{
	return SNew(SComboBox<TSharedPtr<FString>>)
		.OptionsSource(&CultureOptions)
		.OnGenerateWidget_Lambda([](TSharedPtr<FString> Option)
		{
			return SNew(STextBlock).Text(Option->IsEmpty() ? LOCTEXT("NoCompare", "(none)") : FText::FromString(*Option));
		})
		.OnSelectionChanged_Lambda([this, bCompare](TSharedPtr<FString> Option, ESelectInfo::Type)
		{
			if (!Option.IsValid())
			{
				return;
			}
			if (bCompare)
			{
				CompareCulture = *Option;
			}
			else
			{
				SelectedCulture = *Option;
				SaveUIState();
			}
			UpdateCompareColumnVisibility();
			RefreshFilteredEntries();
		})
		[
			SNew(STextBlock)
			.Text_Lambda([this, bCompare]()
			{
				const FString& Culture = bCompare ? CompareCulture : SelectedCulture;
				return Culture.IsEmpty() ? LOCTEXT("NoCulture", "(none)") : FText::FromString(Culture);
			})
		];
}

void SEELocalizationWorkbenchPanel::ApplyToSelection(const TFunctionRef<void(const TSharedPtr<FEELocEntry>&)>& Operation)
{
	if (!EntryList.IsValid())
	{
		return;
	}

	for (const TSharedPtr<FEELocEntry>& Entry : EntryList->GetSelectedItems())
	{
		if (Entry.IsValid())
		{
			Operation(Entry);
		}
	}

	RefreshFilteredEntries();
}

TSharedPtr<SWidget> SEELocalizationWorkbenchPanel::MakeGridContextMenu()
{
	if (!EntryList.IsValid() || EntryList->GetSelectedItems().Num() == 0 || SelectedCulture.IsEmpty())
	{
		return nullptr;
	}

	FMenuBuilder MenuBuilder(/*bShouldCloseWindowAfterMenuSelection*/ true, nullptr);
	const int32 SelectionCount = EntryList->GetSelectedItems().Num();

	MenuBuilder.BeginSection(NAME_None,
		FText::Format(LOCTEXT("BatchSection", "{0} selected ({1})"), FText::AsNumber(SelectionCount), FText::FromString(SelectedCulture)));

	auto NotLocked = [this](const TSharedPtr<FEELocEntry>& Entry)
	{
		return !ReviewStore->IsLocked(Entry->Namespace, Entry->Key, SelectedCulture);
	};

	MenuBuilder.AddMenuEntry(
		LOCTEXT("BatchCopySource", "Copy Source to Translation"),
		LOCTEXT("BatchCopySourceTooltip", "For text that should stay identical (names, numbers). Skips Locked entries."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this, NotLocked]()
		{
			ApplyToSelection([this, &NotLocked](const TSharedPtr<FEELocEntry>& Entry)
			{
				FString Error;
				if (NotLocked(Entry))
				{
					Session->SetTranslation(Entry, SelectedCulture, Entry->SourceText, Error);
				}
			});
		})));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("BatchClear", "Clear Translation"),
		LOCTEXT("BatchClearTooltip", "Back to Missing so it gets retranslated. Skips Locked entries."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this, NotLocked]()
		{
			ApplyToSelection([this, &NotLocked](const TSharedPtr<FEELocEntry>& Entry)
			{
				FString Error;
				if (NotLocked(Entry))
				{
					Session->SetTranslation(Entry, SelectedCulture, FString(), Error);
				}
			});
		})));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("BatchReviewed", "Mark Human Reviewed"),
		FText::GetEmpty(),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]()
		{
			ApplyToSelection([this](const TSharedPtr<FEELocEntry>& Entry)
			{
				ReviewStore->SetState(Entry->Namespace, Entry->Key, SelectedCulture,
					EEELocReviewState::HumanReviewed, FPlatformProcess::UserName(), Entry->SourceText);
			});
		})));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("BatchLock", "Lock"),
		LOCTEXT("BatchLockTooltip", "Locked translations are refused by every write path until unlocked."),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]()
		{
			ApplyToSelection([this](const TSharedPtr<FEELocEntry>& Entry)
			{
				ReviewStore->SetState(Entry->Namespace, Entry->Key, SelectedCulture,
					EEELocReviewState::Locked, FPlatformProcess::UserName(), Entry->SourceText);
			});
		})));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("BatchUnlock", "Unlock"),
		FText::GetEmpty(),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]()
		{
			ApplyToSelection([this](const TSharedPtr<FEELocEntry>& Entry)
			{
				if (ReviewStore->IsLocked(Entry->Namespace, Entry->Key, SelectedCulture))
				{
					ReviewStore->SetState(Entry->Namespace, Entry->Key, SelectedCulture,
						EEELocReviewState::HumanReviewed, FPlatformProcess::UserName(), Entry->SourceText);
				}
			});
		})));

	MenuBuilder.EndSection();
	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SEELocalizationWorkbenchPanel::MakePipelineMenu()
{
	return SNew(SComboButton)
		.ButtonContent()
		[
			SNew(STextBlock).Text(LOCTEXT("Pipeline", "Pipeline"))
		]
		.OnGetMenuContent_Lambda([this]()
		{
			FMenuBuilder MenuBuilder(/*bShouldCloseWindowAfterMenuSelection*/ true, nullptr);

			auto GetWindow = [this]() { return FSlateApplication::Get().FindWidgetWindow(AsShared()); };

			MenuBuilder.AddMenuEntry(
				LOCTEXT("GatherMenu", "Gather (with change set)"),
				LOCTEXT("GatherMenuTooltip", "Run the gather commandlet, then show newly discovered, removed, and changed entries."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SEELocalizationWorkbenchPanel::RunGatherWithChangeSet)));

			MenuBuilder.AddMenuEntry(
				LOCTEXT("ImportMenu", "Import Translations..."),
				LOCTEXT("ImportMenuTooltip", "Import PO files through the standard pipeline."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([this, GetWindow]()
				{
					if (const TSharedPtr<SWindow> Window = GetWindow())
					{
						// B-5: report what the import actually changed.
						auto CountTranslated = [this]()
						{
							int32 Count = 0;
							for (const TSharedPtr<FEELocEntry>& Entry : Session->GetEntries())
							{
								for (const TPair<FString, FEELocTranslation>& Pair : Entry->Translations)
								{
									if (Pair.Value.State != EEELocEntryState::Missing)
									{
										++Count;
									}
								}
							}
							return Count;
						};

						const int32 Before = CountTranslated();
						if (EELocPipeline::Import(SelectedTarget, Window.ToSharedRef()))
						{
							OpenTarget(SelectedTarget);
							StatusNote = FString::Printf(TEXT("Import complete: %d translated entries (%+d)."),
								CountTranslated(), CountTranslated() - Before);
						}
					}
				})));

			MenuBuilder.AddMenuEntry(
				LOCTEXT("ExportMenu", "Export Translations..."),
				LOCTEXT("ExportMenuTooltip", "Export PO files through the standard pipeline."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([this, GetWindow]()
				{
					if (const TSharedPtr<SWindow> Window = GetWindow())
					{
						EELocPipeline::Export(SelectedTarget, Window.ToSharedRef());
					}
				})));

			MenuBuilder.AddMenuEntry(
				LOCTEXT("CompileMenu", "Compile + Refresh Live"),
				LOCTEXT("CompileMenuTooltip", "Compile LocRes and hot-reload localization resources into the editor."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([this, GetWindow]()
				{
					if (const TSharedPtr<SWindow> Window = GetWindow())
					{
						if (EELocPipeline::Compile(SelectedTarget, Window.ToSharedRef()))
						{
							EELocPipeline::RefreshLiveResources();
						}
					}
				})));

			MenuBuilder.AddMenuSeparator();

			MenuBuilder.AddMenuEntry(
				LOCTEXT("SetupMenu", "Localization Setup..."),
				LOCTEXT("SetupMenuTooltip", "Re-run the guided setup: target, cultures, gather paths, first gather."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([this]()
				{
					SetupState = EELocSetup::DetectState(SelectedTarget.IsEmpty() ? SetupTargetName : SelectedTarget);
					if (SetupState == EELocSetup::EEESetupState::Ready)
					{
						StatusNote = TEXT("Localization is fully configured — nothing for the setup wizard to do.");
					}
				})));

			MenuBuilder.AddMenuSeparator();

			MenuBuilder.AddMenuEntry(
				LOCTEXT("MachineDraftMenu", "Draft Missing (Machine)"),
				LOCTEXT("MachineDraftMenuTooltip", "Fill Missing translations for the selected culture via a registered translation provider. Results stage as Machine Draft; nothing is saved until you review and Save."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateSP(this, &SEELocalizationWorkbenchPanel::RunMachineDrafts)));

			MenuBuilder.AddMenuEntry(
				LOCTEXT("ContextPackageMenu", "Export Context Package (CSV)"),
				LOCTEXT("ContextPackageMenuTooltip", "Export identity, source, translation, states, review info, and provider context for the selected culture."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([this]()
				{
					FString FilePath, Error;
					if (EELocPipeline::ExportContextPackage(*Session, *ReviewStore, SelectedCulture, FilePath, Error))
					{
						LastError.Reset();
						StatusNote = FString::Printf(TEXT("Context package written to %s"), *FilePath);
					}
					else
					{
						LastError = Error;
					}
				})));

			return MenuBuilder.MakeWidget();
		});
}

void SEELocalizationWorkbenchPanel::RunGatherWithChangeSet()
{
	if (SelectedTarget.IsEmpty())
	{
		return;
	}

	// Snapshot identity -> source before the gather.
	TMap<FString, FString> Before;
	if (Session->IsOpen())
	{
		for (const TSharedPtr<FEELocEntry>& Entry : Session->GetEntries())
		{
			Before.Add(Entry->Namespace + TEXT(",") + Entry->Key, Entry->SourceText);
		}
	}

	const TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared());
	if (!Window.IsValid())
	{
		return;
	}

	if (!EELocPipeline::Gather(SelectedTarget, Window.ToSharedRef()))
	{
		LastError = TEXT("Gather failed or was cancelled.");
		return;
	}

	// Reopen and diff into the issues panel as the change set.
	OpenTarget(SelectedTarget);

	Issues.Reset();
	TSet<FString> SeenIdentities;
	int32 AddedCount = 0, ChangedCount = 0, RemovedCount = 0;

	for (const TSharedPtr<FEELocEntry>& Entry : Session->GetEntries())
	{
		const FString Identity = Entry->Namespace + TEXT(",") + Entry->Key;
		SeenIdentities.Add(Identity);

		if (const FString* OldSource = Before.Find(Identity))
		{
			if (*OldSource != Entry->SourceText)
			{
				++ChangedCount;
				Issues.Add(MakeShared<FEELocIssue>(FEELocIssue{Entry, FString(), TEXT("GatherDiff"),
					EEELocIssueSeverity::Warning, TEXT("Changed: source text differs from the previous gather.")}));
			}
		}
		else
		{
			++AddedCount;
			Issues.Add(MakeShared<FEELocIssue>(FEELocIssue{Entry, FString(), TEXT("GatherDiff"),
				EEELocIssueSeverity::Warning, TEXT("New: discovered by this gather.")}));
		}
	}

	for (const TPair<FString, FString>& Pair : Before)
	{
		if (!SeenIdentities.Contains(Pair.Key))
		{
			++RemovedCount;
			Issues.Add(MakeShared<FEELocIssue>(FEELocIssue{nullptr, FString(), TEXT("GatherDiff"),
				EEELocIssueSeverity::Warning,
				FString::Printf(TEXT("Removed: %s no longer exists (source was \"%s\")."), *Pair.Key, *Pair.Value)}));
		}
	}

	IssuesHeading = LOCTEXT("ChangeSetHeading", "Gather change set");
	bShowIssues = Issues.Num() > 0;
	if (IssueList.IsValid())
	{
		IssueList->RequestListRefresh();
	}

	StatusNote = FString::Printf(TEXT("Gather complete: %d new, %d changed, %d removed."), AddedCount, ChangedCount, RemovedCount);
}

void SEELocalizationWorkbenchPanel::RunMachineDrafts()
{
	if (!Session->IsOpen() || SelectedCulture.IsEmpty())
	{
		return;
	}

	FString UnavailableReason;
	const TSharedPtr<IEELocTranslationProvider> Provider = FEELocTranslationProviderRegistry::Get().GetFirstAvailable(UnavailableReason);
	if (!Provider.IsValid())
	{
		StatusNote = UnavailableReason;
		return;
	}

	// Batch every Missing entry for the culture; Locked entries are never requested.
	TArray<FEELocTranslationRequest> Requests;
	TMap<FString, TSharedPtr<FEELocEntry>> RequestEntries;

	for (const TSharedPtr<FEELocEntry>& Entry : Session->GetEntries())
	{
		const FEELocTranslation* Translation = Entry->Translations.Find(SelectedCulture);
		if (Translation && Translation->State != EEELocEntryState::Missing)
		{
			continue;
		}
		if (ReviewStore->IsLocked(Entry->Namespace, Entry->Key, SelectedCulture))
		{
			continue;
		}

		FEELocTranslationRequest Request;
		Request.Namespace = Entry->Namespace;
		Request.Key = Entry->Key;
		Request.SourceText = Entry->SourceText;
		Request.NativeCulture = Session->GetNativeCulture();
		Request.TargetCulture = SelectedCulture;

		FEELocContext Context;
		FEELocContextProviderRegistry::Get().BuildContext(*Entry, Context);
		for (const FEELocContextField& Field : Context.Fields)
		{
			Request.ContextText += FString::Printf(TEXT("%s: %s; "), *Field.Label, *Field.Value);
		}

		// Protected tokens: {named} arguments must survive translation verbatim.
		int32 SearchIndex = 0;
		while (true)
		{
			const int32 Open = Entry->SourceText.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchIndex);
			if (Open == INDEX_NONE)
			{
				break;
			}
			const int32 Close = Entry->SourceText.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Open + 1);
			if (Close == INDEX_NONE)
			{
				break;
			}
			Request.ProtectedTokens.Add(Entry->SourceText.Mid(Open, Close - Open + 1));
			SearchIndex = Close + 1;
		}

		RequestEntries.Add(Request.Namespace + TEXT(",") + Request.Key, Entry);
		Requests.Add(MoveTemp(Request));
	}

	if (Requests.Num() == 0)
	{
		StatusNote = TEXT("No unlocked Missing entries to draft.");
		return;
	}

	const FText CostEstimate = Provider->GetCostEstimate(Requests.Num());
	TMap<FString, FString> Drafts;
	FString Error;
	if (!Provider->Translate(Requests, Drafts, Error))
	{
		LastError = FString::Printf(TEXT("Provider '%s' failed: %s"), *Provider->GetProviderName().ToString(), *Error);
		return;
	}

	int32 Applied = 0, Dropped = 0;
	for (const TPair<FString, FString>& Draft : Drafts)
	{
		const TSharedPtr<FEELocEntry>* Entry = RequestEntries.Find(Draft.Key);
		if (!Entry)
		{
			continue;
		}

		// Drop drafts that lost a protected token.
		bool bTokensIntact = true;
		for (const FEELocTranslationRequest& Request : Requests)
		{
			if (Request.Namespace + TEXT(",") + Request.Key != Draft.Key)
			{
				continue;
			}
			for (const FString& Token : Request.ProtectedTokens)
			{
				if (!Draft.Value.Contains(Token))
				{
					bTokensIntact = false;
					break;
				}
			}
			break;
		}
		if (!bTokensIntact)
		{
			++Dropped;
			continue;
		}

		if (Session->SetTranslation(*Entry, SelectedCulture, Draft.Value, Error))
		{
			// Always Machine Draft — never auto-reviewed; user reviews before Save.
			ReviewStore->SetState((*Entry)->Namespace, (*Entry)->Key, SelectedCulture,
				EEELocReviewState::MachineDraft, Provider->GetProviderName().ToString(), (*Entry)->SourceText);
			++Applied;
		}
	}

	StatusNote = FString::Printf(TEXT("Machine drafts staged: %d applied, %d dropped (token loss), %d requested.%s%s Review, then Save."),
		Applied, Dropped, Requests.Num(),
		CostEstimate.IsEmpty() ? TEXT("") : TEXT(" Cost: "),
		CostEstimate.IsEmpty() ? TEXT("") : *CostEstimate.ToString());
	RefreshFilteredEntries();
}

TSharedRef<SWidget> SEELocalizationWorkbenchPanel::MakePseudoCombo()
{
	for (const EEELocPseudoType Type : EELocPseudo::AllTypes)
	{
		PseudoOptions.Add(MakeShared<EEELocPseudoType>(Type));
	}

	return SNew(SComboBox<TSharedPtr<EEELocPseudoType>>)
		.OptionsSource(&PseudoOptions)
		.OnGenerateWidget_Lambda([](TSharedPtr<EEELocPseudoType> Option)
		{
			return SNew(STextBlock).Text(EELocPseudo::TypeToLabel(*Option));
		})
		.OnSelectionChanged_Lambda([this](TSharedPtr<EEELocPseudoType> Option, ESelectInfo::Type)
		{
			if (Option.IsValid())
			{
				PseudoType = *Option;
				UpdateCompareColumnVisibility();
				RefreshFilteredEntries();
			}
		})
		.ToolTipText(LOCTEXT("PseudoTooltip", "Preview-only pseudo-localization of the source text in the Compare column. Never written to archives."))
		[
			SNew(STextBlock)
			.Text_Lambda([this]() { return EELocPseudo::TypeToLabel(PseudoType); })
		];
}

TSharedRef<SWidget> SEELocalizationWorkbenchPanel::MakeStateFilterCombo()
{
	return SNew(SComboBox<TSharedPtr<FString>>)
		.OptionsSource(&StateFilterOptions)
		.OnGenerateWidget_Lambda([](TSharedPtr<FString> Option)
		{
			return SNew(STextBlock).Text(FText::FromString(*Option));
		})
		.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Option, ESelectInfo::Type)
		{
			if (Option.IsValid())
			{
				StateFilter = *Option;
				RefreshFilteredEntries();
			}
		})
		[
			SNew(STextBlock)
			.Text_Lambda([this]() { return FText::FromString(StateFilter); })
		];
}

TSharedRef<ITableRow> SEELocalizationWorkbenchPanel::MakeEntryRow(TSharedPtr<FEELocEntry> Entry, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SEELocGridRow, OwnerTable, Entry, this);
}

TSharedRef<SWidget> SEELocalizationWorkbenchPanel::MakeContextInspector()
{
	return SNew(SBox)
		.WidthOverride(320.0f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().Padding(4.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ContextTitle", "Context"))
				.Font(FAppStyle::GetFontStyle("BoldFont"))
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(4.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("OpenUsage", "Open Usage"))
				.ToolTipText(LOCTEXT("OpenUsageTooltip", "Open the owning asset or source location."))
				.OnClicked_Lambda([this]()
				{
					if (SelectedEntry.IsValid())
					{
						FEELocContextProviderRegistry::Get().OpenUsage(*SelectedEntry);
					}
					return FReply::Handled();
				})
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(4.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("PreviewInUILab", "Preview in UI Lab"))
				.ToolTipText(LOCTEXT("PreviewInUILabTooltip", "Host the owning Widget Blueprint in the UI Lab with this culture's preview."))
				.OnClicked_Lambda([this]()
				{
					if (!SelectedEntry.IsValid())
					{
						return FReply::Handled();
					}

					// Owner asset path is the leading "/Game/..." portion of the source location.
					FString AssetPath = SelectedEntry->SourceLocation;
					int32 SpaceIndex;
					if (AssetPath.FindChar(TEXT(' '), SpaceIndex))
					{
						AssetPath.LeftInline(SpaceIndex);
					}
					int32 ColonIndex;
					if (AssetPath.FindChar(TEXT(':'), ColonIndex))
					{
						AssetPath.LeftInline(ColonIndex);
					}

					const UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(FSoftObjectPath(AssetPath).TryLoad());
					if (!WidgetBlueprint || !WidgetBlueprint->GeneratedClass)
					{
						StatusNote = TEXT("The selected entry's owner is not a Widget Blueprint.");
						return FReply::Handled();
					}

					FEEUILabFeature::OpenWidgetInLab(WidgetBlueprint->GeneratedClass, SelectedCulture);
					return FReply::Handled();
				})
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(4.0f)
			[
				MakeReviewSection()
			]

			+ SVerticalBox::Slot().FillHeight(1.0f).Padding(4.0f)
			[
				SAssignNew(ContextFieldsBox, SVerticalBox)
			]
		];
}

TSharedRef<SWidget> SEELocalizationWorkbenchPanel::MakeReviewSection()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ReviewTitle", "Review"))
			.Font(FAppStyle::GetFontStyle("BoldFont"))
		]

		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SComboBox<TSharedPtr<FString>>)
			.OptionsSource(&ReviewStateOptions)
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> Option)
			{
				return SNew(STextBlock).Text(FText::FromString(*Option));
			})
			.OnSelectionChanged_Lambda([this](TSharedPtr<FString> Option, ESelectInfo::Type SelectInfo)
			{
				if (!Option.IsValid() || SelectInfo == ESelectInfo::Direct || !SelectedEntry.IsValid() || SelectedCulture.IsEmpty())
				{
					return;
				}
				ReviewStore->SetState(SelectedEntry->Namespace, SelectedEntry->Key, SelectedCulture,
					FEELocReviewStore::ReviewStateFromString(*Option),
					FPlatformProcess::UserName(), SelectedEntry->SourceText);
			})
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					if (!SelectedEntry.IsValid() || SelectedCulture.IsEmpty())
					{
						return LOCTEXT("NoReviewSelection", "(select an entry)");
					}
					const FEELocReviewRecord Record = ReviewStore->GetRecord(SelectedEntry->Namespace, SelectedEntry->Key, SelectedCulture);
					return FText::FromString(FEELocReviewStore::ReviewStateToString(Record.State));
				})
			]
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
		[
			SNew(STextBlock)
			.Text_Lambda([this]()
			{
				if (!SelectedEntry.IsValid() || SelectedCulture.IsEmpty())
				{
					return FText::GetEmpty();
				}
				const FEELocReviewRecord Record = ReviewStore->GetRecord(SelectedEntry->Namespace, SelectedEntry->Key, SelectedCulture);
				FString Info;
				if (!Record.Reviewer.IsEmpty())
				{
					Info += FString::Printf(TEXT("Reviewer: %s"), *Record.Reviewer);
				}
				if (!Record.SourceTextAtReview.IsEmpty() && SelectedEntry.IsValid() && Record.SourceTextAtReview != SelectedEntry->SourceText)
				{
					Info += Info.IsEmpty() ? TEXT("") : TEXT("\n");
					Info += TEXT("Source changed since review!");
				}
				for (const FString& Comment : Record.Comments)
				{
					Info += Info.IsEmpty() ? TEXT("") : TEXT("\n");
					Info += TEXT("• ") + Comment;
				}
				return FText::FromString(Info);
			})
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.5f)))
		]

		// B-4: comment entry (storage existed since LW-5; this is its UI).
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
		[
			SNew(SEditableTextBox)
			.HintText(LOCTEXT("AddCommentHint", "Add a review comment and press Enter..."))
			.IsEnabled_Lambda([this]() { return SelectedEntry.IsValid() && !SelectedCulture.IsEmpty(); })
			.OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type CommitType)
			{
				if (CommitType == ETextCommit::OnEnter && SelectedEntry.IsValid() && !NewText.IsEmpty())
				{
					ReviewStore->AddComment(SelectedEntry->Namespace, SelectedEntry->Key, SelectedCulture,
						FString::Printf(TEXT("%s: %s"), FPlatformProcess::UserName(), *NewText.ToString()));
				}
			})
			.ClearKeyboardFocusOnCommit(true)
		];
}

void SEELocalizationWorkbenchPanel::HandleSelectionChanged(TSharedPtr<FEELocEntry> Entry, ESelectInfo::Type SelectInfo)
{
	SelectedEntry = Entry;

	if (!ContextFieldsBox.IsValid())
	{
		return;
	}
	ContextFieldsBox->ClearChildren();

	if (!Entry.IsValid())
	{
		return;
	}

	FEELocContext Context;
	Context.Add(TEXT("Namespace"), Entry->Namespace);
	Context.Add(TEXT("Key"), Entry->Key);
	Context.Add(TEXT("Location"), Entry->SourceLocation);
	FEELocContextProviderRegistry::Get().BuildContext(*Entry, Context);

	for (const FEELocContextField& Field : Context.Fields)
	{
		ContextFieldsBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 2.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Field.Label))
				.Font(FAppStyle::GetFontStyle("SmallFontBold"))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.8f, 1.0f)))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Field.Value))
				.AutoWrapText(true)
			]
		];
	}
}

void SEELocalizationWorkbenchPanel::OpenTarget(const FString& TargetName)
{
	LastError.Reset();
	SelectedTarget = TargetName;
	if (!TargetName.IsEmpty())
	{
		SetupTargetName = TargetName;
	}

	// B-1: a broken/empty pipeline routes to the setup wizard instead of a dead grid.
	SetupState = EELocSetup::DetectState(TargetName);
	if (SetupState != EELocSetup::EEESetupState::Ready)
	{
		Session->Close();
		RefreshCultureOptions();
		RefreshFilteredEntries();
		return;
	}

	FString Error;
	if (!Session->Open(TargetName, Error))
	{
		LastError = Error;
	}

	ReviewStore->Load(TargetName);
	RefreshCultureOptions();
	RefreshFilteredEntries();
}

void SEELocalizationWorkbenchPanel::RefreshSetupState()
{
	const FString TargetName = SelectedTarget.IsEmpty() ? SetupTargetName : SelectedTarget;
	SetupState = EELocSetup::DetectState(TargetName);

	if (SetupState == EELocSetup::EEESetupState::Ready)
	{
		OpenTarget(TargetName);
	}
}

TSharedRef<SWidget> SEELocalizationWorkbenchPanel::MakeSetupPanel()
{
	auto MakeStepVisibility = [this](const EELocSetup::EEESetupState State)
	{
		return TAttribute<EVisibility>::CreateLambda([this, State]()
		{
			return SetupState == State ? EVisibility::Visible : EVisibility::Collapsed;
		});
	};

	return SNew(SBox)
		.WidthOverride(640.0f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SetupTitle", "Localization Setup"))
				.Font(FAppStyle::GetFontStyle("HeadingExtraSmall"))
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 12.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return EELocSetup::DescribeState(SetupState); })
				.AutoWrapText(true)
			]

			// Step 1: create target
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				.Visibility(MakeStepVisibility(EELocSetup::EEESetupState::NoTargets))

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("TargetNameLabel", "Target name"))
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SBox).WidthOverride(160.0f)
					[
						SNew(SEditableTextBox)
						.Text_Lambda([this]() { return FText::FromString(SetupTargetName); })
						.OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type) { SetupTargetName = NewText.ToString(); })
					]
				]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("CreateTarget", "Create Target"))
					.OnClicked_Lambda([this]()
					{
						FString Error;
						if (EELocSetup::CreateGameTarget(SetupTargetName, Error))
						{
							TargetOptions.Add(MakeShared<FString>(SetupTargetName));
							SelectedTarget = SetupTargetName;
							if (SetupTargetName != TEXT("Game"))
							{
								StatusNote = TEXT("Note: targets not named 'Game' need a LocalizationPaths entry in DefaultGame.ini ([Internationalization] +LocalizationPaths=%GAMEDIR%Content/Localization/") + SetupTargetName + TEXT(") to load at runtime.");
							}
							RefreshSetupState();
						}
						else
						{
							LastError = Error;
						}
						return FReply::Handled();
					})
				]
			]

			// Step 2: cultures
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SVerticalBox)
				.Visibility(MakeStepVisibility(EELocSetup::EEESetupState::NoCultures))

				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						SNew(STextBlock).Text(LOCTEXT("NativeCultureLabel", "Native culture (source language)"))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(80.0f)
						[
							SNew(SEditableTextBox)
							.Text_Lambda([this]() { return FText::FromString(SetupNativeCulture); })
							.OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type) { SetupNativeCulture = NewText.ToString(); })
						]
					]
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						SNew(STextBlock).Text(LOCTEXT("ForeignCulturesLabel", "Translate into (comma-separated, e.g. tr, de, fr)"))
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[
						SNew(SBox).WidthOverride(200.0f)
						[
							SNew(SEditableTextBox)
							.Text_Lambda([this]() { return FText::FromString(SetupForeignCultures); })
							.OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type) { SetupForeignCultures = NewText.ToString(); })
						]
					]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("ConfigureCultures", "Configure Cultures"))
						.OnClicked_Lambda([this]()
						{
							TArray<FString> Foreign;
							SetupForeignCultures.ParseIntoArray(Foreign, TEXT(","));

							FString Error;
							if (EELocSetup::ConfigureCultures(SetupTargetName, SetupNativeCulture, Foreign, Error))
							{
								RefreshSetupState();
							}
							else
							{
								LastError = Error;
							}
							return FReply::Handled();
						})
					]
				]
			]

			// Step 3: gather paths
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SVerticalBox)
				.Visibility(MakeStepVisibility(EELocSetup::EEESetupState::NoGatherPaths))

				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([this]() { return bSetupGatherSource ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState) { bSetupGatherSource = NewState == ECheckBoxState::Checked; })
					[
						SNew(STextBlock).Text(LOCTEXT("GatherSource", "Gather C++ source (Source/ — LOCTEXT, NSLOCTEXT)"))
					]
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([this]() { return bSetupGatherContent ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this](ECheckBoxState NewState) { bSetupGatherContent = NewState == ECheckBoxState::Checked; })
					[
						SNew(STextBlock).Text(LOCTEXT("GatherContent", "Gather assets (Content/* — widgets, DataTables, StringTables)"))
					]
				]

				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
				[
					SNew(SBox).HAlign(HAlign_Left)
					[
						SNew(SButton)
						.Text(LOCTEXT("AddGatherPaths", "Add Gather Paths"))
						.OnClicked_Lambda([this]()
						{
							FString Error;
							if (EELocSetup::AddDefaultGatherPaths(SetupTargetName, bSetupGatherSource, bSetupGatherContent, Error))
							{
								RefreshSetupState();
							}
							else
							{
								LastError = Error;
							}
							return FReply::Handled();
						})
					]
				]
			]

			// Step 4: first gather
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SBox)
				.Visibility(MakeStepVisibility(EELocSetup::EEESetupState::NeverGathered))
				.HAlign(HAlign_Left)
				[
					SNew(SButton)
					.Text(LOCTEXT("RunFirstGather", "Run First Gather + Compile"))
					.OnClicked_Lambda([this]()
					{
						const TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared());
						if (!Window.IsValid())
						{
							return FReply::Handled();
						}

						if (EELocPipeline::Gather(SetupTargetName, Window.ToSharedRef()))
						{
							EELocPipeline::Compile(SetupTargetName, Window.ToSharedRef());
							StatusNote = TEXT("First gather complete. Welcome to the workbench!");
							RefreshSetupState();
						}
						else
						{
							LastError = TEXT("Gather failed or was cancelled — see the Output Log for the commandlet report.");
						}
						return FReply::Handled();
					})
				]
			]

			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return FText::FromString(LastError.IsEmpty() ? StatusNote : TEXT("Error: ") + LastError); })
				.AutoWrapText(true)
				.ColorAndOpacity_Lambda([this]() { return FSlateColor(LastError.IsEmpty() ? FLinearColor(0.7f, 0.9f, 0.7f) : FLinearColor(1.0f, 0.4f, 0.4f)); })
			]
		];
}

void SEELocalizationWorkbenchPanel::RefreshCultureOptions()
{
	CultureOptions.Reset();
	CultureOptions.Add(MakeShared<FString>(TEXT("")));

	if (Session->IsOpen())
	{
		for (const FString& Culture : Session->GetForeignCultures())
		{
			CultureOptions.Add(MakeShared<FString>(Culture));
		}

		// Restore the per-project last culture when it still exists on this target.
		FString LastCulture;
		GConfig->GetString(TEXT("ExtendedLocalizationWorkbench"), TEXT("LastCulture"), LastCulture, GEditorPerProjectIni);

		if (!LastCulture.IsEmpty() && Session->GetForeignCultures().Contains(LastCulture))
		{
			SelectedCulture = LastCulture;
		}
		else
		{
			SelectedCulture = Session->GetForeignCultures().Num() > 0 ? Session->GetForeignCultures()[0] : FString();
		}
		CompareCulture.Reset();
		SaveUIState();
	}

	RebuildOverview();
	UpdateCompareColumnVisibility();
}

void SEELocalizationWorkbenchPanel::RefreshFilteredEntries()
{
	FilteredEntries.Reset();

	if (Session->IsOpen())
	{
		for (const TSharedPtr<FEELocEntry>& Entry : Session->GetEntries())
		{
			// State filter (against the selected culture).
			if (StateFilter != TEXT("All"))
			{
				const FEELocTranslation* Translation = Entry->Translations.Find(SelectedCulture);
				const EEELocEntryState State = Translation ? Translation->State : EEELocEntryState::Missing;
				if (StateToText(State).ToString() != StateFilter)
				{
					continue;
				}
			}

			// Source-type filter (B-4).
			if (SourceTypeFilter != TEXT("All") && Entry->SourceType != SourceTypeFilter)
			{
				continue;
			}

			// Text filter across identity, source, and the selected translation.
			if (!FilterText.IsEmpty())
			{
				const FEELocTranslation* Translation = Entry->Translations.Find(SelectedCulture);
				const bool bMatches =
					Entry->Namespace.Contains(FilterText) ||
					Entry->Key.Contains(FilterText) ||
					Entry->SourceText.Contains(FilterText) ||
					(Translation && Translation->Text.Contains(FilterText));
				if (!bMatches)
				{
					continue;
				}
			}

			FilteredEntries.Add(Entry);
		}
	}

	RebuildCultureStats();

	if (EntryList.IsValid())
	{
		EntryList->RequestListRefresh();
	}
}

void SEELocalizationWorkbenchPanel::CommitTranslation(const TSharedPtr<FEELocEntry>& Entry, const FString& NewText)
{
	// Locked translations are never modified without an explicit unlock (LW-5).
	if (ReviewStore->IsLocked(Entry->Namespace, Entry->Key, SelectedCulture))
	{
		LastError = FString::Printf(TEXT("'%s , %s' (%s) is Locked. Change its review state to edit it."),
			*Entry->Namespace, *Entry->Key, *SelectedCulture);
		return;
	}

	FString Error;
	if (!Session->SetTranslation(Entry, SelectedCulture, NewText, Error))
	{
		LastError = Error;
		return;
	}

	// A fresh human edit is a Draft until reviewed.
	ReviewStore->SetState(Entry->Namespace, Entry->Key, SelectedCulture,
		EEELocReviewState::Draft, FString(), Entry->SourceText);

	LastError.Reset();
	RefreshFilteredEntries();
}

FText SEELocalizationWorkbenchPanel::GetStatusText() const
{
	if (!LastError.IsEmpty())
	{
		return FText::FromString(FString::Printf(TEXT("Error: %s"), *LastError));
	}

	if (!Session->IsOpen())
	{
		if (TargetOptions.Num() == 0)
		{
			return LOCTEXT("NoTargets", "No game localization targets are configured. Create one in the Localization Dashboard first — the workbench orchestrates the standard pipeline, it does not replace it.");
		}
		return LOCTEXT("NoSession", "Pick a localization target to open its manifest and archives.");
	}

	FString Status = FString::Printf(TEXT("%s — %d entries (%d shown), native %s."),
		*Session->GetTargetName(), Session->GetEntries().Num(), FilteredEntries.Num(), *Session->GetNativeCulture());

	if (!SelectedCulture.IsEmpty())
	{
		Status += FString::Printf(TEXT("   %s: %d missing, %d stale, %d identical, %d translated."),
			*SelectedCulture,
			Session->CountState(SelectedCulture, EEELocEntryState::Missing),
			Session->CountState(SelectedCulture, EEELocEntryState::Stale),
			Session->CountState(SelectedCulture, EEELocEntryState::Identical),
			Session->CountState(SelectedCulture, EEELocEntryState::Translated));
	}

	if (Session->HasUnsavedChanges())
	{
		Status += TEXT("   [unsaved changes]");
	}

	if (!StatusNote.IsEmpty())
	{
		Status += TEXT("\n") + StatusNote;
	}

	return FText::FromString(Status);
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR
