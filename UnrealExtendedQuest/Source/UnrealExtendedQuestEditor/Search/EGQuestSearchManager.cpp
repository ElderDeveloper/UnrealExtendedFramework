// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestSearchManager.h"

#include "Widgets/Docking/SDockTab.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EGQuestSearchUtilities.h"
#include "WorkspaceMenuStructureModule.h"
#include "WorkspaceMenuStructure.h"
#include "EdGraphNode_Comment.h"
#include "Runtime/Launch/Resources/Version.h"

#include "UnrealExtendedQuest/EGQuestGraph.h"
#include "UnrealExtendedQuest/EGQuestManager.h"
#include "UnrealExtendedQuest/EGQuestHelper.h"
#include "SEGQuestFindInQuests.h"
#include "UnrealExtendedQuestEditor/Editor/Graph/EGQuestEdGraph.h"
#include "UnrealExtendedQuestEditor/Editor/Nodes/EGQuestGraphNode.h"
#include "UnrealExtendedQuestEditor/EGQuestStyle.h"
#include "UnrealExtendedQuest/EGQuestConstants.h"

#define LOCTEXT_NAMESPACE "SEGQuestBrowser"

FEGQuestSearchManager* FEGQuestSearchManager::Instance = nullptr;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FEGQuestSearchManager
FEGQuestSearchManager* FEGQuestSearchManager::Get()
{
	if (Instance == nullptr)
	{
		Instance = new Self();
	}

	return Instance;
}

FEGQuestSearchManager::FEGQuestSearchManager()
{
	// Create the Tab Ids
	for (int32 TabIdx = 0; TabIdx < NY_ARRAY_COUNT(GlobalFindResultsTabIDs); TabIdx++)
	{
		const FName TabID = FName(*FString::Printf(TEXT("GlobalQuestFindResults_%02d"), TabIdx + 1));
		GlobalFindResultsTabIDs[TabIdx] = TabID;
	}
}

FEGQuestSearchManager::~FEGQuestSearchManager()
{
	UnInitialize();
}

bool FEGQuestSearchManager::QueryQuestTextArgument(
	const FEGQuestSearchFilter& SearchFilter,
	const FEGQuestTextArgument& InQuestTextArgument,
	const TSharedPtr<FEGQuestSearchResult>& OutParentNode,
	int32 ArgumentIndex
)
{
	if (SearchFilter.SearchString.IsEmpty() || !OutParentNode.IsValid())
	{
		return false;
	}
	bool bContainsSearchString = false;

	// Test DisplayString
	if (InQuestTextArgument.DisplayString.Contains(SearchFilter.SearchString))
	{
		bContainsSearchString = true;
		const FText Category = FText::Format(
			LOCTEXT("QuestTextArgumentDisplayString", "TextArgument.DisplayString at index = {0}"),
			FText::AsNumber(ArgumentIndex)
		);
		MakeChildTextNode(
			OutParentNode,
			FText::FromString(InQuestTextArgument.DisplayString),
			Category,
			Category.ToString()
		);
	}

	if (SearchFilter.bIncludeCustomObjectNames)
	{
		// Test CustomTextArgument
		FString FoundName;
		if (FEGQuestSearchUtilities::DoesObjectClassNameContainString(InQuestTextArgument.CustomTextArgument, SearchFilter.SearchString, FoundName))
		{
			bContainsSearchString = true;
			const FText Category = FText::Format(
				LOCTEXT("QuestTextArgumentCustomTextArgument", "TextArgument.CustomTextArgument at index = {0}"),
				FText::AsNumber(ArgumentIndex)
			);
			MakeChildTextNode(
				OutParentNode,
				FText::FromString(FoundName),
				Category,
				Category.ToString()
			);
		}
	}

	return bContainsSearchString;
}

bool FEGQuestSearchManager::QueryQuestObjective(
	const FEGQuestSearchFilter& SearchFilter,
	const UEGQuestNode_Objective* InObjective,
	const TSharedPtr<FEGQuestSearchResult>& OutParentNode
)
{
	if (SearchFilter.SearchString.IsEmpty() || !OutParentNode.IsValid() || InObjective == nullptr)
	{
		return false;
	}
	bool bContainsSearchString = false;

	auto AddResult = [&](const FString& Field, const FString& Value)
	{
		bContainsSearchString = true;
		const FText Category = FText::Format(
			LOCTEXT("QuestObjectiveField", "Objective.{0}"),
			FText::FromString(Field)
		);
		MakeChildTextNode(OutParentNode, FText::FromString(Value), Category, Category.ToString());
	};

	if (SearchFilter.bIncludeCustomObjectNames)
	{
		FString FoundName;
		if (FEGQuestSearchUtilities::DoesObjectClassNameContainString(InObjective, SearchFilter.SearchString, FoundName))
		{
			AddResult(TEXT("Class"), FoundName);
		}
	}

	// Each objective class reports its own searchable fields; the filter decides which kinds count.
	TArray<FEGQuestSearchTerm> Terms;
	InObjective->GetSearchTerms(Terms);
	for (const FEGQuestSearchTerm& Term : Terms)
	{
		switch (Term.Kind)
		{
		case EEGQuestSearchTermKind::Number:
			if (!SearchFilter.bIncludeNumericalTypes) continue;
			break;
		case EEGQuestSearchTermKind::NodeGUID:
			if (!SearchFilter.bIncludeNodeGUID) continue;
			break;
		default:
			break;
		}

		// GUIDs match against any of their formats, so let the GUID helper do that comparison.
		if (Term.Kind == EEGQuestSearchTermKind::NodeGUID)
		{
			FGuid Guid;
			FString FoundGUID;
			if (FGuid::Parse(Term.Value, Guid) &&
				FEGQuestSearchUtilities::DoesGUIDContainString(Guid, SearchFilter.SearchString, FoundGUID))
			{
				AddResult(Term.Label, FoundGUID);
			}
		}
		else if (Term.Value.Contains(SearchFilter.SearchString))
		{
			AddResult(Term.Label, Term.Value);
		}
	}

	return bContainsSearchString;
}

bool FEGQuestSearchManager::QueryQuestEvent(
	const FEGQuestSearchFilter& SearchFilter,
	const UEGQuestEventCustom* InQuestEvent,
	const TSharedPtr<FEGQuestSearchResult>& OutParentNode,
	int32 EventIndex,
	FName EventMemberName
)
{
	if (SearchFilter.SearchString.IsEmpty() || !OutParentNode.IsValid() || InQuestEvent == nullptr)
	{
		return false;
	}
	bool bContainsSearchString = false;

	auto AddResult = [&](const FString& Field, const FString& Value)
	{
		bContainsSearchString = true;
		const FText Category = FText::Format(
			LOCTEXT("QuestEventField", "{0}.{1} at index = {2}"),
			FText::FromName(EventMemberName), FText::FromString(Field), FText::AsNumber(EventIndex)
		);
		MakeChildTextNode(OutParentNode, FText::FromString(Value), Category, Category.ToString());
	};

	if (SearchFilter.bIncludeCustomObjectNames)
	{
		FString FoundName;
		if (FEGQuestSearchUtilities::DoesObjectClassNameContainString(InQuestEvent, SearchFilter.SearchString, FoundName))
		{
			AddResult(TEXT("Event"), FoundName);
		}
	}

	// Each event class reports its own searchable fields; the filter decides which kinds count.
	TArray<FEGQuestSearchTerm> Terms;
	InQuestEvent->GetSearchTerms(Terms);
	for (const FEGQuestSearchTerm& Term : Terms)
	{
		if (Term.Kind == EEGQuestSearchTermKind::Number && !SearchFilter.bIncludeNumericalTypes)
		{
			continue;
		}
		if (Term.Value.Contains(SearchFilter.SearchString))
		{
			AddResult(Term.Label, Term.Value);
		}
	}

	return bContainsSearchString;
}

bool FEGQuestSearchManager::QueryGraphNode(
	const FEGQuestSearchFilter& SearchFilter,
	const UEGQuestGraphNode* InGraphNode,
	const TSharedPtr<FEGQuestSearchResult>& OutParentNode
)
{
	if (SearchFilter.SearchString.IsEmpty() || !OutParentNode.IsValid() || !IsValid(InGraphNode))
	{
		return false;
	}

	bool bContainsSearchString = false;
	const UEGQuestNode& Node = InGraphNode->GetQuestNode();
	const int32 NodeIndex = InGraphNode->GetQuestNodeIndex();
	const FString NodeType = Node.GetNodeTypeString();

	// Create the GraphNode Node
	const FText DisplayText = FText::Format(
		LOCTEXT("TreeGraphNodeCategory", "{0} Node at index {1}"),
		FText::FromString(NodeType), FText::AsNumber(NodeIndex)
	);
	TSharedPtr<FEGQuestSearchResult_GraphNode> TreeGraphNode = MakeShared<FEGQuestSearchResult_GraphNode>(DisplayText, OutParentNode);
	TreeGraphNode->SetCategory(FText::FromString(NodeType));
	TreeGraphNode->SetGraphNode(InGraphNode);

	// Test the NodeIndex
	if (SearchFilter.bIncludeIndices && !InGraphNode->IsRootNode())
	{
		// NOTE: We do not create another node, we just use the Node DisplayText as the search node.
		if (FString::FromInt(NodeIndex).Contains(SearchFilter.SearchString))
		{
			bContainsSearchString = true;
		}
	}

	// Test the Node Comment
	if (SearchFilter.bIncludeComments)
	{
		if (InGraphNode->NodeComment.Contains(SearchFilter.SearchString))
		{
			bContainsSearchString = true;
			MakeChildTextNode(
				TreeGraphNode,
				FText::FromString(InGraphNode->NodeComment),
				LOCTEXT("NodeCommentKey", "Comment on Node"),
				TEXT("Comment on Node")
			);
		}
	}

	// Test the Node text
	if (Node.GetNodeText().ToString().Contains(SearchFilter.SearchString))
	{
		bContainsSearchString = true;
		MakeChildTextNode(
			TreeGraphNode,
			Node.GetNodeText(),
			LOCTEXT("DescriptionKey", "Text"),
			TEXT("Text")
		);
	}
	// Test the Node Text Data
	if (SearchFilter.bIncludeTextLocalizationData)
	{
		bContainsSearchString = SearchForTextLocalizationData(
			TreeGraphNode,
			SearchFilter.SearchString, Node.GetNodeText(),
			LOCTEXT("TextNamespaceName_Found", "Text Namespace"), TEXT("Text Localization Namespace"),
			LOCTEXT("TextKey_Found", "Text Key"), TEXT("Text Localization Key")
		) || bContainsSearchString;
	}

	// Test an objective's evaluation fields (event tags, counts, class).
	if (const UEGQuestNode_Objective* Objective = Cast<UEGQuestNode_Objective>(&Node))
	{
		bContainsSearchString = QueryQuestObjective(SearchFilter, Objective, TreeGraphNode) || bContainsSearchString;
	}

	// A stage card's objective rows are not graph nodes of their own: search them as part of the card.
	for (const UEGQuestNode_Objective* Objective : InGraphNode->GetObjectives())
	{
		if (!Objective)
		{
			continue;
		}
		if (Objective->GetNodeText().ToString().Contains(SearchFilter.SearchString))
		{
			bContainsSearchString = true;
			MakeChildTextNode(
				TreeGraphNode,
				Objective->GetNodeText(),
				LOCTEXT("ObjectiveTextKey", "Objective Text"),
				TEXT("Objective Text")
			);
		}
		bContainsSearchString = QueryQuestObjective(SearchFilter, Objective, TreeGraphNode) || bContainsSearchString;
	}

	// Test the EnterEvents
	const TArray<TObjectPtr<UEGQuestEventCustom>>& EnterEvents = Node.GetNodeEnterEvents();
	for (int32 Index = 0, Num = EnterEvents.Num(); Index < Num; Index++)
	{
		bContainsSearchString = QueryQuestEvent(
			SearchFilter,
			EnterEvents[Index],
			TreeGraphNode,
			Index,
			TEXT("EnterEvent")
		) || bContainsSearchString;
	}

	// Test TextArguments
	const TArray<FEGQuestTextArgument>& TextArguments = Node.GetTextArguments();
	for (int32 Index = 0, Num = TextArguments.Num(); Index < Num; Index++)
	{
		bContainsSearchString = QueryQuestTextArgument(SearchFilter, TextArguments[Index], TreeGraphNode, Index) || bContainsSearchString;
	}

	if (SearchFilter.bIncludeNodeGUID)
	{
		// Test Node GUID
		FString FoundGUID;
		if (FEGQuestSearchUtilities::DoesGUIDContainString(Node.GetGUID(), SearchFilter.SearchString, FoundGUID))
		{
			bContainsSearchString = true;
			MakeChildTextNode(
				TreeGraphNode,
				FText::FromString(FoundGUID),
				LOCTEXT("NodeGUID", "Node GUID"),
				TEXT("Node GUID")
			);
		}
	}

	if (bContainsSearchString)
	{
		OutParentNode->AddChild(TreeGraphNode);
	}

	return bContainsSearchString;
}

bool FEGQuestSearchManager::QueryCommentNode(
	const FEGQuestSearchFilter& SearchFilter,
	const UEdGraphNode_Comment* InCommentNode,
	const TSharedPtr<FEGQuestSearchResult>& OutParentNode
)
{
	if (!SearchFilter.bIncludeComments || SearchFilter.SearchString.IsEmpty() || !OutParentNode.IsValid() || !IsValid(InCommentNode))
	{
		return false;
	}

	if (InCommentNode->NodeComment.Contains(SearchFilter.SearchString))
	{
		const FText Category = LOCTEXT("TreeNodeCommentCategory", "Comment Node");
		TSharedPtr<FEGQuestSearchResult_CommentNode> TreeCommentNode = MakeShared<FEGQuestSearchResult_CommentNode>(Category, OutParentNode);
		TreeCommentNode->SetCategory(Category);
		TreeCommentNode->SetCommentNode(InCommentNode);

		MakeChildTextNode(
			TreeCommentNode,
			FText::FromString(InCommentNode->NodeComment),
			Category,
			TEXT("")
		);

		OutParentNode->AddChild(TreeCommentNode);
		return true;
	}
	return false;
}

bool FEGQuestSearchManager::QuerySingleQuest(
	const FEGQuestSearchFilter& SearchFilter,
	const UEGQuestGraph* InQuest,
	TSharedPtr<FEGQuestSearchResult>& OutParentNode
)
{
	if (SearchFilter.SearchString.IsEmpty() || !OutParentNode.IsValid() || !IsValid(InQuest))
	{
		return false;
	}

	const UEGQuestEdGraph* Graph = CastChecked<UEGQuestEdGraph>(InQuest->GetGraph());
	TSharedPtr<FEGQuestSearchResult_QuestNode> TreeQuestNode = MakeShared<FEGQuestSearchResult_QuestNode>(
			FText::FromString(InQuest->GetPathName()), OutParentNode
	);
	TreeQuestNode->SetQuest(InQuest);

	// Find in GraphNodes
	bool bFoundInQuest = false;
	const TArray<UEdGraphNode*>& AllGraphNodes = Graph->GetAllGraphNodes();
	for (UEdGraphNode* Node : AllGraphNodes)
	{
		bool bFoundInNode = false;
		if (UEGQuestGraphNode* GraphNode = Cast<UEGQuestGraphNode>(Node))
		{
			bFoundInNode = QueryGraphNode(SearchFilter, GraphNode, TreeQuestNode);
		}
		else if (UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(Node))
		{
			bFoundInNode = QueryCommentNode(SearchFilter, CommentNode, TreeQuestNode);
		}
		else
		{
			// ignore everything else
		}

		// Found at least one match in one of the nodes.
		bFoundInQuest = bFoundInNode || bFoundInQuest;
	}

	// Search for GUID
	if (SearchFilter.bIncludeQuestGUID)
	{
		FString FoundGUID;
		if (FEGQuestSearchUtilities::DoesGUIDContainString(InQuest->GetGUID(), SearchFilter.SearchString, FoundGUID))
		{
			bFoundInQuest = true;
			MakeChildTextNode(
				TreeQuestNode,
				FText::FromString(FoundGUID),
				LOCTEXT("QuestGUID", "Quest GUID"),
				TEXT("Quest.GUID")
			);
		}
	}

	if (bFoundInQuest)
	{
		OutParentNode->AddChild(TreeQuestNode);
	}

	return bFoundInQuest;
}

void FEGQuestSearchManager::QueryAllQuests(
	const FEGQuestSearchFilter& SearchFilter,
	TSharedPtr<FEGQuestSearchResult>& OutParentNode
)
{
	// Iterate over all cached quests
	for (auto& Elem : SearchMap)
	{
		const FEGQuestSearchData& SearchData = Elem.Value;
		if (SearchData.Quest.IsValid())
		{
			QuerySingleQuest(SearchFilter, SearchData.Quest.Get(), OutParentNode);
		}
	}
}

FText FEGQuestSearchManager::GetGlobalFindResultsTabLabel(int32 TabIdx)
{
	// Count the number of opened global Quests
	int32 NumOpenGlobalFindResultsTabs = 0;
	for (int32 i = GlobalFindResultsWidgets.Num() - 1; i >= 0; --i)
	{
		if (GlobalFindResultsWidgets[i].IsValid())
		{
			NumOpenGlobalFindResultsTabs++;
		}
		else
		{
			// Invalid :O remove it
			GlobalFindResultsWidgets.RemoveAt(i);
		}
	}

	if (NumOpenGlobalFindResultsTabs > 1 || TabIdx > 0)
	{
		// Format TabIdx + 1
		return FText::Format(
			LOCTEXT("GlobalFindResultsTabNameWithIndex", "Find in Quests {0}"),
			FText::AsNumber(TabIdx + 1)
		);
	}

	// No Number
	return LOCTEXT("GlobalFindResultsTabName", "Find in Quests");
}

void FEGQuestSearchManager::CloseGlobalFindResults(const TSharedRef<SEGQuestFindInQuests>& FindResults)
{
	for (TWeakPtr<SEGQuestFindInQuests> FindResultsPtr : GlobalFindResultsWidgets)
	{
		// Remove it from the opened find results
		if (FindResultsPtr.Pin() == FindResults)
		{
			GlobalFindResultsWidgets.Remove(FindResultsPtr);
			break;
		}
	}
}

TSharedRef<SDockTab> FEGQuestSearchManager::SpawnGlobalFindResultsTab(const FSpawnTabArgs& SpawnTabArgs, int32 TabIdx)
{
	// Label is Dynamic depending on the number of global tabs
	TAttribute<FText> Label = TAttribute<FText>::Create(
		TAttribute<FText>::FGetter::CreateRaw(this, &Self::GetGlobalFindResultsTabLabel, TabIdx)
	);

	TSharedRef<SDockTab> NewTab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab) // can be docked anywhere
		.Label(Label)
		.ToolTipText(LOCTEXT("GlobalFindResultsTabTooltip", "Search for a string in all Quest assets."));

	TSharedRef<SEGQuestFindInQuests> FindResults = SNew(SEGQuestFindInQuests)
		.bIsSearchWindow(false)
		.ContainingTab(NewTab);

	NewTab->SetContent(FindResults);
	GlobalFindResultsWidgets.Add(FindResults);

	return NewTab;
}

TSharedPtr<SEGQuestFindInQuests> FEGQuestSearchManager::OpenGlobalFindResultsTab()
{
	TSet<FName> OpenGlobalTabIDs;

	// Check all opened global tabs
	for (TWeakPtr<SEGQuestFindInQuests> FindResultsPtr : GlobalFindResultsWidgets)
	{
		TSharedPtr<SEGQuestFindInQuests> FindResults = FindResultsPtr.Pin();
		if (FindResults.IsValid())
		{
			OpenGlobalTabIDs.Add(FindResults->GetHostTabId());
		}
	}

	// Find the unopened tab, that we can open
	for (int32 Idx = 0; Idx < NY_ARRAY_COUNT(GlobalFindResultsTabIDs); ++Idx)
	{
		const FName GlobalTabId = GlobalFindResultsTabIDs[Idx];
		if (!OpenGlobalTabIDs.Contains(GlobalTabId))
		{
			// GlobalTabId is not opened, open it now
			TSharedPtr<SDockTab> NewTab = FEGQuestHelper::InvokeTab(FGlobalTabmanager::Get(), GlobalTabId);
			if (NewTab.IsValid())
			{
				return StaticCastSharedRef<SEGQuestFindInQuests>(NewTab->GetContent());
			}
		}
	}

	return TSharedPtr<SEGQuestFindInQuests>();
}

TSharedPtr<SEGQuestFindInQuests> FEGQuestSearchManager::GetGlobalFindResults()
{
	// Find opened find tab
	TSharedPtr<SEGQuestFindInQuests> FindResultsToUse;
	for (TWeakPtr<SEGQuestFindInQuests> FindResultsPtr : GlobalFindResultsWidgets)
	{
		TSharedPtr<SEGQuestFindInQuests> FindResults = FindResultsPtr.Pin();
		if (FindResults.IsValid())
		{
			FindResultsToUse = FindResults;
			break;
		}
	}

	if (FindResultsToUse.IsValid())
	{
		// found invoke it
		FEGQuestHelper::InvokeTab(FGlobalTabmanager::Get(), FindResultsToUse->GetHostTabId());
	}
	else
	{
		// not found, open a new tab.
		FindResultsToUse = OpenGlobalFindResultsTab();
	}

	return FindResultsToUse;
}

void FEGQuestSearchManager::EnableGlobalFindResults(TSharedPtr<FWorkspaceItem> ParentTabCategory)
{
	const TSharedRef<FGlobalTabmanager>& GlobalTabManager = FGlobalTabmanager::Get();

	// Register the spawners for all global Find Results tabs
	const FSlateIcon GlobalFindResultsIcon(FEGQuestStyle::GetStyleSetName(), FEGQuestStyle::PROPERTY_QuestSearch_TabIcon);

	// Add the menu item
	if (!ParentTabCategory.IsValid())
	{
		ParentTabCategory = WorkspaceMenu::GetMenuStructure().GetToolsCategory();
	}

	GlobalFindResultsMenuItem = ParentTabCategory->AddGroup(
		LOCTEXT("WorkspaceMenu_GlobalFindResultsCategory", "Find in Quests"),
		LOCTEXT("GlobalFindResultsMenuTooltipText", "Find references to conditions, events, text and variables in all Quests."),
		GlobalFindResultsIcon,
		true
	);

	// Register tab spawners
	for (int32 TabIdx = 0; TabIdx < NY_ARRAY_COUNT(GlobalFindResultsTabIDs); TabIdx++)
	{
		const FName TabID = GlobalFindResultsTabIDs[TabIdx];

		// Tab not registered yet, good.
#if NY_ENGINE_VERSION >= 423
		if (!GlobalTabManager->HasTabSpawner(TabID))
#else
		if (!GlobalTabManager->CanSpawnTab(TabID))
#endif
		{
			const FText DisplayName = FText::Format(
				LOCTEXT("GlobalFindResultsDisplayName", "Find in Quests {0}"),
				FText::AsNumber(TabIdx + 1)
			);
			GlobalTabManager->RegisterNomadTabSpawner(
				TabID,
				FOnSpawnTab::CreateRaw(this, &Self::SpawnGlobalFindResultsTab, TabIdx))
				.SetDisplayName(DisplayName)
				.SetIcon(GlobalFindResultsIcon)
				.SetGroup(GlobalFindResultsMenuItem.ToSharedRef()
			);
		}
	}
}

void FEGQuestSearchManager::DisableGlobalFindResults()
{
	const TSharedRef<FGlobalTabmanager>& GlobalTabManager = FGlobalTabmanager::Get();

	// Close all Global Find Results tabs when turning the feature off, since these may not get closed along with the Blueprint Editor contexts above.
	for (TWeakPtr<SEGQuestFindInQuests> FindResultsPtr : GlobalFindResultsWidgets)
	{
		TSharedPtr<SEGQuestFindInQuests> FindResults = FindResultsPtr.Pin();
		if (FindResults.IsValid())
		{
			FindResults->CloseHostTab();
		}
	}
	GlobalFindResultsWidgets.Empty();

	// Unregister tab spawners
	for (int32 TabIdx = 0; TabIdx < NY_ARRAY_COUNT(GlobalFindResultsTabIDs); TabIdx++)
	{
		const FName TabID = GlobalFindResultsTabIDs[TabIdx];

#if NY_ENGINE_VERSION >= 423
		if (!GlobalTabManager->HasTabSpawner(TabID))
#else
		if (!GlobalTabManager->CanSpawnTab(TabID))
#endif
		{
			GlobalTabManager->UnregisterNomadTabSpawner(TabID);
		}
	}

	// Remove the menu item
	if (GlobalFindResultsMenuItem.IsValid())
	{
		WorkspaceMenu::GetMenuStructure().GetToolsCategory()->RemoveItem(GlobalFindResultsMenuItem.ToSharedRef());
		GlobalFindResultsMenuItem.Reset();
	}
}

void FEGQuestSearchManager::Initialize(TSharedPtr<FWorkspaceItem> ParentTabCategory)
{
	// Must ensure we do not attempt to load the AssetRegistry Module while saving a package, however, if it is loaded already we can safely obtain it
	AssetRegistry = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>(NAME_MODULE_AssetRegistry).Get();

	OnAssetAddedHandle = AssetRegistry->OnAssetAdded().AddRaw(this, &Self::HandleOnAssetAdded);
	OnAssetRemovedHandle = AssetRegistry->OnAssetRemoved().AddRaw(this, &Self::HandleOnAssetRemoved);
	OnAssetRenamedHandle = AssetRegistry->OnAssetRenamed().AddRaw(this, &Self::HandleOnAssetRenamed);

	if (AssetRegistry->IsLoadingAssets())
	{
		OnFilesLoadedHandle = AssetRegistry->OnFilesLoaded().AddRaw(this, &Self::HandleOnAssetRegistryFilesLoaded);
	}
	else
	{
		// Already loaded
		HandleOnAssetRegistryFilesLoaded();
	}
	OnAssetLoadedHandle = FCoreUObjectDelegates::OnAssetLoaded.AddRaw(this, &Self::HandleOnAssetLoaded);

	// Register global find results tabs
	EnableGlobalFindResults(ParentTabCategory);
}

void FEGQuestSearchManager::UnInitialize()
{
	if (AssetRegistry)
	{
		if (OnAssetAddedHandle.IsValid())
		{
			AssetRegistry->OnAssetAdded().Remove(OnAssetAddedHandle);
			OnAssetAddedHandle.Reset();
		}
		if (OnAssetRemovedHandle.IsValid())
		{
			AssetRegistry->OnAssetRemoved().Remove(OnAssetRemovedHandle);
			OnAssetRemovedHandle.Reset();
		}
		if (OnFilesLoadedHandle.IsValid())
		{
			AssetRegistry->OnFilesLoaded().Remove(OnFilesLoadedHandle);
			OnFilesLoadedHandle.Reset();
		}
		if (OnAssetRenamedHandle.IsValid())
		{
			AssetRegistry->OnAssetRenamed().Remove(OnAssetRenamedHandle);
			OnAssetRenamedHandle.Reset();
		}
	}

	if (OnAssetLoadedHandle.IsValid())
	{
		FCoreUObjectDelegates::OnAssetLoaded.Remove(OnAssetLoadedHandle);
		OnAssetLoadedHandle.Reset();
	}

	// Shut down the global find results tab feature.
	DisableGlobalFindResults();
}

void FEGQuestSearchManager::BuildCache()
{
	// Difference between this and the UEGQuestManager::GetAllQuestsFromMemory is that this loads all Quests
	// even those that are not loaded into memory.
	// TODO this seems slow :(
	// AssetRegistryModule = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	// FARFilter ClassFilter;
	// ClassFilter.bRecursiveClasses = true;
	// ClassFilter.ClassNames.Add(UEGQuestGraph::StaticClass()->GetFName());
	// TArray<FAssetData> QuestAssets;
	// AssetRegistryModule->Get().GetAssets(ClassFilter, QuestAssets);
	// for (FAssetData& Asset : QuestAssets)
	// {
	// 	HandleOnAssetAdded(Asset);
	// }

	// We already loaded all Quests into memory in the StartupModule.
	for (UEGQuestGraph* Quest : UEGQuestManager::GetAllQuestsFromMemory())
	{
		FAssetData AssetData(Quest);
		HandleOnAssetAdded(AssetData);
	}
}

void FEGQuestSearchManager::HandleOnAssetAdded(const FAssetData& InAssetData)
{
	// Confirm that the Quest has not been added already, this can occur during duplication of Quests.
	const FEGQuestSearchData* SearchDataPtr = SearchMap.Find(InAssetData.ToSoftObjectPath());
	if (SearchDataPtr != nullptr)
	{
		// Already exists
		return;
	}

	// Ignore other assets
	if (!InAssetData.GetClass() || !InAssetData.GetClass()->IsChildOf<UEGQuestGraph>())
	{
		return;
	}

	// Load the Quest
	UEGQuestGraph* Quest = Cast<UEGQuestGraph>(InAssetData.GetAsset());
	if (!IsValid(Quest))
	{
		return;
	}

	// Add to the loaded cached map
	FEGQuestSearchData SearchData;
	SearchData.Quest = Quest;
	SearchMap.Add(InAssetData.ToSoftObjectPath(), MoveTemp(SearchData));
}

void FEGQuestSearchManager::HandleOnAssetRemoved(const FAssetData& InAssetData)
{
	// TODO
}

void FEGQuestSearchManager::HandleOnAssetRenamed(const FAssetData& InAssetData, const FString& InOldName)
{
	// TODO
}

void FEGQuestSearchManager::HandleOnAssetLoaded(UObject* InAsset)
{

}

void FEGQuestSearchManager::HandleOnAssetRegistryFilesLoaded()
{
	// TODO Pause search if garbage collecting?
	FEGQuestEditorUtilities::LoadAllQuestsAndCheckGUIDs();
	if (AssetRegistry)
	{
		// Do an immediate load of the cache to catch any Blueprints that were discovered by the asset registry before we initialized.
		BuildCache();
	}
}

#undef LOCTEXT_NAMESPACE
#undef NY_ARRAY_COUNT
