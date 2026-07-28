// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianConfluenceBrowser.h"

#include "ExtendedAtlassianClient.h"
#include "ExtendedAtlassianConfluence.h"
#include "ExtendedAtlassianMarkdown.h"
#include "ExtendedAtlassianSettings.h"
#include "SExtendedAtlassianDocumentView.h"
#include "UnrealExtendedAtlassian.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"

#define LOCTEXT_NAMESPACE "ExtendedAtlassianConfluenceBrowser"

namespace ExtendedAtlassianConfluenceBrowserPrivate
{
	/** Guards the breadcrumb walk against a malformed parent chain. */
	constexpr int32 MaxBreadcrumbDepth = 32;

	/** A filler child that exists only so the tree draws an expander arrow, or explains an empty space. */
	TSharedPtr<FExtendedAtlassianDocNode> MakePlaceholder(const FString& Label, const TSharedPtr<FExtendedAtlassianDocNode>& Parent)
	{
		TSharedPtr<FExtendedAtlassianDocNode> Node = MakeShared<FExtendedAtlassianDocNode>();
		Node->Kind = FExtendedAtlassianDocNode::EKind::Page;
		Node->Title = Label;
		Node->bIsPlaceholder = true;
		Node->bChildrenLoaded = true;
		Node->Parent = Parent;
		return Node;
	}

	void SortNodesRecursive(TArray<TSharedPtr<FExtendedAtlassianDocNode>>& Nodes)
	{
		Nodes.Sort([](const TSharedPtr<FExtendedAtlassianDocNode>& A, const TSharedPtr<FExtendedAtlassianDocNode>& B)
		{
			if (!A.IsValid() || !B.IsValid())
			{
				return false;
			}
			return A->Title.Compare(B->Title, ESearchCase::IgnoreCase) < 0;
		});

		for (const TSharedPtr<FExtendedAtlassianDocNode>& Node : Nodes)
		{
			if (Node.IsValid())
			{
				SortNodesRecursive(Node->Children);
			}
		}
	}

	/** True when the input already reads as CQL rather than free text. */
	bool LooksLikeCql(const FString& Input)
	{
		static const TCHAR* Markers[] = { TEXT("~"), TEXT("="), TEXT(" and "), TEXT(" or "), TEXT("order by") };

		const FString Lowered = Input.ToLower();
		for (const TCHAR* Marker : Markers)
		{
			if (Lowered.Contains(Marker))
			{
				return true;
			}
		}
		return false;
	}
}

void SExtendedAtlassianConfluenceBrowser::Construct(const FArguments& InArgs)
{
	// No connect prompt here, unlike the issue browser: this tab also reads the project's own
	// Markdown files, so it is useful before any Atlassian credentials exist.
	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.0f, 6.0f, 6.0f, 2.0f)
		[
			BuildToolbar()
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(6.0f, 2.0f, 6.0f, 6.0f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)

			+ SSplitter::Slot()
			.Value(0.35f)
			[
				BuildTree()
			]

			+ SSplitter::Slot()
			.Value(0.65f)
			[
				BuildContentPane()
			]
		]
	];

	if (const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient())
	{
		AuthChangedHandle = Client->OnAuthStateChanged().AddSP(this, &SExtendedAtlassianConfluenceBrowser::HandleAuthStateChanged);
	}

	RefreshTree();
}

SExtendedAtlassianConfluenceBrowser::~SExtendedAtlassianConfluenceBrowser()
{
	if (const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient())
	{
		Client->OnAuthStateChanged().Remove(AuthChangedHandle);
	}
}

void SExtendedAtlassianConfluenceBrowser::HandleAuthStateChanged()
{
	if (RootNodes.Num() == 0 && !bLoadingTree)
	{
		RefreshTree();
	}
}

TSharedRef<SWidget> SExtendedAtlassianConfluenceBrowser::BuildToolbar()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(SearchBox, SEditableTextBox)
				.HintText(LOCTEXT("SearchHint", "Search pages, or type CQL directly"))
				.OnTextCommitted_Lambda([this](const FText&, ETextCommit::Type CommitType)
				{
					if (CommitType == ETextCommit::OnEnter)
					{
						RunSearch();
					}
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(6.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("SearchButton", "Search"))
				.OnClicked_Lambda([this]()
				{
					RunSearch();
					return FReply::Handled();
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(6.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("ClearSearchButton", "Clear"))
				.Visibility_Lambda([this]() { return bShowingSearchResults ? EVisibility::Visible : EVisibility::Collapsed; })
				.OnClicked_Lambda([this]()
				{
					ClearSearch();
					return FReply::Handled();
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(6.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("RefreshTreeButton", "Refresh"))
				.ToolTipText(LOCTEXT("RefreshTreeTooltip", "Reload the space list and discard cached page bodies."))
				.IsEnabled_Lambda([this]() { return !bLoadingTree; })
				.OnClicked_Lambda([this]()
				{
					RefreshTree();
					return FReply::Handled();
				})
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 4.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SThrobber)
				.Visibility_Lambda([this]() { return (bLoadingTree || bLoadingPage) ? EVisibility::Visible : EVisibility::Collapsed; })
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda([this]() { return StatusMessage; })
				.ColorAndOpacity_Lambda([this]()
				{
					return FSlateColor(bStatusIsError
						? FLinearColor(0.90f, 0.35f, 0.30f)
						: FLinearColor(0.70f, 0.70f, 0.70f));
				})
				.AutoWrapText(true)
			]
		];
}

TSharedRef<SWidget> SExtendedAtlassianConfluenceBrowser::BuildTree()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
		[
			SAssignNew(TreeView, STreeView<FNodePtr>)
			.TreeItemsSource(&RootNodes)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &SExtendedAtlassianConfluenceBrowser::OnGenerateNodeRow)
			.OnGetChildren(this, &SExtendedAtlassianConfluenceBrowser::OnGetNodeChildren)
			.OnExpansionChanged(this, &SExtendedAtlassianConfluenceBrowser::OnNodeExpansionChanged)
			.OnSelectionChanged(this, &SExtendedAtlassianConfluenceBrowser::OnNodeSelectionChanged)
		];
}

TSharedRef<SWidget> SExtendedAtlassianConfluenceBrowser::BuildContentPane()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
		.Padding(8.0f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle(TEXT("BoldFont")))
				.AutoWrapText(true)
				.Text_Lambda([this]()
				{
					return SelectedNode.IsValid()
						? FText::FromString(SelectedNode->Title)
						: LOCTEXT("NoPageSelected", "Select a page");
				})
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 2.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(this, &SExtendedAtlassianConfluenceBrowser::GetBreadcrumbText)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f)))
				.AutoWrapText(true)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 6.0f)
			[
				SNew(SHorizontalBox)
				.Visibility_Lambda([this]()
				{
					const bool bRealPage = SelectedNode.IsValid()
						&& SelectedNode->Kind == FExtendedAtlassianDocNode::EKind::Page
						&& !SelectedNode->bIsPlaceholder
						&& !SelectedNode->Id.IsEmpty();

					return bRealPage ? EVisibility::Visible : EVisibility::Collapsed;
				})

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("OpenPageInBrowser", "Open in Browser"))
					.ToolTipText(LOCTEXT("OpenPageTooltip", "Macro-heavy pages render better in a real browser."))
					.OnClicked_Lambda([this]()
					{
						if (SelectedNode.IsValid() && !SelectedNode->WebUrl.IsEmpty())
						{
							FPlatformProcess::LaunchURL(*SelectedNode->WebUrl, nullptr, nullptr);
						}
						return FReply::Handled();
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("ReloadPage", "Reload Page"))
					.ToolTipText(LOCTEXT("ReloadPageTooltip", "Discard the cached copy and fetch this page again."))
					.IsEnabled_Lambda([this]() { return !bLoadingPage; })
					.OnClicked_Lambda([this]()
					{
						if (SelectedNode.IsValid())
						{
							LoadPageBody(SelectedNode, true);
						}
						return FReply::Handled();
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text_Lambda([this]()
					{
						return bShowRendered
							? LOCTEXT("ShowSource", "View Source")
							: LOCTEXT("ShowRendered", "View Rendered");
					})
					.ToolTipText(LOCTEXT("ToggleViewTooltip", "Switch between the formatted document and its raw text, which can be selected and copied."))
					.OnClicked_Lambda([this]()
					{
						bShowRendered = !bShowRendered;
						return FReply::Handled();
					})
				]
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				// Rendered by default; the source view is the escape hatch when conversion gets
				// something wrong, and the only place text can be selected and copied.
				SNew(SWidgetSwitcher)
				.WidgetIndex_Lambda([this]() { return bShowRendered ? 0 : 1; })

				+ SWidgetSwitcher::Slot()
				[
					SAssignNew(DocumentView, SExtendedAtlassianDocumentView)
				]

				+ SWidgetSwitcher::Slot()
				[
					SNew(SMultiLineEditableTextBox)
					.IsReadOnly(true)
					.AllowMultiLine(true)
					.AutoWrapText(true)
					.Text_Lambda([this]() { return FText::FromString(CurrentBody); })
				]
			]
		];
}

void SExtendedAtlassianConfluenceBrowser::RefreshTree()
{
	if (bLoadingTree)
	{
		return;
	}

	const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient();
	if (!Client.IsValid() || !Client->IsReady())
	{
		// Repo documents still work without Atlassian, so show those rather than an empty tree.
		RootNodes.Reset();
		AddLocalDocumentsRoot();

		if (TreeView.IsValid())
		{
			TreeView->RequestTreeRefresh();
		}

		SetStatus(RootNodes.Num() > 0
			? LOCTEXT("NotConnectedLocalOnly", "Not connected to Atlassian - showing project documents only. Add credentials in Project Settings.")
			: LOCTEXT("NotConnected", "Not connected. Set the site URL and credentials in Project Settings > Extended Framework > Extended Atlassian."), true);
		return;
	}

	bLoadingTree = true;
	bShowingSearchResults = false;
	PageBodyCache.Reset();
	CurrentBody.Reset();
	SelectedNode.Reset();
	SetStatus(LOCTEXT("LoadingSpaces", "Loading spaces..."), false);

	TWeakPtr<SExtendedAtlassianConfluenceBrowser> WeakBrowser = SharedThis(this);

	FExtendedAtlassianConfluence::ListSpaces(FExtendedAtlassianSpacesDelegate::CreateLambda(
		[WeakBrowser](bool bSuccess, const TArray<FExtendedAtlassianSpace>& Spaces, const FExtendedAtlassianError& Error)
		{
			const TSharedPtr<SExtendedAtlassianConfluenceBrowser> Browser = WeakBrowser.Pin();
			if (!Browser.IsValid())
			{
				return;
			}

			Browser->bLoadingTree = false;
			Browser->RootNodes.Reset();

			if (!bSuccess)
			{
				Browser->SetStatus(FText::FromString(Error.Message), true);
				if (Browser->TreeView.IsValid())
				{
					Browser->TreeView->RequestTreeRefresh();
				}
				return;
			}

			// Personal spaces are kept, but behind one collapsed group. There is typically one per
			// teammate, so listing them inline buries the handful of spaces that hold real docs.
			TSharedPtr<FExtendedAtlassianDocNode> PersonalGroup;

			for (const FExtendedAtlassianSpace& Space : Spaces)
			{
				TSharedPtr<FExtendedAtlassianDocNode> Node = MakeShared<FExtendedAtlassianDocNode>();
				Node->Kind = FExtendedAtlassianDocNode::EKind::Space;
				Node->Id = Space.Id;
				Node->SpaceId = Space.Id;

				// A personal space key is just the owner's account id; showing it is pure noise.
				Node->Title = Space.IsPersonal()
					? (Space.Name.IsEmpty() ? Space.Key : Space.Name)
					: (Space.Name.IsEmpty() ? Space.Key : FString::Printf(TEXT("%s (%s)"), *Space.Name, *Space.Key));

				// Without a child there is no expander arrow, and the lazy page load can never fire.
				Node->Children.Add(ExtendedAtlassianConfluenceBrowserPrivate::MakePlaceholder(TEXT("Loading..."), Node));

				if (!Space.IsPersonal())
				{
					Browser->RootNodes.Add(Node);
					continue;
				}

				if (!PersonalGroup.IsValid())
				{
					PersonalGroup = MakeShared<FExtendedAtlassianDocNode>();
					PersonalGroup->Kind = FExtendedAtlassianDocNode::EKind::Space;
					PersonalGroup->bChildrenLoaded = true; // Synthetic: never fetches pages of its own.
					PersonalGroup->bIsGroup = true;
				}

				Node->Parent = PersonalGroup;
				PersonalGroup->Children.Add(Node);
			}

			ExtendedAtlassianConfluenceBrowserPrivate::SortNodesRecursive(Browser->RootNodes);

			if (PersonalGroup.IsValid())
			{
				ExtendedAtlassianConfluenceBrowserPrivate::SortNodesRecursive(PersonalGroup->Children);
				PersonalGroup->Title = FString::Printf(TEXT("Personal Spaces (%d)"), PersonalGroup->Children.Num());

				// Always last, so real documentation spaces stay at the top.
				Browser->RootNodes.Add(PersonalGroup);
			}

			Browser->AddLocalDocumentsRoot();

			if (Browser->TreeView.IsValid())
			{
				Browser->TreeView->RequestTreeRefresh();

				// With a single real space there is nothing to choose; open it rather than making
				// the user click into the only option. The Personal Spaces group does not count.
				TArray<FNodePtr> RealSpaces = Browser->RootNodes.FilterByPredicate(
					[](const FNodePtr& Node) { return Node.IsValid() && !Node->bIsGroup; });

				if (RealSpaces.Num() == 1)
				{
					Browser->TreeView->SetItemExpansion(RealSpaces[0], true);
				}
			}

			Browser->SetStatus(Browser->RootNodes.Num() == 0
				? LOCTEXT("NoSpaces", "No spaces are visible to this account.")
				: FText::Format(LOCTEXT("SpaceCount", "{0} space(s)."), FText::AsNumber(Browser->RootNodes.Num())), false);
		}));
}

void SExtendedAtlassianConfluenceBrowser::OnGetNodeChildren(FNodePtr Item, TArray<FNodePtr>& OutChildren)
{
	if (Item.IsValid())
	{
		OutChildren = Item->Children;
	}
}

void SExtendedAtlassianConfluenceBrowser::OnNodeExpansionChanged(FNodePtr Item, bool bExpanded)
{
	if (!Item.IsValid() || !bExpanded)
	{
		return;
	}

	if (Item->Kind == FExtendedAtlassianDocNode::EKind::Space && !Item->bChildrenLoaded && !Item->bLoadingChildren)
	{
		LoadPagesForSpace(Item);
	}
}

void SExtendedAtlassianConfluenceBrowser::LoadPagesForSpace(FNodePtr SpaceNode)
{
	if (!SpaceNode.IsValid())
	{
		return;
	}

	SpaceNode->bLoadingChildren = true;
	bLoadingTree = true;
	SetStatus(LOCTEXT("LoadingPages", "Loading pages..."), false);

	TWeakPtr<SExtendedAtlassianConfluenceBrowser> WeakBrowser = SharedThis(this);
	TWeakPtr<FExtendedAtlassianDocNode> WeakSpace = SpaceNode;

	FExtendedAtlassianConfluence::ListPages(SpaceNode->Id, FExtendedAtlassianPagesDelegate::CreateLambda(
		[WeakBrowser, WeakSpace](bool bSuccess, const TArray<FExtendedAtlassianPage>& Pages, const FExtendedAtlassianError& Error)
		{
			const TSharedPtr<SExtendedAtlassianConfluenceBrowser> Browser = WeakBrowser.Pin();
			const TSharedPtr<FExtendedAtlassianDocNode> Space = WeakSpace.Pin();

			if (!Browser.IsValid() || !Space.IsValid())
			{
				return;
			}

			Browser->bLoadingTree = false;
			Space->bLoadingChildren = false;

			if (!bSuccess)
			{
				Browser->SetStatus(FText::FromString(Error.Message), true);
				return;
			}

			// Build the hierarchy from ParentId; anything whose parent is outside this space
			// becomes a direct child of the space node rather than being dropped.
			TMap<FString, TSharedPtr<FExtendedAtlassianDocNode>> NodesById;
			NodesById.Reserve(Pages.Num());

			for (const FExtendedAtlassianPage& Page : Pages)
			{
				TSharedPtr<FExtendedAtlassianDocNode> Node = MakeShared<FExtendedAtlassianDocNode>();
				Node->Kind = FExtendedAtlassianDocNode::EKind::Page;
				Node->Id = Page.Id;
				Node->Title = Page.Title;
				Node->SpaceId = Page.SpaceId;
				Node->WebUrl = Page.WebUrl;
				NodesById.Add(Page.Id, Node);
			}

			Space->Children.Reset();

			for (const FExtendedAtlassianPage& Page : Pages)
			{
				const TSharedPtr<FExtendedAtlassianDocNode> Node = NodesById.FindRef(Page.Id);
				if (!Node.IsValid())
				{
					continue;
				}

				const TSharedPtr<FExtendedAtlassianDocNode> Parent =
					Page.ParentId.IsEmpty() ? nullptr : NodesById.FindRef(Page.ParentId);

				if (Parent.IsValid())
				{
					Node->Parent = Parent;
					Parent->Children.Add(Node);
				}
				else
				{
					Node->Parent = Space;
					Space->Children.Add(Node);
				}
			}

			Space->bChildrenLoaded = true;
			ExtendedAtlassianConfluenceBrowserPrivate::SortNodesRecursive(Space->Children);

			// Say so, rather than collapsing back to an arrow-less row that looks broken.
			if (Space->Children.Num() == 0)
			{
				Space->Children.Add(ExtendedAtlassianConfluenceBrowserPrivate::MakePlaceholder(TEXT("(no pages)"), Space));
			}

			if (Browser->TreeView.IsValid())
			{
				Browser->TreeView->RequestTreeRefresh();
			}

			Browser->SetStatus(FText::Format(
				LOCTEXT("PageCount", "{0} page(s) in {1}."),
				FText::AsNumber(Pages.Num()),
				FText::FromString(Space->Title)), false);
		}));
}

void SExtendedAtlassianConfluenceBrowser::OnNodeSelectionChanged(FNodePtr Item, ESelectInfo::Type SelectInfo)
{
	SelectedNode = Item;
	CurrentBody.Reset();

	if (DocumentView.IsValid())
	{
		DocumentView->Clear();
	}

	if (!Item.IsValid() || Item->Kind != FExtendedAtlassianDocNode::EKind::Page)
	{
		return;
	}

	if (!Item->LocalPath.IsEmpty())
	{
		LoadLocalMarkdown(Item);
		return;
	}

	LoadPageBody(Item, false);
}

void SExtendedAtlassianConfluenceBrowser::ShowDocument(const TArray<FExtendedAtlassianDocBlock>& Blocks, const FString& SourceText)
{
	CurrentBody = SourceText;

	if (DocumentView.IsValid())
	{
		DocumentView->SetBlocks(Blocks);
	}
}

void SExtendedAtlassianConfluenceBrowser::LoadLocalMarkdown(FNodePtr FileNode)
{
	if (!FileNode.IsValid() || FileNode->LocalPath.IsEmpty())
	{
		return;
	}

	FString Source;
	if (!FFileHelper::LoadFileToString(Source, *FileNode->LocalPath))
	{
		SetStatus(FText::Format(
			LOCTEXT("MarkdownReadFailed", "Could not read {0}."),
			FText::FromString(FileNode->LocalPath)), true);
		return;
	}

	ShowDocument(FExtendedAtlassianMarkdown::ToBlocks(Source), Source);
	SetStatus(FText::FromString(FileNode->LocalPath), false);
}

void SExtendedAtlassianConfluenceBrowser::AddLocalDocumentsRoot()
{
	using namespace ExtendedAtlassianConfluenceBrowserPrivate;

	const FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());

	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *ProjectDir, TEXT("*.md"), true, false);

	// Generated and vendored trees produce hundreds of irrelevant files.
	static const TCHAR* ExcludedFolders[] = {
		TEXT("/Intermediate/"), TEXT("/Saved/"), TEXT("/Binaries/"),
		TEXT("/DerivedDataCache/"), TEXT("/.git/"), TEXT("/ThirdParty/"), TEXT("/node_modules/"),
	};

	TSharedPtr<FExtendedAtlassianDocNode> Group;

	for (const FString& File : Files)
	{
		FString Normalized = File;
		Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));

		bool bExcluded = false;
		for (const TCHAR* Folder : ExcludedFolders)
		{
			if (Normalized.Contains(Folder))
			{
				bExcluded = true;
				break;
			}
		}

		if (bExcluded)
		{
			continue;
		}

		if (!Group.IsValid())
		{
			Group = MakeShared<FExtendedAtlassianDocNode>();
			Group->Kind = FExtendedAtlassianDocNode::EKind::Space;
			Group->bChildrenLoaded = true;
			Group->bIsGroup = true;
		}

		TSharedPtr<FExtendedAtlassianDocNode> Node = MakeShared<FExtendedAtlassianDocNode>();
		Node->Kind = FExtendedAtlassianDocNode::EKind::Page;
		Node->LocalPath = File;
		Node->Title = Normalized.RightChop(ProjectDir.Len());
		Node->bChildrenLoaded = true;
		Node->Parent = Group;

		Group->Children.Add(Node);
	}

	if (!Group.IsValid())
	{
		return;
	}

	SortNodesRecursive(Group->Children);
	Group->Title = FString::Printf(TEXT("Project Documents (%d)"), Group->Children.Num());
	RootNodes.Add(Group);
}

void SExtendedAtlassianConfluenceBrowser::LoadPageBody(FNodePtr PageNode, bool bBypassCache)
{
	if (!PageNode.IsValid() || PageNode->Kind != FExtendedAtlassianDocNode::EKind::Page)
	{
		return;
	}

	// "Loading..." and "(no pages)" rows are furniture, not pages.
	if (PageNode->bIsPlaceholder || PageNode->Id.IsEmpty())
	{
		return;
	}

	if (!bBypassCache)
	{
		const FString* CachedBody = PageBodyCache.Find(PageNode->Id);
		const TArray<FExtendedAtlassianDocBlock>* CachedBlocks = PageBlockCache.Find(PageNode->Id);

		if (CachedBody && CachedBlocks)
		{
			ShowDocument(*CachedBlocks, *CachedBody);
			SetStatus(LOCTEXT("LoadedFromCache", "Showing the cached copy. Use Reload Page to fetch again."), false);
			return;
		}
	}
	else
	{
		PageBodyCache.Remove(PageNode->Id);
		PageBlockCache.Remove(PageNode->Id);
	}

	bLoadingPage = true;
	SetStatus(LOCTEXT("LoadingPage", "Loading page..."), false);

	const FString PageId = PageNode->Id;
	TWeakPtr<SExtendedAtlassianConfluenceBrowser> WeakBrowser = SharedThis(this);

	FExtendedAtlassianConfluence::GetPage(PageId, FExtendedAtlassianPageDelegate::CreateLambda(
		[WeakBrowser, PageId](bool bSuccess, const FExtendedAtlassianPage& Page, const FExtendedAtlassianError& Error)
		{
			const TSharedPtr<SExtendedAtlassianConfluenceBrowser> Browser = WeakBrowser.Pin();
			if (!Browser.IsValid())
			{
				return;
			}

			Browser->bLoadingPage = false;

			if (!bSuccess)
			{
				Browser->SetStatus(FText::FromString(Error.Message), true);
				return;
			}

			Browser->PageBodyCache.Add(PageId, Page.Body);
			Browser->PageBlockCache.Add(PageId, Page.Blocks);

			// The selection may have moved on while this was in flight.
			if (Browser->SelectedNode.IsValid() && Browser->SelectedNode->Id == PageId)
			{
				Browser->ShowDocument(Page.Blocks, Page.Body);

				// A page whose body converts to nothing is almost always macro-only; say so rather
				// than showing a blank pane that reads like a failure.
				Browser->SetStatus(Page.Blocks.Num() == 0
					? LOCTEXT("EmptyBody", "This page has no text content that converts cleanly. Use Open in Browser.")
					: FText::GetEmpty(), false);
			}
		}));
}

FString SExtendedAtlassianConfluenceBrowser::BuildCqlFromInput(const FString& Input) const
{
	using namespace ExtendedAtlassianConfluenceBrowserPrivate;

	const FString Trimmed = Input.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		return FString();
	}

	if (LooksLikeCql(Trimmed))
	{
		return Trimmed;
	}

	// Escape quotes so a stray one cannot break the query.
	FString Escaped = Trimmed;
	Escaped.ReplaceInline(TEXT("\""), TEXT("\\\""));

	FString Cql = FString::Printf(TEXT("type = page AND text ~ \"%s\""), *Escaped);

	if (const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get())
	{
		TArray<FString> Keys;
		for (const FString& Key : Settings->SpaceKeys)
		{
			const FString KeyTrimmed = Key.TrimStartAndEnd();
			if (!KeyTrimmed.IsEmpty())
			{
				Keys.Add(KeyTrimmed);
			}
		}

		if (Keys.Num() > 0)
		{
			Cql += FString::Printf(TEXT(" AND space IN (%s)"), *FString::Join(Keys, TEXT(",")));
		}
	}

	return Cql;
}

void SExtendedAtlassianConfluenceBrowser::RunSearch()
{
	if (!SearchBox.IsValid())
	{
		return;
	}

	const FString Cql = BuildCqlFromInput(SearchBox->GetText().ToString());
	if (Cql.IsEmpty())
	{
		ClearSearch();
		return;
	}

	// Keep the space tree so Clear can put it back without another round trip.
	if (!bShowingSearchResults)
	{
		SavedRootNodes = RootNodes;
	}

	bLoadingTree = true;
	SetStatus(LOCTEXT("Searching", "Searching..."), false);

	TWeakPtr<SExtendedAtlassianConfluenceBrowser> WeakBrowser = SharedThis(this);

	FExtendedAtlassianConfluence::Search(Cql, FExtendedAtlassianPagesDelegate::CreateLambda(
		[WeakBrowser](bool bSuccess, const TArray<FExtendedAtlassianPage>& Pages, const FExtendedAtlassianError& Error)
		{
			const TSharedPtr<SExtendedAtlassianConfluenceBrowser> Browser = WeakBrowser.Pin();
			if (!Browser.IsValid())
			{
				return;
			}

			Browser->bLoadingTree = false;

			if (!bSuccess)
			{
				Browser->SetStatus(FText::FromString(Error.Message), true);
				return;
			}

			Browser->bShowingSearchResults = true;
			Browser->RootNodes.Reset();

			for (const FExtendedAtlassianPage& Page : Pages)
			{
				TSharedPtr<FExtendedAtlassianDocNode> Node = MakeShared<FExtendedAtlassianDocNode>();
				Node->Kind = FExtendedAtlassianDocNode::EKind::Page;
				Node->Id = Page.Id;
				Node->Title = Page.Title;
				Node->WebUrl = Page.WebUrl;
				Node->bChildrenLoaded = true;
				Browser->RootNodes.Add(Node);
			}

			if (Browser->TreeView.IsValid())
			{
				Browser->TreeView->RequestTreeRefresh();
			}

			Browser->SetStatus(Pages.Num() == 0
				? LOCTEXT("NoSearchResults", "No pages matched.")
				: FText::Format(LOCTEXT("SearchResultCount", "{0} result(s)."), FText::AsNumber(Pages.Num())), false);
		}));
}

void SExtendedAtlassianConfluenceBrowser::ClearSearch()
{
	if (!bShowingSearchResults)
	{
		return;
	}

	bShowingSearchResults = false;
	RootNodes = SavedRootNodes;
	SavedRootNodes.Reset();

	if (SearchBox.IsValid())
	{
		SearchBox->SetText(FText::GetEmpty());
	}

	if (TreeView.IsValid())
	{
		TreeView->RequestTreeRefresh();
	}

	SetStatus(FText::GetEmpty(), false);
}

TSharedRef<ITableRow> SExtendedAtlassianConfluenceBrowser::OnGenerateNodeRow(FNodePtr Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	const bool bIsSpace = Item.IsValid() && Item->Kind == FExtendedAtlassianDocNode::EKind::Space;
	const bool bIsPlaceholder = Item.IsValid() && Item->bIsPlaceholder;

	return SNew(STableRow<FNodePtr>, OwnerTable)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Item.IsValid() ? Item->Title : FString()))
			.ToolTipText(FText::FromString(Item.IsValid() ? Item->Title : FString()))
			.Margin(FMargin(2.0f))
			.ColorAndOpacity(bIsPlaceholder
				? FSlateColor(FLinearColor(0.55f, 0.55f, 0.55f))
				: FSlateColor::UseForeground())
			.Font(bIsSpace
				? FAppStyle::GetFontStyle(TEXT("BoldFont"))
				: FAppStyle::GetFontStyle(TEXT("NormalFont")))
		];
}

FText SExtendedAtlassianConfluenceBrowser::GetBreadcrumbText() const
{
	using namespace ExtendedAtlassianConfluenceBrowserPrivate;

	if (!SelectedNode.IsValid())
	{
		return FText::GetEmpty();
	}

	TArray<FString> Parts;

	TSharedPtr<FExtendedAtlassianDocNode> Node = SelectedNode->Parent.Pin();
	int32 Depth = 0;
	while (Node.IsValid() && Depth++ < MaxBreadcrumbDepth)
	{
		Parts.Insert(Node->Title, 0);
		Node = Node->Parent.Pin();
	}

	return Parts.Num() == 0 ? FText::GetEmpty() : FText::FromString(FString::Join(Parts, TEXT(" / ")));
}

void SExtendedAtlassianConfluenceBrowser::SetStatus(const FText& Message, bool bIsError)
{
	StatusMessage = Message;
	bStatusIsError = bIsError;
}

#undef LOCTEXT_NAMESPACE
