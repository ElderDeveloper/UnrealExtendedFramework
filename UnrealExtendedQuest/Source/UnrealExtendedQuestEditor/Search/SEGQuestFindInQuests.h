// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "Framework/Commands/UICommandList.h"

#include "EGQuestSearchResult.h"

class FEGQuestEditor;
class SSearchBox;
class SDockTab;

/**  Widget for searching across all quests or just a single quest */
class UNREALEXTENDEDQUESTEDITOR_API SEGQuestFindInQuests : public SCompoundWidget
{
private:
	typedef SEGQuestFindInQuests Self;

public:
	SLATE_BEGIN_ARGS(Self)
		: _bIsSearchWindow(true)
		, _bHideSearchBar(false)
		, _ContainingTab()
	{}
		SLATE_ARGUMENT(bool, bIsSearchWindow)
		SLATE_ARGUMENT(bool, bHideSearchBar)
		SLATE_ARGUMENT(TSharedPtr<SDockTab>, ContainingTab)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedPtr<FEGQuestEditor>& InQuestEditor = nullptr);
	~SEGQuestFindInQuests();

	/** Focuses this widget's search box, and changes the mode as well, and optionally the search terms */
	void FocusForUse(bool bSetFindWithinQuest, const FEGQuestSearchFilter& SearchFilter = FEGQuestSearchFilter(), bool bSelectFirstResult = false);

	/**
	 * Submits a search query
	 *
	 * @param SearchFilter						Filter for search
	 * @param bInIsFindWithinQuest			TRUE if searching within the current Quest only
	 */
	void MakeSearchQuery(const FEGQuestSearchFilter& SearchFilter, bool bInIsFindWithinQuest);

	/** If this is a global find results widget, returns the host tab's unique ID. Otherwise, returns NAME_None. */
	FName GetHostTabId() const;

	/** If this is a global find results widget, ask the host tab to close */
	void CloseHostTab();

private:
	/** Called when the host tab is closed (if valid) */
	void HandleHostTabClosed(TSharedRef<SDockTab> DockTab);

	/** Called when user changes the text they are searching for */
	void HandleSearchTextChanged(const FText& Text);

	/** Called when user changes commits text to the search box */
	void HandleSearchTextCommitted(const FText& Text, ETextCommit::Type CommitType);

	/** Called when the user clicks the global find results button */
	FReply HandleOpenGlobalFindResults();

	/** Called when the find mode checkbox is hit */
	void HandleFindModeChanged(ECheckBoxState CheckState)
	{
		bIsInFindWithinQuestMode = CheckState == ECheckBoxState::Checked;
	}

	/** Called to check what the find mode is for the checkbox */
	ECheckBoxState HandleGetFindModeChecked() const
	{
		return bIsInFindWithinQuestMode ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	/* Get the children of a row */
	void HandleGetChildren(TSharedPtr<FEGQuestSearchResult> InItem, TArray<TSharedPtr<FEGQuestSearchResult>>& OutChildren);

	/* Called when user double clicks on a new result */
	void HandleTreeSelectionDoubleClicked(TSharedPtr<FEGQuestSearchResult> Item);

	/* Called when a new row is being generated */
	TSharedRef<ITableRow> HandleGenerateRow(TSharedPtr<FEGQuestSearchResult> InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Callback to build the context menu when right clicking in the tree */
	TSharedPtr<SWidget> HandleContextMenuOpening();

	/** Fills in the filter menu. */
	TSharedRef<SWidget> FillFilterEntries();

private:
	/** Pointer back to the Quest editor that owns us */
	TWeakPtr<FEGQuestEditor> QuestEditorPtr;

	/* The tree view displays the results */
	TSharedPtr<STreeView<TSharedPtr<FEGQuestSearchResult>>> TreeView;

	/** The search text box */
	TSharedPtr<SSearchBox> SearchTextBoxWidget;

	/** Vertical box, used to add and remove widgets dynamically */
	TWeakPtr<SVerticalBox> MainVerticalBoxWidget;

	/** In Find Within Quest mode, we need to keep a handle on the root result, because it won't show up in the tree. */
	TSharedPtr<FEGQuestSearchResult> RootSearchResult;

	/* This buffer stores the currently displayed results */
	TArray<TSharedPtr<FEGQuestSearchResult>> ItemsFound;

	/* The string to highlight in the results */
	FText HighlightText;

	/** The current searach filter */
	FEGQuestSearchFilter CurrentFilter;

	/** Should we search within the current Quest only (rather than all Quests) */
	bool bIsInFindWithinQuestMode;

	/** Tab hosting this widget. May be invalid. */
	TWeakPtr<SDockTab> HostTab;

	/** Commands handled by this widget */
	TSharedPtr<FUICommandList> CommandList;
};
