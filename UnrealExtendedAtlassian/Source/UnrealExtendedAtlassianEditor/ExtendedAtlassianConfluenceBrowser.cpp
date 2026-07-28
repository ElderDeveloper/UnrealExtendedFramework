// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianConfluenceBrowser.h"

#include "ExtendedAtlassianClient.h"
#include "ExtendedAtlassianConfluence.h"
#include "ExtendedAtlassianDocumentStore.h"
#include "ExtendedAtlassianJira.h"
#include "ExtendedAtlassianLog.h"
#include "ExtendedAtlassianMarkdown.h"
#include "ExtendedAtlassianSettings.h"
#include "SExtendedAtlassianDocumentEditor.h"
#include "SExtendedAtlassianDocumentView.h"
#include "UnrealExtendedAtlassian.h"

#include "DirectoryWatcherModule.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "IDirectoryWatcher.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
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

	StartWatchingDocuments();
	RefreshTree();
}

SExtendedAtlassianConfluenceBrowser::~SExtendedAtlassianConfluenceBrowser()
{
	StopWatchingDocuments();

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

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(6.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("PullSpace", "Pull Space to Disk"))
				.ToolTipText(LOCTEXT("PullSpaceTip", "Write every page of this space to Saved/Documents as Markdown, so the whole space is readable and greppable offline."))
				.IsEnabled_Lambda([this]() { return !bPulling && !bLoadingTree; })
				.OnClicked_Lambda([this]()
				{
					PullSpaceToDisk();
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
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("CopyMarkdown", "Copy"))
					.ToolTipText(LOCTEXT("CopyMarkdownTip", "Copy the whole document as Markdown. The rendered view cannot be selected; use this or View Source to take text out."))
					.OnClicked_Lambda([this]()
					{
						if (!CurrentBody.IsEmpty())
						{
							FPlatformApplicationMisc::ClipboardCopy(*CurrentBody);
							SetStatus(LOCTEXT("Copied", "Copied the document to the clipboard as Markdown."), false);
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

			// Jira epics referenced by the document, with live status. Your pages already end with
			// an "Ilgili Jira Epic" section, so this makes an existing convention actionable.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 6.0f)
			[
				SAssignNew(IssueChipsBox, SWrapBox)
				.UseAllottedSize(true)
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				// Rendered by default; the source view is the escape hatch when conversion gets
				// something wrong, and the only place text can be selected and copied.
				SNew(SWidgetSwitcher)
				.WidgetIndex_Lambda([this]() { return bEditMode ? 2 : (bShowRendered ? 0 : 1); })

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

				+ SWidgetSwitcher::Slot()
				[
					SAssignNew(DocumentEditor, SExtendedAtlassianDocumentEditor)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				BuildEditActions()
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

TSharedRef<SWidget> SExtendedAtlassianConfluenceBrowser::BuildEditActions()
{
	auto IsRealConfluencePage = [this]()
	{
		return SelectedNode.IsValid()
			&& SelectedNode->Kind == FExtendedAtlassianDocNode::EKind::Page
			&& !SelectedNode->bIsPlaceholder
			&& SelectedNode->LocalPath.IsEmpty()
			&& !SelectedNode->Id.IsEmpty();
	};

	return SNew(SVerticalBox)

		// Title, editable only while composing.
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([this]() { return bEditMode ? EVisibility::Visible : EVisibility::Collapsed; })

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(STextBlock).Text(LOCTEXT("TitleLabel", "Title"))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SEditableTextBox)
				.Text_Lambda([this]() { return FText::FromString(EditingTitle); })
				.OnTextChanged_Lambda([this](const FText& NewText) { EditingTitle = NewText.ToString(); })
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			// --- View mode ---
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("EditPage", "Edit"))
				.ToolTipText(LOCTEXT("EditPageTip", "Fetch the page source and open it for editing."))
				.Visibility_Lambda([this]() { return bEditMode ? EVisibility::Collapsed : EVisibility::Visible; })
				.IsEnabled_Lambda([IsRealConfluencePage]() { return IsRealConfluencePage(); })
				.OnClicked_Lambda([this]() { BeginEdit(); return FReply::Handled(); })
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("NewPage", "New Page"))
				.ToolTipText(LOCTEXT("NewPageTip", "Compose a new page in the selected space."))
				.Visibility_Lambda([this]() { return bEditMode ? EVisibility::Collapsed : EVisibility::Visible; })
				.OnClicked_Lambda([this]() { BeginNewPage(); return FReply::Handled(); })
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("DeletePage", "Delete Page"))
				.ToolTipText(LOCTEXT("DeletePageTip", "Move this page to the Confluence trash."))
				.Visibility_Lambda([this]() { return bEditMode ? EVisibility::Collapsed : EVisibility::Visible; })
				.IsEnabled_Lambda([IsRealConfluencePage]() { return IsRealConfluencePage(); })
				.OnClicked_Lambda([this]() { DeleteCurrentPage(); return FReply::Handled(); })
			]

			// --- Edit mode ---
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("SaveToFile", "Save to File"))
				.ToolTipText(LOCTEXT("SaveToFileTip", "Write the working copy to Saved/Documents, where other tools can read it."))
				.Visibility_Lambda([this]() { return bEditMode ? EVisibility::Visible : EVisibility::Collapsed; })
				.OnClicked_Lambda([this]() { SaveWorkingCopy(); return FReply::Handled(); })
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Push", "Push to Confluence"))
				.ToolTipText(LOCTEXT("PushTip", "Publish this document. Always explicit - nothing is pushed automatically."))
				.Visibility_Lambda([this]() { return bEditMode ? EVisibility::Visible : EVisibility::Collapsed; })
				.IsEnabled_Lambda([this]() { return !bSaving && EditingPage.bCanRoundTrip; })
				.OnClicked_Lambda([this]() { PushToConfluence(); return FReply::Handled(); })
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("DoneEditing", "Close Editor"))
				.Visibility_Lambda([this]() { return bEditMode ? EVisibility::Visible : EVisibility::Collapsed; })
				.OnClicked_Lambda([this]() { EndEdit(); return FReply::Handled(); })
			]
		];
}

FString SExtendedAtlassianConfluenceBrowser::FindSpaceKeyForSelection() const
{
	// Walk up to the owning space node; its title carries the key in "Name (KEY)" form.
	TSharedPtr<FExtendedAtlassianDocNode> Node = SelectedNode;
	int32 Depth = 0;

	while (Node.IsValid() && Depth++ < 32)
	{
		if (Node->Kind == FExtendedAtlassianDocNode::EKind::Space && !Node->bIsGroup)
		{
			int32 Open = INDEX_NONE;
			if (Node->Title.FindLastChar(TEXT('('), Open) && Node->Title.EndsWith(TEXT(")")))
			{
				return Node->Title.Mid(Open + 1, Node->Title.Len() - Open - 2);
			}
			return Node->Title;
		}
		Node = Node->Parent.Pin();
	}

	return FString();
}

void SExtendedAtlassianConfluenceBrowser::BeginEdit()
{
	if (!SelectedNode.IsValid() || SelectedNode->Id.IsEmpty())
	{
		return;
	}

	const FString PageId = SelectedNode->Id;
	EditingSpaceKey = FindSpaceKeyForSelection();

	SetStatus(LOCTEXT("FetchingSource", "Fetching page source..."), false);

	TWeakPtr<SExtendedAtlassianConfluenceBrowser> WeakBrowser = SharedThis(this);

	// Editing needs storage format, not the rendered body: storage is what a write accepts, and it
	// is where macros are visible so they can be detected before anything becomes savable.
	FExtendedAtlassianConfluence::GetPageForEditing(PageId, FExtendedAtlassianPageDelegate::CreateLambda(
		[WeakBrowser](bool bSuccess, const FExtendedAtlassianPage& Page, const FExtendedAtlassianError& Error)
		{
			const TSharedPtr<SExtendedAtlassianConfluenceBrowser> Browser = WeakBrowser.Pin();
			if (!Browser.IsValid())
			{
				return;
			}

			if (!bSuccess)
			{
				Browser->SetStatus(FText::FromString(Error.Message), true);
				return;
			}

			Browser->EditingPage = Page;
			Browser->EditingTitle = Page.Title;
			Browser->bEditMode = true;

			if (Browser->DocumentEditor.IsValid())
			{
				Browser->DocumentEditor->SetMarkdown(Page.Markdown);

				// A page we cannot rebuild opens read-only: saving it would silently delete the
				// constructs that did not survive conversion.
				Browser->DocumentEditor->SetReadOnly(!Page.bCanRoundTrip, FText::Format(
					LOCTEXT("ReadOnlyReason",
						"Read-only: this page contains {0}, which this editor cannot rebuild. Saving would delete them. Use Open in Browser to edit it in Confluence."),
					FText::FromString(FString::Join(Page.RoundTripBlockers, TEXT(", ")))));
			}

			Browser->SetStatus(Page.bCanRoundTrip
				? FText::Format(LOCTEXT("EditingVersion", "Editing version {0}."), FText::AsNumber(Page.Version))
				: LOCTEXT("EditingBlocked", "Opened read-only - see the banner above."), !Page.bCanRoundTrip);
		}));
}

void SExtendedAtlassianConfluenceBrowser::BeginNewPage()
{
	EditingPage = FExtendedAtlassianPage();
	EditingPage.bCanRoundTrip = true;
	EditingSpaceKey = FindSpaceKeyForSelection();
	EditingTitle = TEXT("New Page");
	CurrentFilePath.Reset();

	// The space to create in comes from the selection; without one there is nowhere to put it.
	TSharedPtr<FExtendedAtlassianDocNode> Node = SelectedNode;
	int32 Depth = 0;
	while (Node.IsValid() && Depth++ < 32)
	{
		if (Node->Kind == FExtendedAtlassianDocNode::EKind::Space && !Node->bIsGroup)
		{
			EditingPage.SpaceId = Node->SpaceId;
			break;
		}
		Node = Node->Parent.Pin();
	}

	if (EditingPage.SpaceId.IsEmpty())
	{
		SetStatus(LOCTEXT("NeedSpace", "Select a space or a page inside one before creating a new page."), true);
		return;
	}

	bEditMode = true;

	if (DocumentEditor.IsValid())
	{
		DocumentEditor->SetReadOnly(false, FText::GetEmpty());
		DocumentEditor->SetMarkdown(TEXT("# New Page\n\n"));
	}

	SetStatus(LOCTEXT("ComposingNew", "New page - Push to Confluence creates it."), false);
}

void SExtendedAtlassianConfluenceBrowser::EndEdit()
{
	if (DocumentEditor.IsValid() && DocumentEditor->IsDirty())
	{
		const EAppReturnType::Type Answer = FMessageDialog::Open(
			EAppMsgType::YesNo,
			LOCTEXT("DiscardChanges", "This document has unsaved changes. Close the editor and discard them?"));

		if (Answer != EAppReturnType::Yes)
		{
			return;
		}
	}

	bEditMode = false;
	SetStatus(FText::GetEmpty(), false);
}

void SExtendedAtlassianConfluenceBrowser::SaveWorkingCopy()
{
	if (!DocumentEditor.IsValid())
	{
		return;
	}

	FExtendedAtlassianPage Page = EditingPage;
	Page.Title = EditingTitle;
	Page.Markdown = DocumentEditor->GetMarkdown();

	FString Path;
	if (!FExtendedAtlassianDocumentStore::Save(Page, EditingSpaceKey, Path))
	{
		SetStatus(LOCTEXT("SaveFileFailed", "Could not write the working copy. See the Output Log."), true);
		return;
	}

	CurrentFilePath = Path;
	DocumentEditor->ClearDirty();

	SetStatus(FText::Format(LOCTEXT("SavedFile", "Saved {0}"), FText::FromString(Path)), false);
}

void SExtendedAtlassianConfluenceBrowser::PushToConfluence()
{
	if (!DocumentEditor.IsValid() || bSaving)
	{
		return;
	}

	if (!EditingPage.bCanRoundTrip)
	{
		SetStatus(LOCTEXT("PushBlocked", "This page cannot be saved from the editor without losing content."), true);
		return;
	}

	const FString Title = EditingTitle.TrimStartAndEnd();
	if (Title.IsEmpty())
	{
		SetStatus(LOCTEXT("NeedTitle", "Give the page a title first."), true);
		return;
	}

	const FString Markdown = DocumentEditor->GetMarkdown();
	bSaving = true;

	TWeakPtr<SExtendedAtlassianConfluenceBrowser> WeakBrowser = SharedThis(this);

	auto OnDone = FExtendedAtlassianPageDelegate::CreateLambda(
		[WeakBrowser](bool bSuccess, const FExtendedAtlassianPage& Page, const FExtendedAtlassianError& Error)
		{
			const TSharedPtr<SExtendedAtlassianConfluenceBrowser> Browser = WeakBrowser.Pin();
			if (!Browser.IsValid())
			{
				return;
			}

			Browser->bSaving = false;

			if (!bSuccess)
			{
				// The editor stays open and dirty: a rejected push must never cost the edit.
				Browser->SetStatus(FText::FromString(Error.Message), true);
				return;
			}

			Browser->EditingPage.Id = Page.Id.IsEmpty() ? Browser->EditingPage.Id : Page.Id;
			Browser->EditingPage.Version = Page.Version;

			if (Browser->DocumentEditor.IsValid())
			{
				Browser->DocumentEditor->ClearDirty();
			}

			// The cached render is now stale.
			Browser->PageBodyCache.Remove(Browser->EditingPage.Id);
			Browser->PageBlockCache.Remove(Browser->EditingPage.Id);

			Browser->SetStatus(FText::Format(
				LOCTEXT("Pushed", "Published as version {0}."), FText::AsNumber(Page.Version)), false);
		});

	if (EditingPage.Id.IsEmpty())
	{
		FExtendedAtlassianConfluence::CreatePage(EditingPage.SpaceId, FString(), Title, Markdown, OnDone);
		SetStatus(LOCTEXT("Creating", "Creating page..."), false);
		return;
	}

	FExtendedAtlassianConfluence::UpdatePage(EditingPage.Id, Title, Markdown, EditingPage.Version, OnDone);
	SetStatus(LOCTEXT("Publishing", "Publishing..."), false);
}

void SExtendedAtlassianConfluenceBrowser::DeleteCurrentPage()
{
	if (!SelectedNode.IsValid() || SelectedNode->Id.IsEmpty())
	{
		return;
	}

	const FString PageId = SelectedNode->Id;
	const FString Title = SelectedNode->Title;

	// Deleting someone's documentation is worth an explicit confirmation naming the page.
	const EAppReturnType::Type Answer = FMessageDialog::Open(
		EAppMsgType::YesNo,
		FText::Format(LOCTEXT("ConfirmDelete", "Move \"{0}\" to the Confluence trash?"), FText::FromString(Title)));

	if (Answer != EAppReturnType::Yes)
	{
		return;
	}

	TWeakPtr<SExtendedAtlassianConfluenceBrowser> WeakBrowser = SharedThis(this);

	FExtendedAtlassianConfluence::DeletePage(PageId, FExtendedAtlassianActionDelegate::CreateLambda(
		[WeakBrowser, Title](bool bSuccess, const FExtendedAtlassianError& Error)
		{
			const TSharedPtr<SExtendedAtlassianConfluenceBrowser> Browser = WeakBrowser.Pin();
			if (!Browser.IsValid())
			{
				return;
			}

			if (!bSuccess)
			{
				Browser->SetStatus(FText::FromString(Error.Message), true);
				return;
			}

			Browser->SetStatus(FText::Format(
				LOCTEXT("Deleted", "Moved \"{0}\" to the trash."), FText::FromString(Title)), false);

			Browser->RefreshTree();
		}));
}

/** In-flight state for a whole-space pull, which outlives any single request. */
struct FExtendedAtlassianSpacePull
{
	TArray<FExtendedAtlassianPage> Pages;
	FString SpaceKey;
	FString SpaceTitle;
	int32 Index = 0;
	int32 SavedCount = 0;
	int32 FailedCount = 0;
	TWeakPtr<SExtendedAtlassianConfluenceBrowser> Browser;
};

void SExtendedAtlassianConfluenceBrowser::PullSpaceToDisk()
{
	if (bPulling)
	{
		return;
	}

	// Find the space that owns the selection, so the button works from a page as well as a space.
	TSharedPtr<FExtendedAtlassianDocNode> SpaceNode = SelectedNode;
	int32 Depth = 0;
	while (SpaceNode.IsValid() && Depth++ < 32)
	{
		if (SpaceNode->Kind == FExtendedAtlassianDocNode::EKind::Space && !SpaceNode->bIsGroup && !SpaceNode->Id.IsEmpty())
		{
			break;
		}
		SpaceNode = SpaceNode->Parent.Pin();
	}

	if (!SpaceNode.IsValid() || SpaceNode->Id.IsEmpty())
	{
		SetStatus(LOCTEXT("PullNeedSpace", "Select a space, or a page inside one, before pulling."), true);
		return;
	}

	bPulling = true;
	SetStatus(LOCTEXT("PullListing", "Listing pages..."), false);

	TSharedRef<FExtendedAtlassianSpacePull> Pull = MakeShared<FExtendedAtlassianSpacePull>();
	Pull->Browser = SharedThis(this);
	Pull->SpaceTitle = SpaceNode->Title;
	Pull->SpaceKey = FindSpaceKeyForSelection();

	if (Pull->SpaceKey.IsEmpty())
	{
		// Fall back to parsing the space node's own title, which carries "Name (KEY)".
		int32 Open = INDEX_NONE;
		if (SpaceNode->Title.FindLastChar(TEXT('('), Open) && SpaceNode->Title.EndsWith(TEXT(")")))
		{
			Pull->SpaceKey = SpaceNode->Title.Mid(Open + 1, SpaceNode->Title.Len() - Open - 2);
		}
	}

	FExtendedAtlassianConfluence::ListPages(SpaceNode->Id, FExtendedAtlassianPagesDelegate::CreateLambda(
		[Pull](bool bSuccess, const TArray<FExtendedAtlassianPage>& Pages, const FExtendedAtlassianError& Error)
		{
			const TSharedPtr<SExtendedAtlassianConfluenceBrowser> Browser = Pull->Browser.Pin();
			if (!Browser.IsValid())
			{
				return;
			}

			if (!bSuccess)
			{
				Browser->bPulling = false;
				Browser->SetStatus(FText::FromString(Error.Message), true);
				return;
			}

			Pull->Pages = Pages;
			Browser->PullNextPage(Pull);
		}));
}

void SExtendedAtlassianConfluenceBrowser::PullNextPage(TSharedRef<FExtendedAtlassianSpacePull> Pull)
{
	if (Pull->Index >= Pull->Pages.Num())
	{
		bPulling = false;

		SetStatus(Pull->FailedCount == 0
			? FText::Format(
				LOCTEXT("PullDone", "Wrote {0} page(s) to {1}"),
				FText::AsNumber(Pull->SavedCount),
				FText::FromString(FExtendedAtlassianDocumentStore::GetRootDirectory()))
			: FText::Format(
				LOCTEXT("PullDonePartial", "Wrote {0} page(s); {1} failed - see the Output Log."),
				FText::AsNumber(Pull->SavedCount),
				FText::AsNumber(Pull->FailedCount)),
			Pull->FailedCount > 0);

		RefreshTree();
		return;
	}

	const FExtendedAtlassianPage& Summary = Pull->Pages[Pull->Index];

	SetStatus(FText::Format(
		LOCTEXT("PullProgress", "Pulling {0} of {1}: {2}"),
		FText::AsNumber(Pull->Index + 1),
		FText::AsNumber(Pull->Pages.Num()),
		FText::FromString(Summary.Title)), false);

	TWeakPtr<SExtendedAtlassianConfluenceBrowser> WeakBrowser = SharedThis(this);

	// Strictly sequential. Firing a request per page in parallel is the fastest way to hit
	// Atlassian's cost-based rate limit, and the retry backoff would then serialise it anyway.
	FExtendedAtlassianConfluence::GetPageForEditing(Summary.Id, FExtendedAtlassianPageDelegate::CreateLambda(
		[WeakBrowser, Pull](bool bSuccess, const FExtendedAtlassianPage& Page, const FExtendedAtlassianError& Error)
		{
			const TSharedPtr<SExtendedAtlassianConfluenceBrowser> Browser = WeakBrowser.Pin();
			if (!Browser.IsValid())
			{
				return;
			}

			if (bSuccess)
			{
				FString Path;
				if (FExtendedAtlassianDocumentStore::Save(Page, Pull->SpaceKey, Path))
				{
					Pull->SavedCount++;
				}
				else
				{
					Pull->FailedCount++;
				}
			}
			else
			{
				// One bad page must not abandon the rest of the space.
				Pull->FailedCount++;
				UE_LOG(LogExtendedAtlassian, Warning, TEXT("Could not pull page %s: %s"),
					*Pull->Pages[Pull->Index].Title, *Error.Message);
			}

			Pull->Index++;
			Browser->PullNextPage(Pull);
		}));
}

void SExtendedAtlassianConfluenceBrowser::StartWatchingDocuments()
{
	FDirectoryWatcherModule& Module = FModuleManager::LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	IDirectoryWatcher* Watcher = Module.Get();
	if (!Watcher)
	{
		return;
	}

	const FString Root = FExtendedAtlassianDocumentStore::GetRootDirectory();
	IFileManager::Get().MakeDirectory(*Root, true);

	Watcher->RegisterDirectoryChangedCallback_Handle(
		Root,
		IDirectoryWatcher::FDirectoryChanged::CreateSP(this, &SExtendedAtlassianConfluenceBrowser::HandleDocumentsChanged),
		DirectoryWatcherHandle,
		IDirectoryWatcher::WatchOptions::IncludeDirectoryChanges);
}

void SExtendedAtlassianConfluenceBrowser::StopWatchingDocuments()
{
	if (!DirectoryWatcherHandle.IsValid())
	{
		return;
	}

	if (FDirectoryWatcherModule* Module = FModuleManager::GetModulePtr<FDirectoryWatcherModule>(TEXT("DirectoryWatcher")))
	{
		if (IDirectoryWatcher* Watcher = Module->Get())
		{
			Watcher->UnregisterDirectoryChangedCallback_Handle(
				FExtendedAtlassianDocumentStore::GetRootDirectory(), DirectoryWatcherHandle);
		}
	}

	DirectoryWatcherHandle.Reset();
}

void SExtendedAtlassianConfluenceBrowser::HandleDocumentsChanged(const TArray<FFileChangeData>& Changes)
{
	if (CurrentFilePath.IsEmpty() || !DocumentEditor.IsValid())
	{
		return;
	}

	const FString Watched = FPaths::ConvertRelativePathToFull(CurrentFilePath);

	for (const FFileChangeData& Change : Changes)
	{
		if (Change.Action == FFileChangeData::FCA_Removed)
		{
			continue;
		}

		if (FPaths::ConvertRelativePathToFull(Change.Filename) != Watched)
		{
			continue;
		}

		// An external edit wins only when the editor has nothing unsaved; silently discarding a
		// half-typed paragraph because a tool touched the file would be worse than being stale.
		if (DocumentEditor->IsDirty())
		{
			SetStatus(LOCTEXT("ExternalChangeIgnored",
				"The file changed on disk, but the editor has unsaved changes. Close the editor and reopen to load it."), true);
			return;
		}

		FExtendedAtlassianDocumentFile File;
		if (!FExtendedAtlassianDocumentStore::Load(Watched, File))
		{
			return;
		}

		DocumentEditor->SetMarkdown(File.Markdown);
		ShowDocument(FExtendedAtlassianMarkdown::ToBlocks(File.Markdown), File.Markdown);

		SetStatus(LOCTEXT("ReloadedFromDisk", "Reloaded after an external change."), false);
		return;
	}
}

void SExtendedAtlassianConfluenceBrowser::ShowDocument(const TArray<FExtendedAtlassianDocBlock>& Blocks, const FString& SourceText)
{
	CurrentBody = SourceText;

	if (DocumentView.IsValid())
	{
		DocumentView->SetBlocks(Blocks);
	}

	RefreshReferencedIssues(SourceText);
}

void SExtendedAtlassianConfluenceBrowser::RefreshReferencedIssues(const FString& SourceText)
{
	ReferencedIssues.Reset();

	if (IssueChipsBox.IsValid())
	{
		IssueChipsBox->ClearChildren();
	}

	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();
	const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient();

	if (!Settings || !Client.IsValid() || !Client->IsReady())
	{
		return;
	}

	// Restricted to real project keys so "UTF-8" and friends cannot become phantom issues.
	const TArray<FString> Keys = FExtendedAtlassianJira::ExtractIssueKeys(SourceText, Settings->GetProjectKeyOptions());
	if (Keys.Num() == 0)
	{
		return;
	}

	// One query for the whole document rather than a request per key.
	const FString Jql = FString::Printf(TEXT("key in (%s)"), *FString::Join(Keys, TEXT(",")));

	TWeakPtr<SExtendedAtlassianConfluenceBrowser> WeakBrowser = SharedThis(this);

	FExtendedAtlassianJira::SearchIssues(Jql, Keys.Num(), FExtendedAtlassianIssuesDelegate::CreateLambda(
		[WeakBrowser](const FExtendedAtlassianIssueQueryResult& Result)
		{
			const TSharedPtr<SExtendedAtlassianConfluenceBrowser> Browser = WeakBrowser.Pin();
			if (!Browser.IsValid() || !Result.bSuccess || !Browser->IssueChipsBox.IsValid())
			{
				return;
			}

			Browser->IssueChipsBox->ClearChildren();

			for (const FExtendedAtlassianIssue& Issue : Result.Issues)
			{
				Browser->ReferencedIssues.Add(MakeShared<FExtendedAtlassianIssue>(Issue));

				const FString Key = Issue.Key;
				const FLinearColor StatusColor =
					Issue.StatusCategoryKey == TEXT("done") ? FLinearColor(0.30f, 0.78f, 0.40f) :
					Issue.StatusCategoryKey == TEXT("indeterminate") ? FLinearColor(0.35f, 0.62f, 0.92f) :
					FLinearColor(0.70f, 0.70f, 0.70f);

				Browser->IssueChipsBox->AddSlot()
					.Padding(0.0f, 0.0f, 6.0f, 4.0f)
					[
						SNew(SButton)
						.ContentPadding(FMargin(6.0f, 2.0f))
						.ToolTipText(FText::FromString(FString::Printf(
							TEXT("%s\n%s - %s"), *Issue.Summary, *Issue.StatusName,
							Issue.AssigneeDisplayName.IsEmpty() ? TEXT("Unassigned") : *Issue.AssigneeDisplayName)))
						.OnClicked_Lambda([Key]()
						{
							const FString Url = FExtendedAtlassianJira::GetIssueBrowseUrl(Key);
							if (!Url.IsEmpty())
							{
								FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
							}
							return FReply::Handled();
						})
						[
							SNew(STextBlock)
							.Text(FText::FromString(FString::Printf(TEXT("%s  %s"), *Issue.Key, *Issue.StatusName)))
							.ColorAndOpacity(FSlateColor(StatusColor))
						]
					];
			}
		}));
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
