// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "GraphEditor.h"
#include "EditorUndoClient.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "IDetailsView.h"
#include "Misc/NotifyHook.h"

#include "UnrealExtendedQuestEditor/Editor/Graph/EGQuestEdGraph.h"
#include "UnrealExtendedQuestEditor/Editor/IEGQuestEditor.h"
#include "UnrealExtendedQuestEditor/EGQuestEditorUtilities.h"
#include "UnrealExtendedQuest/EGQuestGraph.h"
#include "UnrealExtendedQuest/EGQuestPluginSettings.h"
#include "UnrealExtendedQuest/NYEngineVersionHelpers.h"

class IDetailsView;
class UEGQuestGraph;
class SEGQuestFindInQuests;
class FTabManager;

//////////////////////////////////////////////////////////////////////////
// FEGQuestEditor
class UNREALEXTENDEDQUESTEDITOR_API FEGQuestEditor : public IEGQuestEditor, public FGCObject, public FNotifyHook, public FEditorUndoClient
{
	typedef FEGQuestEditor Self;

public:
	SLATE_BEGIN_ARGS(Self) {}
	SLATE_END_ARGS()

	FEGQuestEditor();
	virtual ~FEGQuestEditor();

	//
	// IToolkit interface
	//

	void RegisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
	void UnregisterTabSpawners(const TSharedRef<FTabManager>& TabManager) override;
	FText GetBaseToolkitName() const override;
	FText GetToolkitName() const override;
	FName GetToolkitFName() const override { return FName(TEXT("QuestEditor")); }
	FText GetToolkitToolTipText() const override { return GetToolTipTextForObject(Cast<UObject>(QuestBeingEdited)); }
	FLinearColor GetWorldCentricTabColorScale() const override { return FLinearColor::White; }
	FString GetWorldCentricTabPrefix() const override { return FString(TEXT("QuestEditor")); }

	//
	// FAssetEditorToolkit interface
	//

	/** @return the documentation location for this editor */
	FString GetDocumentationLink() const override { return FString(TEXT("Plugins/UnrealExtendedGameplay/QuestEditor")); }

	bool CanSaveAsset() const override { return true; }
	bool CanSaveAssetAs() const override { return true; }
	void SaveAsset_Execute() override;
	void SaveAssetAs_Execute() override;

	//
	// IAssetEditorInstance interface
	//

	void FocusWindow(UObject* ObjectToFocusOn = nullptr) override;

	//
	// FEditorUndoClient interface
	//

	// Signal that client should run any PostUndo code
	// @param bSuccess	If true than undo succeeded, false if undo failed
	void PostUndo(bool bSuccess) override;

	// Signal that client should run any PostRedo code
	// @param bSuccess	If true than redo succeeded, false if redo failed
	void PostRedo(bool bSuccess) override;

	// Undo/redo restored some mix of graph and runtime data; recompiling makes them agree again.
	void HandleTransactionReverted();

	//
	// FSerializableObject interface, FGCObject
	//

	void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		Collector.AddReferencedObject(QuestBeingEdited);
	}

#if NY_ENGINE_VERSION >= 500
	virtual FString GetReferencerName() const override
	{
		return TEXT("FEGQuestEditor");
	}
#endif


	//
	// IEGQuestEditor interface
	//

	// Get the currently selected set of nodes
	TSet<UObject*> GetSelectedNodes() const override
	{
		check(GraphEditorView.IsValid());
		return GraphEditorView->GetSelectedNodes();
	}

	/**
	 * Get the bounding area for the currently selected nodes
	 *
	 * @param Rect Final output bounding area, including padding
	 * @param Padding An amount of padding to add to all sides of the bounds
	 *
	 * @return false if nothing is selected
	 */
	bool GetBoundsForSelectedNodes(class FSlateRect& Rect, float Padding) const override
	{
		return GraphEditorView->GetBoundsForSelectedNodes(Rect, Padding);
	}

	// Clears the viewport selection set
	void ClearViewportSelection() const
	{
		if (GraphEditorView.IsValid())
		{
			GraphEditorView->ClearSelectionSet();
		}
	}

	// Refreshes the graph viewport.
	void RefreshViewport() const
	{
		if (GraphEditorView.IsValid())
		{
			GraphEditorView->NotifyGraphChanged();
		}
	}

	// Refreshes the details panel with the Quest
	void RefreshDetailsView(bool bRestorePreviousSelection) override;
	void Refresh(bool bRestorePreviousSelection) override;
	void JumpToObject(const UObject* Object) override;

	//
	// Own methods
	//

	// Summons The Search UI
	void SummonSearchUI(bool bSetFindWithinQuest, FString NewSearchTerms = FString(), bool bSelectFirstResult = false);

	// Edits the specified Quest. This is called from the TypeActions object.
	void InitQuestEditor(
		EToolkitMode::Type Mode,
		const TSharedPtr<IToolkitHost>& InitToolkitHost,
		UEGQuestGraph* InitQuest
	);

	// Helper method to get directly the Quest Graph
	UEGQuestEdGraph* GetQuestEdGraph() const { return CastChecked<UEGQuestEdGraph>(QuestBeingEdited->GetGraph()); }

	// Gets/Sets the quest being edited
	UEGQuestGraph* GetQuestBeingEdited()
	{
		check(QuestBeingEdited);
		return QuestBeingEdited;
	}
	void SetQuestBeingEdited(UEGQuestGraph* NewQuest);

private:
	//
	// FNotifyHook interface
	//
	void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;

	//
	// Own methods
	//

	// Creates all internal widgets for the tabs to point at
	void CreateInternalWidgets();

	// Create new graph editor widget
	TSharedRef<SGraphEditor> CreateGraphEditorWidget();

	// Bind the commands from the editor. See GraphEditorCommands for more details.
	void BindEditorCommands();

	// Extend the Menus of the editor
	void ExtendMenu();

	// Extends the Top Toolbar
	void ExtendToolbar();

	//
	// Functions to spawn each tab type.
	//

	// Spawn the details tab where we can see the internal structure of the quest.
	TSharedRef<SDockTab> SpawnTab_Details(const FSpawnTabArgs& Args) const;

	// Spawn the main graph canvas where all the nodes live. */
	TSharedRef<SDockTab> SpawnTab_GraphCanvas(const FSpawnTabArgs& Args) const;

	// Spawn the find in Quest tab view.
	TSharedRef<SDockTab> SpawnTab_FindInQuest(const FSpawnTabArgs& Args) const;

	//
	// Graph editor commands
	//

	// Called to undo the last action
	void OnCommandUndoGraphAction() const;

	// Called to redo the last undone action
	void OnCommandRedoGraphAction() const;

	//
	// Edit Node commands
	//

	// Converts a objective sequence node to a list of objective node.

	// Converts a list of objective nodes to a objective sequence node

	// Remove the currently selected nodes from editor view
	void OnCommandDeleteSelectedNodes() const;

	// Whether we are able to remove the currently selected nodes
	bool CanDeleteNodes() const;

	// Copy the currently selected nodes to the text buffer.
	void OnCommandCopySelectedNodes() const;

	// Whether we are able to copy the currently selected nodes.
	bool CanCopyNodes() const;

	// Copy the currently selected nodes and remove them.
	void OnCommandCutSelectedNodes() const;
	bool CanCutNodes() const;

	// Copy and immediately paste the selection, offset from the originals.
	void OnCommandDuplicateNodes();

	// Paste the nodes at the current location
	void OnCommandPasteNodes();

	// Paste the nodes at the specified Location.
	void PasteNodesHere(const FNYVector2f& Location);

	// Whether we are able to paste from the clipboard
	bool CanPasteNodes() const;

	//
	// Toolbar commands
	//

	// Compiles the graph into the runtime quest data
	void OnCommandQuestCompile() const;

	// Opens (creating on demand) the quest's script Blueprint
	void OnCommandOpenQuestScript();

	//
	// Graph events
	//

	// Called when the selection changes in the GraphEditor
	void OnSelectedNodesChanged(const TSet<UObject*>& NewSelection);

	// Called to create context menu when right-clicking on graph
	FActionMenuContent OnCreateGraphActionMenu(UEdGraph* InGraph,
		const FNYVector2f& InNodePosition,
		const TArray<UEdGraphPin*>& InDraggedPins,
		bool bAutoExpand,
		SGraphEditor::FActionMenuClosed InOnMenuClosed);

	// Called from graph context menus when they close to tell the editor why they closed
	void OnGraphActionMenuClosed(bool bActionExecuted, bool bGraphPinContext);

	/**
	 * Called when a node's title is committed for a rename
	 *
	 * @param	NewText				New title text
	 * @param	CommitInfo			How text was committed
	 * @param	NodeBeingChanged	The node being changed
	 */
	void OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged) const;

	/** Called when the preview text changes */
//	void OnPreviewTextChanged(const FString& Text);
//
//	/** Called when the selection changes in the GraphEditor */
//	void OnSelectedNodesChanged(const TSet<class UObject*>& NewSelection);

	const UEGQuestPluginSettings& GetSettings() const { return *Settings; }

	/** Selected proxy nodes highlights their targets */

	// The quest we are currently editing
	TObjectPtr<UEGQuestGraph> QuestBeingEdited;

	// The quest system settings
	UEGQuestPluginSettings* Settings = nullptr;

	// Graph Editor
	TSharedPtr<SGraphEditor> GraphEditorView;

	// The custom details view used
	TSharedPtr<IDetailsView> DetailsView;

	/** Find results log as well as the search filter */
	TSharedPtr<SEGQuestFindInQuests> FindResultsView;

	// Command list for this editor. Synced with FEGQuestEditorCommands. Aka list of shortcuts supported.
	TSharedPtr<FUICommandList> GraphEditorCommands;

	// Keep track of the previous selected objects so that we can reverse selection
	TArray<TWeakObjectPtr<UObject>> PreviousSelectedNodeObjects;

	/**	The tab ids for all the tabs used */
	static const FName DetailsTabID;
	static const FName GraphCanvasTabID;
	static const FName FindInQuestTabId;
};
