// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianTypes.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SHeaderRow.h"

class FActiveTimerHandle;
class ITableRow;
class SEditableTextBox;
class SExtendedAtlassianDocumentView;
class SMultiLineEditableTextBox;
class STableViewBase;
class SWidget;

template <typename ItemType> class SListView;
template <typename OptionType> class SComboBox;

/**
 * Docked tab listing Jira issues for a JQL query, with a detail pane for the selected issue.
 *
 * Status transitions apply optimistically — the row updates immediately and rolls back with a
 * notification if Jira rejects the change — because round-tripping a workflow transition is slow
 * enough that a blocking UI would be worse than an occasional rollback.
 */
class SExtendedAtlassianIssueBrowser : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExtendedAtlassianIssueBrowser) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SExtendedAtlassianIssueBrowser() override;

	/** Re-runs the current query. Ignored while a request is already in flight. */
	void Refresh();

	/**
	 * Opens the create form and re-runs the query once Jira accepts the issue.
	 *
	 * The new issue is selected if the current query matches it. It often will not — creating an
	 * unassigned task while "My open issues" is selected is the ordinary case — so a miss says so
	 * rather than leaving the list looking unchanged.
	 */
	void OpenNewIssueDialog();

private:
	using FIssuePtr = TSharedPtr<FExtendedAtlassianIssue>;
	using FTransitionPtr = TSharedPtr<FExtendedAtlassianTransition>;
	using FCommentPtr = TSharedPtr<FExtendedAtlassianComment>;

	TSharedRef<SWidget> BuildQueryBar();
	TSharedRef<SWidget> BuildIssueList();
	TSharedRef<SWidget> BuildDetailPane();

	TSharedRef<ITableRow> OnGenerateIssueRow(FIssuePtr Item, const TSharedRef<STableViewBase>& OwnerTable);
	TSharedRef<ITableRow> OnGenerateCommentRow(FCommentPtr Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnIssueSelectionChanged(FIssuePtr Item, ESelectInfo::Type SelectInfo);

	void OnSortModeChanged(EColumnSortPriority::Type Priority, const FName& ColumnId, EColumnSortMode::Type NewMode);
	EColumnSortMode::Type GetSortModeForColumn(FName ColumnId) const;
	void ApplySort();

	void LoadDetailsFor(FIssuePtr Issue);
	void RefreshDescription();

	/** Consumes PendingSelectKey against the freshly loaded results. */
	void SelectPendingIssue();
	void PostComment();
	void ApplyTransition(FTransitionPtr Transition);
	void ArchiveSelectedIssue();

	EActiveTimerReturnType HandlePollTimer(double InCurrentTime, float InDeltaTime);

	/** Runs the first query as soon as credentials become usable, so connecting fills the tab. */
	void HandleAuthStateChanged();

	void SetStatus(const FText& Message, bool bIsError);

	FDelegateHandle AuthChangedHandle;

	void OnPresetChanged(TSharedPtr<FString> NewPreset, ESelectInfo::Type SelectInfo);

	/** Query presets from settings, plus a "Custom" entry for hand-written JQL. */
	TArray<TSharedPtr<FString>> PresetNames;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> PresetCombo;
	TSharedPtr<SEditableTextBox> JqlBox;
	FString CurrentJql;

	TArray<FIssuePtr> Issues;
	TSharedPtr<SListView<FIssuePtr>> IssueListView;
	FIssuePtr SelectedIssue;
	TSharedPtr<SExtendedAtlassianDocumentView> DescriptionView;

	/** Key to select after the next query returns, set when an issue is created from this tab. */
	FString PendingSelectKey;

	TArray<FTransitionPtr> Transitions;
	TSharedPtr<SComboBox<FTransitionPtr>> TransitionCombo;

	TArray<FCommentPtr> Comments;
	TSharedPtr<SListView<FCommentPtr>> CommentListView;
	TSharedPtr<SMultiLineEditableTextBox> CommentBox;

	bool bLoading = false;
	bool bDetailLoading = false;
	bool bPostingComment = false;

	FText StatusMessage;
	bool bStatusIsError = false;

	FName SortColumn;
	EColumnSortMode::Type SortMode = EColumnSortMode::Descending;

	double LastPollTime = 0.0;
	TSharedPtr<FActiveTimerHandle> PollTimerHandle;
};
