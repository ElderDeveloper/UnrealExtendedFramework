// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/Docking/SDockTab.h"

#include "EGQuestSearchResult.h"

// The maximum amount of global Quest Search windows opened.
static constexpr int32 MAX_GLOBAL_QUEST_SEARCH_RESULTS = 4;

class SEGQuestFindInQuests;
class FAssetRegistryModule;

class FWorkspaceItem;
class UEGQuestGraphNode;
class UEdGraphNode_Comment;
class IAssetRegistry;
struct FAssetData;
class UEGQuestEventCustom;
class UEGQuestNode_Objective;
struct FEGQuestEdge;
struct FEGQuestTextArgument;

struct UNREALEXTENDEDQUESTEDITOR_API FEGQuestSearchData
{
	/** The Quest this search data points to, if available */
	TWeakObjectPtr<UEGQuestGraph> Quest;
};

/** Singleton manager for handling all Quest searches */
class UNREALEXTENDEDQUESTEDITOR_API FEGQuestSearchManager
{
private:
	typedef FEGQuestSearchManager Self;

public:
	static Self* Get();

	FEGQuestSearchManager();
	~FEGQuestSearchManager();

	/**
	 * Searches for InSearchString in the InQuestTextArgument. Adds the result as a child in OutParentNode.
	 * @return True if found anything matching the InSearchString
	 */
	bool QueryQuestTextArgument(
		const FEGQuestSearchFilter& SearchFilter,
		const FEGQuestTextArgument& InQuestTextArgument,
		const TSharedPtr<FEGQuestSearchResult>& OutParentNode,
		int32 ArgumentIndex = INDEX_NONE
	);

	/**
	 * Searches for InSearchString in the InObjective's evaluation fields (class name, event tags,
	 * counts). Adds the result as a child in OutParentNode.
	 * @return True if found anything matching the InSearchString
	 */
	bool QueryQuestObjective(
		const FEGQuestSearchFilter& SearchFilter,
		const UEGQuestNode_Objective* InObjective,
		const TSharedPtr<FEGQuestSearchResult>& OutParentNode
	);

	/**
	 * Searches for InSearchString in the InQuestEvent. Adds the result as a child in OutParentNode.
	 * @return True if found anything matching the InSearchString
	 */
	bool QueryQuestEvent(
		const FEGQuestSearchFilter& SearchFilter,
		const UEGQuestEventCustom* InQuestEvent,
		const TSharedPtr<FEGQuestSearchResult>& OutParentNode,
		int32 EventIndex = INDEX_NONE,
		FName EventMemberName = TEXT("Event")
	);

	/**
	 * Searches for InSearchString in the InGraphNode. Adds the result as a child in OutParentNode.
	 * @return True if found anything matching the InSearchString
	 */
	bool QueryGraphNode(
		const FEGQuestSearchFilter& SearchFilter,
		const UEGQuestGraphNode* InGraphNode,
		const TSharedPtr<FEGQuestSearchResult>& OutParentNode
	);

	/**
	 * Searches for InSearchString in the Comment Node. Adds the result as a child in OutParentNode.
	 * @return True if found anything matching the InSearchString
	 */
	bool QueryCommentNode(
		const FEGQuestSearchFilter& SearchFilter,
		const UEdGraphNode_Comment* InCommentNode,
		const TSharedPtr<FEGQuestSearchResult>& OutParentNode
	);

	/**
	 * Searches for InSearchString in the InQuest. Adds the result as a child of OutParentNode.
	 * @return True if found anything matching the InSearchString
	 */
	bool QuerySingleQuest(
		const FEGQuestSearchFilter& SearchFilter,
		const UEGQuestGraph* InQuest,
		TSharedPtr<FEGQuestSearchResult>& OutParentNode
	);

	// Searches for InSearchString in all Quests. Adds the result as children of OutParentNode.
	void QueryAllQuests(const FEGQuestSearchFilter& SearchFilter, TSharedPtr<FEGQuestSearchResult>& OutParentNode);

	// Determines the global find results tab label
	FText GetGlobalFindResultsTabLabel(int32 TabIdx);

	// Close One of the global find results.
	void CloseGlobalFindResults(const TSharedRef<SEGQuestFindInQuests>& FindResults);

	// Find or create the global find results widget
	TSharedPtr<SEGQuestFindInQuests> GetGlobalFindResults();

	// Enables the global find results tab feature in the Windows Menu.
	void EnableGlobalFindResults(TSharedPtr<FWorkspaceItem> ParentTabCategory = nullptr);

	// Disables the global find results tab feature in the Windows Menu.
	void DisableGlobalFindResults();

	// Initializes the manager. Should only be called once in the FEGQuestPluginEditorModule::StartupModule()
	void Initialize(TSharedPtr<FWorkspaceItem> ParentTabCategory = nullptr);

	// Uninitializes the manager. Should only be called once in the FEGQuestPluginEditorModule::ShutdownModule()
	void UnInitialize();

private:
	// Helper method to make a Text Node and add it as a child to ParentNode
	TSharedPtr<FEGQuestSearchResult> MakeChildTextNode(
		const TSharedPtr<FEGQuestSearchResult>& ParentNode,
		const FText& DisplayName, const FText& Category,
		const FString& CommentString
	)
	{
		TSharedPtr<FEGQuestSearchResult> TextNode = MakeShared<FEGQuestSearchResult>(DisplayName, ParentNode);
		TextNode->SetCategory(Category);
		if (!CommentString.IsEmpty())
		{
			TextNode->SetCommentString(CommentString);
		}
		ParentNode->AddChild(TextNode);
		return TextNode;
	}

	bool SearchForTextLocalizationData(
		const TSharedPtr<FEGQuestSearchResult>& ParentNode,
		const FString& SearchString,
		const FText& Text,
		const FText& NamespaceCategory,
		const FString& NamespaceCommentString,
		const FText& KeyCategory,
		const FString& KeyCommentString
	)
	{
		static const FString DefaultValue = TEXT("");
		bool bContainsSearchString = false;

		const FString CurrentFullNamespace = FTextInspector::GetNamespace(Text).Get(DefaultValue);
		const FString CurrentKey = FTextInspector::GetKey(Text).Get(DefaultValue);
		if (CurrentFullNamespace.Contains(SearchString))
		{
			bContainsSearchString = true;
			MakeChildTextNode(
				ParentNode,
				FText::AsCultureInvariant(CurrentFullNamespace),
				NamespaceCategory,
				NamespaceCommentString
			);
		}
		if (CurrentKey.Contains(SearchString))
		{
			bContainsSearchString = true;
			MakeChildTextNode(
				ParentNode,
				FText::AsCultureInvariant(CurrentKey),
				KeyCategory,
				KeyCommentString
			);
		}

		return bContainsSearchString;
	}

	// Handler for a request to spawn a new global find results tab
	TSharedRef<SDockTab> SpawnGlobalFindResultsTab(const FSpawnTabArgs& SpawnTabArgs, int32 TabIdx);

	// Creates and opens a new global find results tab. The next one in the available list.
	TSharedPtr<SEGQuestFindInQuests> OpenGlobalFindResultsTab();

	// Builds the cache from all available Quests assets that the asset registry has discovered at the time of this function. Occurs on startup.
	void BuildCache();

	// Callback hook from the Asset Registry when an asset is added
	void HandleOnAssetAdded(const FAssetData& InAssetData);

	// Callback hook from the Asset Registry, marks the asset for deletion from the cache
	void HandleOnAssetRemoved(const FAssetData& InAssetData);

	// Callback hook from the Asset Registry, marks the asset for deletion from the cache
	void HandleOnAssetRenamed(const FAssetData& InAssetData, const FString& InOldName);

	// Callback hook from the Asset Registry when an asset is loaded
	void HandleOnAssetLoaded(UObject* InAsset);

	// Callback when the Asset Registry loads all its assets
	void HandleOnAssetRegistryFilesLoaded();

private:
	static Self* Instance;

	// Maps the Quest path => SearchData.
	TMap<FSoftObjectPath, FEGQuestSearchData> SearchMap;

	// Because we are unable to query for the module on another thread, cache it for use later
	IAssetRegistry* AssetRegistry = nullptr;

	// The tab identifier/instance name for global find results
	FName GlobalFindResultsTabIDs[MAX_GLOBAL_QUEST_SEARCH_RESULTS];

	// Array of open global find results widgets
	TArray<TWeakPtr<SEGQuestFindInQuests>> GlobalFindResultsWidgets;

	// Global Find Results workspace menu item
	TSharedPtr<FWorkspaceItem> GlobalFindResultsMenuItem;

	// Handlers
	FDelegateHandle OnAssetAddedHandle;
	FDelegateHandle OnAssetRemovedHandle;
	FDelegateHandle OnAssetRenamedHandle;
	FDelegateHandle OnFilesLoadedHandle;
	FDelegateHandle OnAssetLoadedHandle;
};
