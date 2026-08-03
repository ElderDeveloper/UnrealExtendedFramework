// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianTypes.h"
#include "Widgets/SCompoundWidget.h"

class ITableRow;
class SEditableTextBox;
class STableViewBase;

template <typename ItemType> class STreeView;

/** A node in the documentation tree: either a space or a page. */
struct FExtendedAtlassianDocNode
{
	enum class EKind : uint8
	{
		Space,
		Page,
	};

	EKind Kind = EKind::Space;
	FString Id;
	FString Title;
	FString SpaceId;
	FString WebUrl;

	/** Set for repo Markdown files, which are read from disk rather than fetched. */
	FString LocalPath;

	/** Spaces load their pages lazily on first expansion. */
	bool bChildrenLoaded = false;
	bool bLoadingChildren = false;

	/**
	 * A non-selectable filler row.
	 *
	 * STreeView only draws an expander arrow for items that already report children, so a space with
	 * no children yet cannot be expanded — which makes the lazy load unreachable. An unloaded space
	 * carries one placeholder child purely so the arrow exists. Also used to say "no pages".
	 */
	bool bIsPlaceholder = false;

	/** A synthetic grouping row ("Personal Spaces"), not a real space. Never fetches anything. */
	bool bIsGroup = false;

	TArray<TSharedPtr<FExtendedAtlassianDocNode>> Children;
	TWeakPtr<FExtendedAtlassianDocNode> Parent;
};

/**
 * Docked tab for reading Confluence pages beside the level.
 *
 * Spaces expand lazily; page bodies are cached until explicitly reloaded, because pages are large
 * and mostly static. Every page keeps an "Open in Browser" button — the HTML-to-text conversion is
 * good for prose and poor for macro-heavy pages, and the escape hatch matters more than pretending
 * otherwise.
 */
class SExtendedAtlassianConfluenceBrowser : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExtendedAtlassianConfluenceBrowser) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SExtendedAtlassianConfluenceBrowser() override;

	/** Reloads the space list and clears cached page bodies. */
	void RefreshTree();

private:
	using FNodePtr = TSharedPtr<FExtendedAtlassianDocNode>;

	TSharedRef<SWidget> BuildToolbar();
	TSharedRef<SWidget> BuildTree();
	TSharedRef<SWidget> BuildContentPane();

	TSharedRef<ITableRow> OnGenerateNodeRow(FNodePtr Item, const TSharedRef<STableViewBase>& OwnerTable);
	void OnGetNodeChildren(FNodePtr Item, TArray<FNodePtr>& OutChildren);
	void OnNodeExpansionChanged(FNodePtr Item, bool bExpanded);
	void OnNodeSelectionChanged(FNodePtr Item, ESelectInfo::Type SelectInfo);

	void LoadPagesForSpace(FNodePtr SpaceNode);
	void LoadPageBody(FNodePtr PageNode, bool bBypassCache);
	void LoadLocalMarkdown(FNodePtr FileNode);
	void RunSearch();
	void ClearSearch();

	/** Builds the "Project Documents" root from .md files in the project tree. */
	void AddLocalDocumentsRoot();

	void ShowDocument(const TArray<FExtendedAtlassianDocBlock>& Blocks, const FString& SourceText);

	TSharedRef<SWidget> BuildEditActions();

	/** Fetches the page as storage format and opens it for editing, or refuses with a reason. */
	void BeginEdit();
	void EndEdit();

	/** Starts an unsaved page in the space of the current selection. */
	void BeginNewPage();

	void SaveWorkingCopy();
	void PushToConfluence();
	void ArchiveCurrentPage();
	void DeleteCurrentPage();

	/** The space key of whichever space contains the current selection. */
	FString FindSpaceKeyForSelection() const;

	void StartWatchingDocuments();
	void StopWatchingDocuments();
	void HandleDocumentsChanged(const TArray<struct FFileChangeData>& Changes);

	/** Writes every page of the selected space to Saved/Documents as Markdown. */
	void PullSpaceToDisk();
	void PullNextPage(TSharedRef<struct FExtendedAtlassianSpacePull> Pull);

	/** Finds Jira keys in the open document and fetches their current status. */
	void RefreshReferencedIssues(const FString& SourceText);

	bool bPulling = false;

	TArray<TSharedPtr<FExtendedAtlassianIssue>> ReferencedIssues;
	TSharedPtr<class SWrapBox> IssueChipsBox;

	/** Turns free text into CQL, or passes through what already looks like CQL. */
	FString BuildCqlFromInput(const FString& Input) const;

	FText GetBreadcrumbText() const;

	/** Loads the space list as soon as credentials become usable, so connecting fills the tab. */
	void HandleAuthStateChanged();

	void SetStatus(const FText& Message, bool bIsError);

	FDelegateHandle AuthChangedHandle;

	TArray<FNodePtr> RootNodes;
	TSharedPtr<STreeView<FNodePtr>> TreeView;
	FNodePtr SelectedNode;

	/** Populated while showing search results, so the space tree can be restored. */
	TArray<FNodePtr> SavedRootNodes;
	bool bShowingSearchResults = false;

	TSharedPtr<SEditableTextBox> SearchBox;

	/** Page id to converted body. Pages are large and rarely change mid-session. */
	TMap<FString, FString> PageBodyCache;
	TMap<FString, TArray<FExtendedAtlassianDocBlock>> PageBlockCache;
	FString CurrentBody;

	TSharedPtr<class SExtendedAtlassianDocumentView> DocumentView;
	TSharedPtr<class SExtendedAtlassianDocumentEditor> DocumentEditor;

	/** False shows the raw text instead, which is selectable and copyable. */
	bool bShowRendered = true;

	/** Edit mode replaces both view modes with the split editor. */
	bool bEditMode = false;

	/** Identity and base version of the page being edited; empty Id means an unsaved new page. */
	FExtendedAtlassianPage EditingPage;
	FString EditingSpaceKey;
	FString EditingTitle;

	/** Working copy on disk, when one has been written. */
	FString CurrentFilePath;

	bool bSaving = false;

	FDelegateHandle DirectoryWatcherHandle;

	bool bLoadingTree = false;
	bool bLoadingPage = false;

	FText StatusMessage;
	bool bStatusIsError = false;
};
