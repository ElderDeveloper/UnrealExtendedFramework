// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Localization/Model/EELocalizationSession.h"
#include "Localization/Pipeline/EELocSetup.h"
#include "Localization/Pseudo/EELocPseudo.h"
#include "Localization/Review/EELocReviewStore.h"
#include "Localization/Validation/EELocalizationValidator.h"
#include "Widgets/SCompoundWidget.h"

class ITableRow;
class SHorizontalBox;
class STableViewBase;
class SVerticalBox;
template <typename ItemType> class SListView;

/**
 * Extended Localization Workbench main panel.
 *
 * LW-1: opens a localization target through the session model.
 * LW-2: source browser (text + state filters) and translation grid — namespace/key, native
 * source, editable selected-culture translation, optional comparison culture, and state —
 * over a virtualized list so large targets stay responsive. Edits stage in the session;
 * Save writes archives through engine serialization.
 */
class SEELocalizationWorkbenchPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEELocalizationWorkbenchPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// Grid row callbacks
	FEELocalizationSession& GetSession() const { return *Session; }
	FEELocReviewStore& GetReviewStore() const { return *ReviewStore; }
	const FString& GetSelectedCulture() const { return SelectedCulture; }
	const FString& GetCompareCulture() const { return CompareCulture; }
	EEELocPseudoType GetPseudoType() const { return PseudoType; }
	void CommitTranslation(const TSharedPtr<FEELocEntry>& Entry, const FString& NewText);

private:
	TSharedRef<SWidget> MakeToolbar();
	TSharedRef<SWidget> MakeTargetCombo();
	TSharedRef<SWidget> MakeCultureCombo(bool bCompare);
	TSharedRef<SWidget> MakeStateFilterCombo();
	TSharedRef<ITableRow> MakeEntryRow(TSharedPtr<FEELocEntry> Entry, const TSharedRef<STableViewBase>& OwnerTable);

	void OpenTarget(const FString& TargetName);
	void RefreshCultureOptions();
	void RefreshFilteredEntries();
	FText GetStatusText() const;

	TSharedPtr<FEELocalizationSession> Session;

	// Options
	TArray<TSharedPtr<FString>> TargetOptions;
	TArray<TSharedPtr<FString>> CultureOptions;
	TArray<TSharedPtr<FString>> StateFilterOptions;

	// Filters
	FString SelectedTarget;
	FString SelectedCulture;
	FString CompareCulture;
	FString FilterText;
	FString StateFilter = TEXT("All");
	FString SourceTypeFilter = TEXT("All");
	TArray<TSharedPtr<FString>> SourceTypeOptions;

	// Batch operations (B-4)
	TSharedPtr<SWidget> MakeGridContextMenu();
	void ApplyToSelection(const TFunctionRef<void(const TSharedPtr<FEELocEntry>&)>& Operation);

	// Grid
	TArray<TSharedPtr<FEELocEntry>> FilteredEntries;
	TSharedPtr<SListView<TSharedPtr<FEELocEntry>>> EntryList;
	TSharedPtr<class SHeaderRow> HeaderRow;
	void UpdateCompareColumnVisibility();

	// Overview header (B-2)
	struct FEECultureStats
	{
		int32 Missing = 0;
		int32 Stale = 0;
		int32 Identical = 0;
		int32 Translated = 0;
		int32 Total() const { return Missing + Stale + Identical + Translated; }
	};
	void RebuildCultureStats();
	void RebuildOverview();
	TMap<FString, FEECultureStats> CultureStats;
	TSharedPtr<SHorizontalBox> OverviewBox;

	// Per-project memory (B-2)
	void SaveUIState() const;

	// Onboarding wizard (B-1)
	TSharedRef<SWidget> MakeSetupPanel();
	void RefreshSetupState();
	EELocSetup::EEESetupState SetupState = EELocSetup::EEESetupState::Ready;
	FString SetupTargetName = TEXT("Game");
	FString SetupNativeCulture = TEXT("en");
	FString SetupForeignCultures = TEXT("tr");
	bool bSetupGatherSource = true;
	bool bSetupGatherContent = true;

	// Pipeline orchestration (LW-7)
	TSharedRef<SWidget> MakePipelineMenu();
	void RunGatherWithChangeSet();

	// Machine drafts (LW-8)
	void RunMachineDrafts();

	// Pseudo-localization (LW-6): preview layer, never written to archives.
	TSharedRef<SWidget> MakePseudoCombo();
	TArray<TSharedPtr<EEELocPseudoType>> PseudoOptions;
	EEELocPseudoType PseudoType = EEELocPseudoType::None;

	// Review state (LW-5)
	TSharedRef<SWidget> MakeReviewSection();
	TSharedPtr<FEELocReviewStore> ReviewStore;
	TArray<TSharedPtr<FString>> ReviewStateOptions;

	// Validation (LW-4) + one-click fixes (B-3)
	TSharedRef<ITableRow> MakeIssueRow(TSharedPtr<FEELocIssue> Issue, const TSharedRef<STableViewBase>& OwnerTable);
	void RunValidation();
	/** Applies the safe automated remedy for an issue. bAlternate picks the secondary action (e.g. Clear instead of Keep). */
	bool ApplyIssueFix(const FEELocIssue& Issue, bool bAlternate);
	void FixAllShownIssues();
	TArray<TSharedPtr<FEELocIssue>> Issues;
	TSharedPtr<SListView<TSharedPtr<FEELocIssue>>> IssueList;
	FText IssuesHeading;
	bool bShowIssues = false;

	// Context inspector (LW-3)
	TSharedRef<SWidget> MakeContextInspector();
	void HandleSelectionChanged(TSharedPtr<FEELocEntry> Entry, ESelectInfo::Type SelectInfo);
	TSharedPtr<FEELocEntry> SelectedEntry;
	TSharedPtr<SVerticalBox> ContextFieldsBox;

	FString LastError;
	/** Non-error status note (gather summaries, export paths). */
	FString StatusNote;
};

#endif // WITH_EDITOR
