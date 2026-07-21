// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"

#include "UnrealExtendedQuest/EGQuestGraph.h"
#include "UnrealExtendedQuest/TreeViewHelpers/EGQuestTreeViewNode.h"

class FEGQuestSearchResult;
class UEGQuestGraphNode;
class UEdGraphNode_Comment;

// Filter used when searching for Quest Data
struct UNREALEXTENDEDQUESTEDITOR_API FEGQuestSearchFilter
{
public:
	bool IsEmptyFilter() const
	{
		return SearchString.IsEmpty()
			&& bIncludeIndices == false
			&& bIncludeQuestGUID == false
			&& bIncludeNodeGUID == false
			&& bIncludeComments == true
			&& bIncludeNumericalTypes == false
			&& bIncludeCustomObjectNames == false;
	}

public:
	// Search term that the search items must match
	FString SearchString;

	// Include node/edge indices in search results?
	bool bIncludeIndices = false;

	// Include the Quest GUID in search results
	bool bIncludeQuestGUID = false;

	// Include the Node GUID in search results
	bool bIncludeNodeGUID = false;

	// Include node comments in search results?
	bool bIncludeComments = true;

	// Include numerical data in search results like (int32, floats)?
	bool bIncludeNumericalTypes = false;

	// Include the Text localization data in search results (namespace, key)
	bool bIncludeTextLocalizationData = false;

	// Include the Custom Text Argument/Condition/Event/Node Data object names
	bool bIncludeCustomObjectNames = true;
};

// Base class that matched the search results. When used by itself it is a simple text node.
class UNREALEXTENDEDQUESTEDITOR_API FEGQuestSearchResult : public FEGQuestTreeViewNode<FEGQuestSearchResult>
{
	typedef FEGQuestSearchResult Self;
	typedef FEGQuestTreeViewNode Super;

public:
	FEGQuestSearchResult(const FText& InDisplayText, const TSharedPtr<Self>& InParent);

	// Create an icon to represent the result
	virtual TSharedRef<SWidget>	CreateIcon() const;

	// Gets the Quest housing all these search results. Aka the Quest this search result belongs to.
	virtual TWeakObjectPtr<const UEGQuestGraph> GetParentQuest() const;

	// Category:
	FText GetCategory() const { return Category; }
	void SetCategory(const FText& InCategory) { Category = InCategory; }

	// CommentString
	FString GetCommentString() const { return CommentString; }
	void SetCommentString(const FString& InCommentString) { CommentString = InCommentString; }

protected:
	// The category of this node.
	FText Category;

	// Display text for comment information
	FString CommentString;
};


// Root Node, should not be displayed.
class UNREALEXTENDEDQUESTEDITOR_API FEGQuestSearchResult_RootNode : public FEGQuestSearchResult
{
	typedef FEGQuestSearchResult Super;
public:
	FEGQuestSearchResult_RootNode();
};


// Tree Node search results that represents the Quest.
class UNREALEXTENDEDQUESTEDITOR_API FEGQuestSearchResult_QuestNode : public FEGQuestSearchResult
{
	typedef FEGQuestSearchResult Super;
public:
	FEGQuestSearchResult_QuestNode(const FText& InDisplayText, const TSharedPtr<FEGQuestSearchResult>& InParent);

	FReply OnClick() override;
	TWeakObjectPtr<const UEGQuestGraph> GetParentQuest() const override;
	TSharedRef<SWidget>	CreateIcon() const override;

	// Quest:
	void SetQuest(TWeakObjectPtr<const UEGQuestGraph> InQuest) { Quest = InQuest; }

protected:
	// The Quest this represents.
	TWeakObjectPtr<const UEGQuestGraph> Quest;
};


// Tree Node result that represents the GraphNode
class UNREALEXTENDEDQUESTEDITOR_API FEGQuestSearchResult_GraphNode : public FEGQuestSearchResult
{
	typedef FEGQuestSearchResult Super;
public:
	FEGQuestSearchResult_GraphNode(const FText& InDisplayText, const TSharedPtr<FEGQuestSearchResult>& InParent);

	FReply OnClick() override;
	TSharedRef<SWidget> CreateIcon() const override;

	// GraphNode:
	void SetGraphNode(TWeakObjectPtr<const UEGQuestGraphNode> InGraphNode) { GraphNode = InGraphNode; }

protected:
	// The GraphNode this represents.
	TWeakObjectPtr<const UEGQuestGraphNode> GraphNode;
};


// Tree Node result that represents the CommentNode
class UNREALEXTENDEDQUESTEDITOR_API FEGQuestSearchResult_CommentNode : public FEGQuestSearchResult
{
	typedef FEGQuestSearchResult Super;
public:
	FEGQuestSearchResult_CommentNode(const FText& InDisplayText, const TSharedPtr<FEGQuestSearchResult>& InParent);

	FReply OnClick() override;
	TSharedRef<SWidget> CreateIcon() const override;

	// CommentNode:
	void SetCommentNode(TWeakObjectPtr<const UEdGraphNode_Comment> InCommentNode) { CommentNode = InCommentNode; }

protected:
	// The EdgeNode this represents.
	TWeakObjectPtr<const UEdGraphNode_Comment> CommentNode;
};
