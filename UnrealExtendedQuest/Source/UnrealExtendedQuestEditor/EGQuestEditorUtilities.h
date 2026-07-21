// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "ConnectionDrawingPolicy.h"
#include "EdGraphNode_Comment.h"

#include "Editor/Graph/EGQuestEdGraph.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode.h"
#include "UnrealExtendedQuest/NYEngineVersionHelpers.h"
#include "Subsystems/AssetEditorSubsystem.h"

enum class EEGQuestBlueprintOpenType : uint8
{
	None = 0,
	Function,
	Event
};

//////////////////////////////////////////////////////////////////////////
// FEGQuestEditorUtilities

class UEGQuestGraph;
class UEdGraphSchema;
class UEGQuestNode;
class UEdGraph;
class FSlateRect;
class UK2Node_Event;
class IAssetEditorInstance;

class UNREALEXTENDEDQUESTEDITOR_API FEGQuestEditorUtilities
{
public:
	/** Spawns a GraphNode in the specified ParentGraph and at Location. */
	template <typename GraphNodeType>
	static GraphNodeType* SpawnGraphNodeFromTemplate(UEdGraph* ParentGraph, const FIntPoint& Location, bool bSelectNewNode = true)
	{
		FGraphNodeCreator<GraphNodeType> NodeCreator(*ParentGraph);
		GraphNodeType* GraphNode = NodeCreator.CreateUserInvokedNode(bSelectNewNode);
		NodeCreator.Finalize(); // Calls on the node: CreateNewGuid, PostPlacedNewNode, AllocateDefaultPins
		GraphNode->NodePosX = Location.X;
		GraphNode->NodePosY = Location.Y;
		GraphNode->SetFlags(RF_Transactional);

		return GraphNode;
	}

	// Loads all quests into memory and checks the GUIDs for duplicates
	static void LoadAllQuestsAndCheckGUIDs();

	/** Gets the nodes that are currently selected */
	static const TSet<UObject*> GetSelectedNodes(const UEdGraph* Graph);

	/** Get the bounding area for the currently selected nodes
	 *
	 * @param Graph The Graph we are finding bounds for
	 * @param Rect Final output bounding area, including padding
	 * @param Padding An amount of padding to add to all sides of the bounds
	 *
	 * @return false if nothing is selected
	 */
	static bool GetBoundsForSelectedNodes(const UEdGraph* Graph, FSlateRect& Rect, float Padding = 0.0f);

	/** Refreshes the details panel for the editor of the specified Graph. */
	static void RefreshDetailsView(const UEdGraph* Graph, bool bRestorePreviousSelection);

	// Refresh the viewport and property/details pane
	static void Refresh(const UEdGraph* Graph, bool bRestorePreviousSelection);

	/** Helper function to remove the provided node from it's graph. Returns true on success, false otherwise. */
	static bool RemoveNode(UEdGraphNode* NodeToRemove);

	/**
	 * Creates a new empty graph.
	 *
	 * @param	ParentScope		The outer of the new graph (typically a blueprint).
	 * @param	GraphName		Name of the graph to add.
	 * @param	SchemaClass		Schema to use for the new graph.
	 *
	 * @return	nullptr if it fails, else ther new created graph
	 */
	static UEdGraph* CreateNewGraph(
		UObject* ParentScope,
		FName GraphName,
		TSubclassOf<UEdGraph> GraphClass,
		TSubclassOf<UEdGraphSchema> SchemaClass
	);

	/** Helper function that checks if the data is valid in the Quest/Graph and tries to fix the data. */
	static bool CheckAndTryToFixQuest(UEGQuestGraph* Quest, bool bDisplayWarning = true);

	/**
	 * Tries to create the default graph for the Quest if the number of nodes differ from the quest data and the graph data
	 *
	 * @param Quest		The Quest we want to create the default graph for.
	 * @param bPrompt		Indicates if we should prompt the user for a response.
	 */
	static void TryToCreateDefaultGraph(UEGQuestGraph* Quest, bool bPrompt = true);

	/** Tells us if the number of quest nodes matches with the number of graph nodes (corresponding to quests). */
	static bool AreQuestNodesInSyncWithGraphNodes(const UEGQuestGraph* Quest);

	// Tries to get the closest UEGQuestNode for a  UEdGraphNode
	static UEGQuestNode* GetClosestNodeFromGraphNode(UEdGraphNode* GraphNode);

	/** Gets the Quest from the Graph */
	static UEGQuestGraph* GetQuestForGraph(const UEdGraph* Graph)
	{
		return CastChecked<UEGQuestEdGraph>(Graph)->GetQuest();
	}

	/**
	 * Automatically reposition all the nodes in the graph.
	 *
	 * @param	RootNode				The Node that is considered the node
	 * @param	GraphNodes				The rest of the graph nodes
	 * @param	OffsetBetweenColumnsX   The offset between nodes on the X axis
	 * @param	OffsetBetweenRowsY		The offset between nodes on the Y axis
	 * @param	bIsDirectionVertical	Is direction vertical? If false it is horizontal
	 */
	static void AutoPositionGraphNodes(
		UEGQuestGraphNode* RootNode,
		const TArray<UEGQuestGraphNode*>& GraphNodes,
		int32 OffsetBetweenColumnsX,
		int32 OffsetBetweenRowsY,
		bool bIsDirectionVertical
	);

	/** Close any editor which is not this one */
	static void CloseOtherEditors(UObject* Asset, IAssetEditorInstance* OnlyEditor);

	/**
	 * Tries to open the editor for the specified asset. Returns true if the asset is opened in an editor.
	 * If the file is already open in an editor, it will not create another editor window but instead bring it to front
	 */
	static bool OpenEditorForAsset(const UObject* Asset);

	/** Returns the primary editor if one is already open for the specified asset.
	 * If there is one open and bFocusIfOpen is true, that editor will be brought to the foreground and focused if possible.
	 */
	static IAssetEditorInstance* FindEditorForAsset(UObject* Asset, bool bFocusIfOpen);

	/**
	 * Tries to open an Quest editor for the GraphNode and jumps to it. Returns true if the asset is opened in an editor.
	 * If the file is already open in an editor, it will not create another editor window but instead bring it to front
	 */
	static bool OpenEditorAndJumpToGraphNode(const UEdGraphNode* GraphNode, bool bFocusIfOpen = false);

	// Just jumps to that graph node without trying to open any Quest Editor
	// If you want that just call OpenEditorAndJumpToGraphNode
	static bool JumpToGraphNode(const UEdGraphNode* GraphNode);
	static bool JumpToGraphNodeIndex(const UEGQuestGraph* Quest, int32 NodeIndex);

	// Wrapper over standard message box that that also logs to the console
	static EAppReturnType::Type ShowMessageBox(EAppMsgType::Type MsgType, const FString& Text, const FString& Caption);

	// Returns true if the TestPoint is inside the Geometry.
	static bool IsPointInsideGeometry(const FNYVector2f& TestPoint, const FGeometry& Geometry)
	{
		TArray<FNYVector2f> GeometryPoints;
		FGeometryHelper::ConvertToPoints(Geometry, GeometryPoints);
		return FNYBox2f(GeometryPoints).IsInside(TestPoint);
	}

	// Gets the Quest for the provided UEdGraphNode
	static UEGQuestGraph* GetQuestFromGraphNode(const UEdGraphNode* GraphNode);

	// Save all the quests.
	// @return True on success or false on failure.
	static bool SaveAllQuests();

	// Deletes all teh quests text files
	// @return True on success or false on failure.
	static bool DeleteAllQuestsTextFiles();

	/***
	* Pops up a class picker dialog to choose the class that is a child of the Classprovided.
	*
	* @param	TitleText		The title of the class picker dialog
	* @param	OutChosenClass  The class chosen (if this function returns false, this will be null) by the the user
	* @param	Class			The children of this class we are displaying and prompting the user to choose from.
	*
	* @return true if OK was pressed, false otherwise
	*/
	static bool PickChildrenOfClass(const FText& TitleText, UClass*& OutChosenClass, UClass* Class);

	// Opens the specified Blueprint at the last edited graph by default
	// or if the OpenType is set to Function or Event it opens that with the FunctionNameToOpen
	static bool OpenBlueprintEditor(
		UBlueprint* Blueprint,
		EEGQuestBlueprintOpenType OpenType = EEGQuestBlueprintOpenType::None,
		FName FunctionNameToOpen = NAME_None,
		bool bForceFullEditor = true,
		bool bAddBlueprintFunctionIfItDoesNotExist = false
	);

	// Adds the function if it does not exist
	// Return the Function Graph of the existing function or the newly created one
	static UEdGraph* BlueprintGetOrAddFunction(UBlueprint* Blueprint, FName FunctionName, UClass* FunctionClassSignature);

	// Same as BlueprintGetOrAddFunction but does not add it
	static UEdGraph* BlueprintGetFunction(UBlueprint* Blueprint, FName FunctionName, UClass* FunctionClassSignature);

	// Same as BlueprintGetOrAddFunction but only for an overriden event
	static UK2Node_Event* BlueprintGetOrAddEvent(UBlueprint* Blueprint, FName EventName, UClass* EventClassSignature);

	// Same as BlueprintGetOrAddEvent but does not add it
	static UK2Node_Event* BlueprintGetEvent(UBlueprint* Blueprint, FName EventName, UClass* EventClassSignature);

	// Adds a comment to the Blueprint
	static UEdGraphNode_Comment* BlueprintAddComment(UBlueprint* Blueprint, const FString& CommentString, FNYVector2f Location = FNYVector2f::ZeroVector);

	static void RefreshQuestEditorForGraph(const UEdGraph* Graph);

private:
	// Get the QuestEditor for given object, if it exists
	static TSharedPtr<class IEGQuestEditor> GetQuestEditorForGraph(const UEdGraph* Graph);

	FEGQuestEditorUtilities() = delete;
};
