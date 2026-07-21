// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestEditor.h"

#include "Editor.h"
#include "SSingleObjectDetailsPanel.h"
#include "Widgets/Docking/SDockTab.h"
#include "ScopedTransaction.h"
#include "Framework/Application/SlateApplication.h"
#include "PropertyEditorModule.h"
#include "GraphEditor.h"
#include "Toolkits/IToolkitHost.h"
#include "IDetailsView.h"
#include "Framework/Commands/GenericCommands.h"
#include "GraphEditorActions.h"
#include "Modules/ModuleManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EdGraphUtilities.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Editor/Transactor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"

#include "UnrealExtendedQuest/EGQuestScript.h"

#include "UnrealExtendedQuestEditor/EGQuestPluginEditorModule.h"
#include "UnrealExtendedQuestEditor/EGQuestStyle.h"
#include "UnrealExtendedQuest/EGQuestGraph.h"
#include "UnrealExtendedQuest/EGQuestHelper.h"
#include "SEGQuestGraphActionMenu.h"
#include "UnrealExtendedQuestEditor/Editor/Nodes/EGQuestGraphNode_Root.h"
#include "UnrealExtendedQuestEditor/Editor/Nodes/EGQuestGraphNode.h"
#include "UnrealExtendedQuestEditor/Editor/Graph/EGQuestEdGraphSchema.h"
#include "UnrealExtendedQuestEditor/EGQuestCommands.h"
#include "UnrealExtendedQuestEditor/Search/EGQuestSearchManager.h"
#include "UnrealExtendedQuestEditor/Search/SEGQuestFindInQuests.h"

#define LOCTEXT_NAMESPACE "QuestEditor"

// define constants
const FName FEGQuestEditor::DetailsTabID(TEXT("QuestEditor_Details"));
const FName FEGQuestEditor::GraphCanvasTabID(TEXT("QuestEditor_GraphCanvas"));
const FName FEGQuestEditor::FindInQuestTabId(TEXT("QuestEditor_Find"));
const FName QuestEditorAppName = FName(TEXT("QuestEditorAppName"));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FEGQuestEditor
FEGQuestEditor::FEGQuestEditor() : QuestBeingEdited(nullptr)
{
	GEditor->RegisterForUndo(this);
}

FEGQuestEditor::~FEGQuestEditor()
{
	GEditor->UnregisterForUndo(this);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin IToolkit interface
void FEGQuestEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = TabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_QuestEditor", "Quest Editor"));
	const TSharedRef<FWorkspaceItem> WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	// spawn tabs
	InTabManager->RegisterTabSpawner(
		GraphCanvasTabID,
		FOnSpawnTab::CreateSP(this, &Self::SpawnTab_GraphCanvas)
	)
	.SetDisplayName(LOCTEXT("GraphCanvasTab", "Viewport"))
	.SetGroup(WorkspaceMenuCategoryRef)
	.SetIcon(FSlateIcon(NY_GET_APP_STYLE_NAME(), "GraphEditor.EventGraph_16x"));

	InTabManager->RegisterTabSpawner(
		DetailsTabID,
		FOnSpawnTab::CreateSP(this, &Self::SpawnTab_Details)
	)
	.SetDisplayName(LOCTEXT("DetailsTabLabel", "Details"))
	.SetGroup(WorkspaceMenuCategoryRef)
	.SetIcon(FSlateIcon(NY_GET_APP_STYLE_NAME(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(
		FindInQuestTabId,
		FOnSpawnTab::CreateSP(this, &Self::SpawnTab_FindInQuest)
	)
	.SetDisplayName(LOCTEXT("FindInQuestTab", "Find Results"))
	.SetGroup(WorkspaceMenuCategoryRef)
	.SetIcon(FSlateIcon(NY_GET_APP_STYLE_NAME(), "Kismet.Tabs.FindResults"));
}

void FEGQuestEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(GraphCanvasTabID);
	InTabManager->UnregisterTabSpawner(DetailsTabID);
	InTabManager->UnregisterTabSpawner(FindInQuestTabId);
}

FText FEGQuestEditor::GetBaseToolkitName() const
{
	return LOCTEXT("QuestEditorAppLabel", "Quest Editor");
}

FText FEGQuestEditor::GetToolkitName() const
{
	const bool bDirtyState = QuestBeingEdited->GetOutermost()->IsDirty();

	FFormatNamedArguments Args;
	Args.Add(TEXT("QuestName"), FText::FromString(QuestBeingEdited->GetName()));
	Args.Add(TEXT("DirtyState"), bDirtyState ? FText::FromString(TEXT("*")) : FText::GetEmpty());
	return FText::Format(LOCTEXT("QuestEditorToolkitName", "{QuestName}{DirtyState}"), Args);
}
// End of IToolkit Interface
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin FAssetEditorToolkit
void FEGQuestEditor::SaveAsset_Execute()
{
	FAssetEditorToolkit::SaveAsset_Execute();
}

void FEGQuestEditor::SaveAssetAs_Execute()
{
	FAssetEditorToolkit::SaveAssetAs_Execute();
}
// End of FAssetEditorToolkit
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin IAssetEditorInstance
void FEGQuestEditor::FocusWindow(UObject* ObjectToFocusOn)
{
	BringToolkitToFront();
	JumpToObject(ObjectToFocusOn);
}
// End of IAssetEditorInstance
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin FEditorUndoClient Interface
void FEGQuestEditor::PostUndo(bool bSuccess)
{
	if (bSuccess)
	{
		HandleTransactionReverted();
	}
}

void FEGQuestEditor::PostRedo(bool bSuccess)
{
	if (bSuccess)
	{
		HandleTransactionReverted();
	}
}

void FEGQuestEditor::HandleTransactionReverted()
{
	// The graph is the editing authority and the runtime arrays are compiler output, so whatever
	// mix of the two the transaction restored, one compile makes them agree again. Asserting that
	// they already agree here would fail on perfectly ordinary undos.
	QuestBeingEdited->CompileQuestNodesFromGraphNodes();
	Refresh(false);
}
// End of FEditorUndoClient Interface
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin IEGQuestEditor

void FEGQuestEditor::RefreshDetailsView(bool bRestorePreviousSelection)
{
	if (DetailsView.IsValid())
	{
		if (QuestBeingEdited)
		{
			DetailsView->SetObject(QuestBeingEdited, true);
		}
		else
		{
			DetailsView->ForceRefresh();
		}
		DetailsView->ClearSearch();
	}

	if (bRestorePreviousSelection)
	{
		// Create local copy because this can be changed by node selection again
		TArray<TWeakObjectPtr<UObject>> ArrayCopy = PreviousSelectedNodeObjects;

		// Select all previous nodes
		for (const TWeakObjectPtr<UObject>& WeakObj : ArrayCopy)
		{
			if (!WeakObj.IsValid(false))
			{
				continue;
			}

			UObject* Object = WeakObj.Get();
			if (!IsValid(Object))
			{
				continue;
			}

			UEdGraphNode* GraphNode = Cast<UEdGraphNode>(Object);
			if (!IsValid(GraphNode))
			{
				continue;
			}

			GraphEditorView->SetNodeSelection(const_cast<UEdGraphNode*>(GraphNode), true);
		}
	}
}

void FEGQuestEditor::Refresh(bool bRestorePreviousSelection)
{
	ClearViewportSelection();
	RefreshViewport();
	RefreshDetailsView(bRestorePreviousSelection);
	FSlateApplication::Get().DismissAllMenus();
}

void FEGQuestEditor::JumpToObject(const UObject* Object)
{
	// Ignore invalid objects
	if (!IsValid(Object) || !GraphEditorView.IsValid())
	{
		return;
	}

	const UEdGraphNode* GraphNode = Cast<UEdGraphNode>(Object);
	if (!IsValid(GraphNode))
	{
		return;
	}

	// Are we in the same graph?
	if (QuestBeingEdited->GetGraph() != GraphNode->GetGraph())
	{
		return;
	}

	// TODO create custom SGraphEditor
	// Not part of the graph anymore :(
	if (!QuestBeingEdited->GetGraph()->Nodes.Contains(GraphNode))
	{
		return;
	}

	// Jump to the node
	static constexpr bool bRequestRename = false;
	static constexpr bool bSelectNode = true;
	Refresh(false);
	GraphEditorView->JumpToNode(GraphNode, bRequestRename, bSelectNode);

	// Select from JumpNode seems to be buggy sometimes, WE WILL DO IT OURSELFS!
	// I know, I know, it is not my fault that SetNodeSelection does not have the graph node as const, sigh
	GraphEditorView->SetNodeSelection(const_cast<UEdGraphNode*>(GraphNode), bSelectNode);
}
// End of IEGQuestEditor
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Begin own functions
void FEGQuestEditor::SummonSearchUI(bool bSetFindWithinQuest, FString NewSearchTerms, bool bSelectFirstResult)
{
	TSharedPtr<SEGQuestFindInQuests> FindResultsToUse;
	if (bSetFindWithinQuest)
	{
		// Open local tab
		FindResultsToUse = FindResultsView;
		FEGQuestHelper::InvokeTab(TabManager, FindInQuestTabId);
	}
	else
	{
		// Open global tab
		FindResultsToUse = FEGQuestSearchManager::Get()->GetGlobalFindResults();
	}

	if (FindResultsToUse.IsValid())
	{
		FEGQuestSearchFilter Filter;
		Filter.SearchString = NewSearchTerms;
		FindResultsToUse->FocusForUse(bSetFindWithinQuest, Filter, bSelectFirstResult);
	}
}

void FEGQuestEditor::InitQuestEditor(
	EToolkitMode::Type Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost,
	UEGQuestGraph* InitQuest
)
{
	Settings = GetMutableDefault<UEGQuestPluginSettings>();

	// close all other editors editing this asset
	FEGQuestEditorUtilities::CloseOtherEditors(InitQuest, this);
	QuestBeingEdited = InitQuest;
	FEGQuestEditorUtilities::TryToCreateDefaultGraph(QuestBeingEdited);

	// Bind Undo/Redo methods
	QuestBeingEdited->SetFlags(RF_Transactional);

	// Sync the runtime data with the graph so warnings are visible from the first frame.
	QuestBeingEdited->InitialCompileQuestNodesFromGraphNodes();

	// Bind commands
	FGraphEditorCommands::Register();
	FEGQuestCommands::Register();
	BindEditorCommands();
	CreateInternalWidgets();

	// Default layout
	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout =
		FTabManager::NewLayout("Standalone_QuestEditor_Layout_v2")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				// Toolbar
				FTabManager::NewStack()
				->SetSizeCoefficient(0.1f)
#if NY_ENGINE_VERSION < 500
				->AddTab(GetToolbarTabId(), ETabState::OpenedTab)
#endif
				->SetHideTabWell(true)
			)
			->Split
			(
				// Main Application area
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->SetSizeCoefficient(0.9f)
				->Split
				(
					// Left
					// Details tab
					FTabManager::NewStack()
					->SetSizeCoefficient(0.3f)
					->AddTab(DetailsTabID, ETabState::OpenedTab)
				)
				->Split
				(
					// Middle
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.7f)
					->Split
					(
						// Top
						// Graph canvas
						FTabManager::NewStack()
						->SetSizeCoefficient(0.8f)
						->SetHideTabWell(true)
						->AddTab(GraphCanvasTabID, ETabState::OpenedTab)
					)
					->Split
					(
						// Bottom
						// Find Quest results
						FTabManager::NewStack()
						->SetSizeCoefficient(0.2f)
						->AddTab(FindInQuestTabId, ETabState::ClosedTab)
					)

				)

			)
		);

	// Initialize the asset editor and spawn nothing (dummy layout)
	constexpr bool bCreateDefaultStandaloneMenu = true;
	constexpr bool bCreateDefaultToolbar = true;
	constexpr bool bInIsToolbarFocusable = false;
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, QuestEditorAppName, StandaloneDefaultLayout,
			bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, /*ObjectToEdit =*/ InitQuest, bInIsToolbarFocusable);

	// extend menus and toolbar
	ExtendMenu();
	ExtendToolbar();
	RegenerateMenusAndToolbars();
}

void FEGQuestEditor::SetQuestBeingEdited(UEGQuestGraph* NewQuest)
{
	// TODO do we need this method?

	// not different or a null poointer, do not set anything
	if (NewQuest == QuestBeingEdited || !IsValid(NewQuest))
		return;

	// set to the new quest
	UEGQuestGraph* OldQuest = QuestBeingEdited;
	QuestBeingEdited = NewQuest;

	// Let the viewport know that we are editing something different

	// Let the editor know that are editing something different
	RemoveEditingObject(OldQuest);
	AddEditingObject(NewQuest);

	// Update the asset picker to select the new active quest
}

void FEGQuestEditor::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
{
	if (GraphEditorView.IsValid() && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		GraphEditorView->NotifyGraphChanged();
	}
}

void FEGQuestEditor::CreateInternalWidgets()
{
	// The graph Viewport
	GraphEditorView = CreateGraphEditorWidget();

	// Details View (properties panel)
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.NotifyHook = this;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::ObjectsUseNameArea;
	DetailsViewArgs.bHideSelectionTip = false;

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObject(QuestBeingEdited);

	// Find Results
	FindResultsView = SNew(SEGQuestFindInQuests, SharedThis(this));
}

TSharedRef<SGraphEditor> FEGQuestEditor::CreateGraphEditorWidget()
{
	check(QuestBeingEdited);
	// Customize the appearance of the graph.
	FGraphAppearanceInfo AppearanceInfo;
	// The text that appears on the bottom right corner in the graph view.
	AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_QuestGraph", "QUEST");
	AppearanceInfo.InstructionText = LOCTEXT("AppearanceInstructionText_QuestGraph", "Right Click to add new nodes.");

	// Bind graph events actions from the editor
	SGraphEditor::FGraphEditorEvents InEvents;
	InEvents.OnTextCommitted = FOnNodeTextCommitted::CreateSP(this, &FEGQuestEditor::OnNodeTitleCommitted);
	InEvents.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &FEGQuestEditor::OnSelectedNodesChanged);
#if NY_ENGINE_VERSION >= 506
	InEvents.OnCreateActionMenuAtLocation = SGraphEditor::FOnCreateActionMenuAtLocation::CreateSP(this, &FEGQuestEditor::OnCreateGraphActionMenu);
#else
	InEvents.OnCreateActionMenu = SGraphEditor::FOnCreateActionMenu::CreateSP(this, &FEGQuestEditor::OnCreateGraphActionMenu);
#endif

	return SNew(SGraphEditor)
		.AdditionalCommands(GraphEditorCommands)
		.IsEditable(true)
		.Appearance(AppearanceInfo)
		.GraphToEdit(QuestBeingEdited->GetGraph())
		.GraphEvents(InEvents)
		.ShowGraphStateOverlay(false);
}

void FEGQuestEditor::BindEditorCommands()
{
	// Prevent duplicate assigns. This should never happen
	if (GraphEditorCommands.IsValid())
	{
		return;
	}
	GraphEditorCommands = MakeShared<FUICommandList>();

	// Graph Editor Commands
	// Create comment node on graph. Default when you press the "C" key on the keyboard to create a comment.
	GraphEditorCommands->MapAction(
		FGraphEditorCommands::Get().CreateComment,
		FExecuteAction::CreateLambda([this]
		{
			FEGQuestNewComment_GraphSchemaAction CommentAction;
#if NY_ENGINE_VERSION >= 506
			FVector2f PasteLocation = GraphEditorView->GetPasteLocation2f();
#else
			FVector2D PasteLocation = GraphEditorView->GetPasteLocation();
#endif
			CommentAction.PerformAction(QuestBeingEdited->GetGraph(), nullptr, PasteLocation);
		})
	);

	GraphEditorCommands->MapAction(
		FGenericCommands::Get().SelectAll,
		FExecuteAction::CreateLambda([this] { GraphEditorView->SelectAllNodes(); } ),
		FCanExecuteAction::CreateLambda([] { return true; })
	);

	// Edit Node commands
	const auto QuestCommands = FEGQuestCommands::Get();
	GraphEditorCommands->MapAction(
		FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &Self::OnCommandDeleteSelectedNodes),
		FCanExecuteAction::CreateSP(this, &Self::CanDeleteNodes)
	);

	GraphEditorCommands->MapAction(
		FGenericCommands::Get().Copy,
		FExecuteAction::CreateRaw(this, &Self::OnCommandCopySelectedNodes),
		FCanExecuteAction::CreateRaw(this, &Self::CanCopyNodes)
	);

	GraphEditorCommands->MapAction(
		FGenericCommands::Get().Cut,
		FExecuteAction::CreateRaw(this, &Self::OnCommandCutSelectedNodes),
		FCanExecuteAction::CreateRaw(this, &Self::CanCutNodes)
	);

	GraphEditorCommands->MapAction(
		FGenericCommands::Get().Paste,
		FExecuteAction::CreateRaw(this, &Self::OnCommandPasteNodes),
		FCanExecuteAction::CreateRaw(this, &Self::CanPasteNodes)
	);

	GraphEditorCommands->MapAction(
		FGenericCommands::Get().Duplicate,
		FExecuteAction::CreateRaw(this, &Self::OnCommandDuplicateNodes),
		FCanExecuteAction::CreateRaw(this, &Self::CanCopyNodes)
	);

	// Toolikit/Toolbar commands/Menu Commands
	// Undo Redo menu options
	ToolkitCommands->MapAction(
		FGenericCommands::Get().Undo,
		FExecuteAction::CreateSP(this, &Self::OnCommandUndoGraphAction)
	);

	ToolkitCommands->MapAction(
		FGenericCommands::Get().Redo,
		FExecuteAction::CreateSP(this, &Self::OnCommandRedoGraphAction)
	);

	// The toolbar compile button
	ToolkitCommands->MapAction(
		QuestCommands.QuestCompile,
		FExecuteAction::CreateSP(this, &Self::OnCommandQuestCompile)
	);

	// The toolbar script button
	ToolkitCommands->MapAction(
		QuestCommands.OpenQuestScript,
		FExecuteAction::CreateSP(this, &Self::OnCommandOpenQuestScript)
	);

	// Find in All Quests
	ToolkitCommands->MapAction(
		FEGQuestCommands::Get().FindInAllQuests,
		FExecuteAction::CreateLambda([this] { SummonSearchUI(false); })
	);

	// Find in current Quest
	ToolkitCommands->MapAction(
		FEGQuestCommands::Get().FindInQuest,
		FExecuteAction::CreateLambda([this] { SummonSearchUI(true); })
	);

	// Map the global actions
	FEGQuestPluginEditorModule::MapActionsForFileMenuExtender(ToolkitCommands);

	ToolkitCommands->MapAction(
		FEGQuestCommands::Get().DeleteCurrentQuestTextFiles,
		FExecuteAction::CreateLambda([this]
		{
			check(QuestBeingEdited);
			const FString Text = TEXT("Delete All text files for this Quest?");
			const EAppReturnType::Type Response = FPlatformMisc::MessageBoxExt(EAppMsgType::YesNo, *Text, *Text);
			if (Response == EAppReturnType::Yes)
			{
				QuestBeingEdited->DeleteAllTextFiles();
			}
		})
	);
}

void FEGQuestEditor::ExtendMenu()
{
	TSharedPtr<FExtender> MenuExtender = MakeShared<FExtender>();

	// Extend the Edit menu
	MenuExtender->AddMenuExtension(
		"EditHistory",
		EExtensionHook::After,
		ToolkitCommands,
		FMenuExtensionDelegate::CreateLambda([this](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.BeginSection("EditSearch", LOCTEXT("EditMenu_SearchHeading", "Search") );
			{
				MenuBuilder.AddMenuEntry(FEGQuestCommands::Get().FindInQuest);
				MenuBuilder.AddMenuEntry(FEGQuestCommands::Get().FindInAllQuests);
			}
			MenuBuilder.EndSection();
		})
	);

	AddMenuExtender(MenuExtender);

	AddMenuExtender(
		FEGQuestPluginEditorModule::CreateFileMenuExtender(
			ToolkitCommands,
			{FEGQuestCommands::Get().DeleteCurrentQuestTextFiles}
		)
	);
}

void FEGQuestEditor::ExtendToolbar()
{
	// Make toolbar to the right of the Asset Toolbar (Save and Find in content browser buttons).
	{
		TSharedPtr<FExtender> ToolbarExtender = MakeShared<FExtender>();
		ToolbarExtender->AddToolBarExtension(
			"Asset",
			EExtensionHook::After,
			ToolkitCommands,
			FToolBarExtensionDelegate::CreateLambda([this](FToolBarBuilder& ToolbarBuilder)
			{
				ToolbarBuilder.BeginSection("Quest");
				{
					// Compile is the action this editor exists for, so it gets a green button. The
					// style is swapped only around it, and restored so its neighbours are untouched.
					const ISlateStyle* OriginalStyleSet = ToolbarBuilder.GetStyleSet();
					const FName OriginalStyleName = ToolbarBuilder.GetStyleName();
					const FName GreenStyleName = FEGQuestStyle::GetGreenToolBarStyle(OriginalStyleSet, OriginalStyleName);
					const bool bHasGreenStyle = GreenStyleName != OriginalStyleName;
					if (bHasGreenStyle)
					{
						ToolbarBuilder.SetStyle(FEGQuestStyle::Get().Get(), GreenStyleName);
					}
					ToolbarBuilder.AddToolBarButton(FEGQuestCommands::Get().QuestCompile);
					if (bHasGreenStyle)
					{
						ToolbarBuilder.SetStyle(OriginalStyleSet, OriginalStyleName);
					}

					ToolbarBuilder.AddToolBarButton(
						FEGQuestCommands::Get().OpenQuestScript,
						NAME_None,
						TAttribute<FText>(),
						TAttribute<FText>(),
						FSlateIcon(NY_GET_APP_STYLE_NAME(), "GraphEditor.EventGraph_24x", "GraphEditor.EventGraph_16x")
					);
					ToolbarBuilder.AddToolBarButton(FEGQuestCommands::Get().FindInQuest);
				}
				ToolbarBuilder.EndSection();
			})
		);
		AddToolbarExtender(ToolbarExtender);
	}
}

TSharedRef<SDockTab> FEGQuestEditor::SpawnTab_Details(const FSpawnTabArgs& Args) const
{
	check(Args.GetTabId() == DetailsTabID);

	TSharedRef<SDockTab> NewTab = SNew(SDockTab)
		.Label(LOCTEXT("QuestDetailsTitle", "Details"))
		.TabColorScale(GetTabColorScale())
		[
			DetailsView.ToSharedRef()
		];

	// TODO use QuestEditor.Tabs.Properties
	const auto* IconBrush = FNYAppStyle::GetBrush(TEXT("GenericEditor.Tabs.Properties"));
	NewTab->SetTabIcon(IconBrush);

	return NewTab;
}

TSharedRef<SDockTab> FEGQuestEditor::SpawnTab_GraphCanvas(const FSpawnTabArgs& Args) const
{
	check(Args.GetTabId() == GraphCanvasTabID);
	return SNew(SDockTab)
		.Label(LOCTEXT("QuestEdGraphCanvasTiele", "Viewport"))
		[
			GraphEditorView.ToSharedRef()
		];
}

TSharedRef<SDockTab> FEGQuestEditor::SpawnTab_FindInQuest(const FSpawnTabArgs& Args) const
{
	check(Args.GetTabId() == FindInQuestTabId);

	TSharedRef<SDockTab> NewTab = SNew(SDockTab)
		.Label(LOCTEXT("FindResultsView", "Find Results"))
		[
			FindResultsView.ToSharedRef()
		];

	const auto* IconBrush = FNYAppStyle::GetBrush(TEXT("Kismet.Tabs.FindResults"));
	NewTab->SetTabIcon(IconBrush);

	return NewTab;
}

void FEGQuestEditor::OnCommandUndoGraphAction() const
{
	constexpr bool bCanRedo = true;
	if (!GEditor->UndoTransaction(bCanRedo))
	{
		const FText TransactionName = GEditor->Trans->GetUndoContext(true).Title;
		UE_LOG(LogEGQuestPluginEditor, Warning, TEXT("Undo Transaction with Name = `%s` failed"), *TransactionName.ToString());
	}
}

void FEGQuestEditor::OnCommandRedoGraphAction() const
{
	// Clear selection, to avoid holding refs to nodes that go away
	ClearViewportSelection();
	if (!GEditor->RedoTransaction())
	{
		const FText TransactionName = GEditor->Trans->GetUndoContext(true).Title;
		UE_LOG(LogEGQuestPluginEditor, Warning, TEXT("Redo Transaction with Name = `%s` failed"), *TransactionName.ToString());
	}
}

void FEGQuestEditor::OnCommandDeleteSelectedNodes() const
{
	const FScopedTransaction Transaction(LOCTEXT("QuestEditRemoveSelectedNode", "Quest Editor: Remove Node"));
	QuestBeingEdited->Modify();

	const TSet<UObject*> SelectedNodes = GetSelectedNodes();
	UEGQuestEdGraph* QuestEdGraph = GetQuestEdGraph();
	QuestEdGraph->Modify();

	int32 NumStartNodesRemoved = 0;
	int32 NumQuestNodesRemoved = 0;
	const int32 Initial_StartNodeNum = QuestEdGraph->GetRootGraphNodes().Num();

	// Unselect nodes we are about to delete
	ClearViewportSelection();

	// Compile once at the end, not per removed node
	QuestBeingEdited->DisableCompileQuest();

	for (UObject* NodeObject : SelectedNodes)
	{
		UEdGraphNode* SelectedNode = CastChecked<UEdGraphNode>(NodeObject);

		if (!SelectedNode->CanUserDeleteNode())
		{
			continue;
		}

		// only allow removing root nodes while at least one remains
		if (SelectedNode->IsA<UEGQuestGraphNode_Root>())
		{
			if (NumStartNodesRemoved == Initial_StartNodeNum - 1)
			{
				continue;
			}
			NumStartNodesRemoved++;
		}
		else if (SelectedNode->IsA<UEGQuestGraphNode>())
		{
			// Deleting a stage card deletes its rows with it: the objectives live on the card.
			NumQuestNodesRemoved++;
		}

		// Removing the node breaks its links, which notifies its neighbours.
		FEGQuestEditorUtilities::RemoveNode(SelectedNode);
	}

	QuestBeingEdited->EnableCompileQuest();
	if (NumQuestNodesRemoved > 0 || NumStartNodesRemoved > 0)
	{
		QuestBeingEdited->CompileQuestNodesFromGraphNodes();
		QuestBeingEdited->PostEditChange();
		QuestBeingEdited->MarkPackageDirty();
		RefreshViewport();
	}
}

bool FEGQuestEditor::CanDeleteNodes() const
{
	const TSet<UObject*>& SelectedNodes = GetSelectedNodes();
	// Return false if only the last root node is selected, as it can't be deleted
	if (SelectedNodes.Num() == 1)
	{
		const UObject* SelectedNode = *FEGQuestHelper::GetFirstSetElement(SelectedNodes);
		return !SelectedNode->IsA(UEGQuestGraphNode_Root::StaticClass()) || GetQuestEdGraph()->GetRootGraphNodes().Num() > 0;
	}

	return SelectedNodes.Num() > 0;
}

void FEGQuestEditor::OnCommandCopySelectedNodes() const
{
	// Export the selected nodes and place the text on the clipboard. Nodes that cannot be
	// duplicated (the root) are dropped here, or pasting would mint a second start node.
	TSet<UObject*> SelectedNodes = GetSelectedNodes();
	for (auto It = SelectedNodes.CreateIterator(); It; ++It)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(*It);
		if (!Node || !Node->CanDuplicateNode())
		{
			It.RemoveCurrent();
			continue;
		}
		Node->PrepareForCopying();
	}

	// Copy to clipboard
	FString ExportedText;
	FEdGraphUtilities::ExportNodesToText(SelectedNodes, /*out*/ ExportedText);
	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);

	for (UObject* Object : SelectedNodes)
	{
		if (UEGQuestGraphNode_Base* Node = Cast<UEGQuestGraphNode_Base>(Object))
		{
			Node->PostCopyNode();
		}
	}
}

bool FEGQuestEditor::CanCopyNodes() const
{
	// If any of the nodes can be duplicated then we should allow copying
	for (UObject* Object : GetSelectedNodes())
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(Object);
		if (Node && Node->CanDuplicateNode())
		{
			return true;
		}
	}

	return false;
}

void FEGQuestEditor::OnCommandCutSelectedNodes() const
{
	// Cut is copy plus delete; the delete carries the transaction, so one undo restores the nodes.
	OnCommandCopySelectedNodes();
	OnCommandDeleteSelectedNodes();
}

bool FEGQuestEditor::CanCutNodes() const
{
	return CanCopyNodes() && CanDeleteNodes();
}

void FEGQuestEditor::OnCommandDuplicateNodes()
{
	OnCommandCopySelectedNodes();

	// Land the copies offset from the originals, so a duplicate is visibly a duplicate rather than
	// a perfect overlap.
	FNYVector2f PasteLocation;
	FSlateRect SelectionBounds;
	if (GetBoundsForSelectedNodes(SelectionBounds, 0.f))
	{
		PasteLocation = FNYVector2f(
			(SelectionBounds.Left + SelectionBounds.Right) * 0.5f + 40.f,
			(SelectionBounds.Top + SelectionBounds.Bottom) * 0.5f + 40.f);
	}
	else
	{
#if NY_ENGINE_VERSION >= 506
		PasteLocation = GraphEditorView->GetPasteLocation2f();
#else
		PasteLocation = GraphEditorView->GetPasteLocation();
#endif
	}

	PasteNodesHere(PasteLocation);
}

void FEGQuestEditor::OnCommandPasteNodes()
{
#if NY_ENGINE_VERSION >= 506
	FVector2f PasteLocation = GraphEditorView->GetPasteLocation2f();
#else
	FVector2D PasteLocation = GraphEditorView->GetPasteLocation();
#endif
	PasteNodesHere(PasteLocation);
}

void FEGQuestEditor::PasteNodesHere(const FNYVector2f& Location)
{
	// Undo/Redo support
	const FScopedTransaction Transaction(FGenericCommands::Get().Paste->GetDescription());
	UEGQuestEdGraph* QuestEdGraph = GetQuestEdGraph();
	QuestBeingEdited->Modify();
	QuestEdGraph->Modify();

	// Clear the selection set (newly pasted stuff will be selected)
	ClearViewportSelection();

	// Compile once at the end
	QuestBeingEdited->DisableCompileQuest();

	// Grab the text to paste from the clipboard.
	FString TextToImport;
	FPlatformApplicationMisc::ClipboardPaste(TextToImport);

	// Import the nodes. PostEditImport on each pasted card already reset ownership to this quest,
	// regenerated the stage/objective GUIDs and rebuilt the pins.
	TSet<UEdGraphNode*> PastedNodes;
	FEdGraphUtilities::ImportNodesFromText(QuestEdGraph, TextToImport, /*out*/ PastedNodes);

	// Average position of nodes so we can move them while still maintaining relative distances to each other
	FVector2f AvgNodePosition(0.0f, 0.0f);
	for (UEdGraphNode* Node : PastedNodes)
	{
		AvgNodePosition.X += Node->NodePosX;
		AvgNodePosition.Y += Node->NodePosY;
	}
	if (PastedNodes.Num() > 0)
	{
		const float InvNumNodes = 1.0f / float(PastedNodes.Num());
		AvgNodePosition.X *= InvNumNodes;
		AvgNodePosition.Y *= InvNumNodes;
	}

	for (UEdGraphNode* Node : PastedNodes)
	{
		// Select the newly pasted stuff
		GraphEditorView->SetNodeSelection(Node, true);

		const float NodePosX = (Node->NodePosX - AvgNodePosition.X) + Location.X;
		const float NodePosY = (Node->NodePosY - AvgNodePosition.Y) + Location.Y;
		if (auto* GraphNodeBase = Cast<UEGQuestGraphNode_Base>(Node))
		{
			GraphNodeBase->SetPosition(NodePosX, NodePosY);
		}
		else
		{
			Node->NodePosX = NodePosX;
			Node->NodePosY = NodePosY;
		}

		// Assign new ID
		Node->CreateNewGuid();
	}

	// Compile: the compiler rebuilds the runtime arrays wholesale from the graph, which folds the
	// pasted cards in with correct indices and edges.
	QuestBeingEdited->EnableCompileQuest();
	QuestBeingEdited->CompileQuestNodesFromGraphNodes();

	// Notify objects of change
	RefreshViewport();
	QuestBeingEdited->PostEditChange();
	QuestBeingEdited->MarkPackageDirty();
}

bool FEGQuestEditor::CanPasteNodes() const
{
	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);

	return FEdGraphUtilities::CanImportNodesFromText(QuestBeingEdited->GetGraph(), ClipboardContent);
}

void FEGQuestEditor::OnCommandQuestCompile() const
{
	check(QuestBeingEdited);
	// Graph data -> quest data. Deliberately not the OnPreAssetSaved path: that also exports the
	// .quest text file, and compiling should not touch the disk.
	QuestBeingEdited->CompileQuestNodesFromGraphNodes();
	QuestBeingEdited->UpdateAndRefreshData(true);
	// Compiling rewrites the runtime node array, so the asset now differs from what is on disk.
	QuestBeingEdited->MarkPackageDirty();
}

void FEGQuestEditor::OnCommandOpenQuestScript()
{
	check(QuestBeingEdited);

	// The script baked into this asset.
	if (UBlueprint* Embedded = QuestBeingEdited->GetQuestScriptBlueprint())
	{
		FEGQuestEditorUtilities::OpenBlueprintEditor(Embedded);
		return;
	}

	// No embedded script, but a class assigned by hand: open it when it is a separate-asset
	// blueprint (the pre-embedded flow), explain when it is native.
	if (UClass* ScriptClass = QuestBeingEdited->GetQuestScriptClass())
	{
		if (UBlueprint* Blueprint = Cast<UBlueprint>(ScriptClass->ClassGeneratedBy))
		{
			FEGQuestEditorUtilities::OpenBlueprintEditor(Blueprint);
		}
		else
		{
			FEGQuestEditorUtilities::ShowMessageBox(EAppMsgType::Ok,
				FString::Printf(TEXT("The quest script class '%s' is native C++, there is no Blueprint to open."), *ScriptClass->GetName()),
				TEXT("Quest Script"));
		}
		return;
	}

	// No script yet: bake one into the quest asset, the way a level blueprint lives in its level.
	// It is not a content browser asset - it saves, duplicates and deletes with the quest.
	const FScopedTransaction Transaction(LOCTEXT("QuestEditorCreateScript", "Quest Editor: Create Quest Script"));
	QuestBeingEdited->Modify();

	const FName ScriptName = MakeUniqueObjectName(
		QuestBeingEdited, UBlueprint::StaticClass(), FName(*(QuestBeingEdited->GetName() + TEXT("_Script"))));
	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		UEGQuestScript::StaticClass(), QuestBeingEdited, ScriptName, BPTYPE_Normal,
		UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), FName("QuestEditor"));
	if (!Blueprint)
	{
		return;
	}

	QuestBeingEdited->SetQuestScriptBlueprint(Blueprint);
	QuestBeingEdited->SetQuestScriptClass(Cast<UClass>(Blueprint->GeneratedClass));
	QuestBeingEdited->MarkPackageDirty();
	RefreshDetailsView(true);

	FEGQuestEditorUtilities::OpenBlueprintEditor(Blueprint);
}

void FEGQuestEditor::OnSelectedNodesChanged(const TSet<UObject*>& NewSelection)
{
	TArray<UObject*> ViewSelection;

	PreviousSelectedNodeObjects.Empty();
	if (NewSelection.Num())
	{
		for (UObject* Selected : NewSelection)
		{
			PreviousSelectedNodeObjects.Add(Selected);
			ViewSelection.Add(Selected);
		}
	}
	else
	{
		// Nothing selected, view the properties of this Quest.
		ViewSelection.Add(QuestBeingEdited);
	}

	// View the selected objects
	if (DetailsView.IsValid())
	{
		DetailsView->SetObjects(ViewSelection, /*bForceRefresh=*/ true);
	}

}

FActionMenuContent FEGQuestEditor::OnCreateGraphActionMenu(
	UEdGraph* InGraph,
	const FNYVector2f& InNodePosition,
	const TArray<UEdGraphPin*>& InDraggedPins,
	bool bAutoExpand,
	SGraphEditor::FActionMenuClosed InOnMenuClosed
)
{
	TSharedRef<SEGQuestGraphActionMenu> ActionMenu = SNew(SEGQuestGraphActionMenu)
		.Graph(InGraph)
		.NewNodePosition(InNodePosition)
		.DraggedFromPins(InDraggedPins)
		.AutoExpandActionMenu(bAutoExpand)
		.OnClosedCallback(InOnMenuClosed)
		.OnCloseReason(this, &Self::OnGraphActionMenuClosed);

	return FActionMenuContent(ActionMenu, ActionMenu->GetFilterTextBox());
}

void FEGQuestEditor::OnGraphActionMenuClosed(bool bActionExecuted, bool bGraphPinContext)
{
}

void FEGQuestEditor::OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged) const
{
	if (!IsValid(NodeBeingChanged))
	{
		return;
	}

	// Rename the node to the new set text.
	const FScopedTransaction Transaction(LOCTEXT("RenameNode", "Quest Editor: Rename Node"));
	verify(NodeBeingChanged->Modify());
	NodeBeingChanged->OnRenameNode(NewText.ToString());
}

// End of own functions
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
