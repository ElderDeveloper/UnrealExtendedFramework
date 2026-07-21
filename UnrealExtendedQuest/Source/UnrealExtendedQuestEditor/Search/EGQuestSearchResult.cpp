// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestSearchResult.h"

#include "Widgets/Images/SImage.h"

#include "UnrealExtendedQuestEditor/EGQuestEditorUtilities.h"
#include "UnrealExtendedQuestEditor/Editor/Nodes/EGQuestGraphNode.h"
#include "UnrealExtendedQuestEditor/EGQuestStyle.h"

#define LOCTEXT_NAMESPACE "QuestSearchResult"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FEGQuestSearchResult
FEGQuestSearchResult::FEGQuestSearchResult(const FText& InDisplayText, const TSharedPtr<Self>& InParent)
	: Super(InDisplayText, InParent)
{
}

TSharedRef<SWidget>	FEGQuestSearchResult::CreateIcon() const
{
	const FLinearColor IconColor = FLinearColor::White;
	const FSlateBrush* Brush = nullptr;

	return SNew(SImage)
			.Image(Brush)
			.ColorAndOpacity(IconColor)
			.ToolTipText(GetCategory());
}

TWeakObjectPtr<const UEGQuestGraph> FEGQuestSearchResult::GetParentQuest() const
{
	if (Parent.IsValid())
	{
		return Parent.Pin()->GetParentQuest();
	}

	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FEGQuestSearchResult_RootNode
FEGQuestSearchResult_RootNode::FEGQuestSearchResult_RootNode() :
	Super(FText::FromString(TEXT("Display Text should not be visible")), nullptr)
{
	Category = FText::FromString(TEXT("ROOT NODE SHOULD NOT BE VISIBLE"));
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FEGQuestSearchResult_QuestNode
FEGQuestSearchResult_QuestNode::FEGQuestSearchResult_QuestNode(const FText& InDisplayText, const TSharedPtr<FEGQuestSearchResult>& InParent) :
	Super(InDisplayText, InParent)
{
	Category = LOCTEXT("FEGQuestSearchResult_QuestNodeCategory", "Quest");
}

FReply FEGQuestSearchResult_QuestNode::OnClick()
{
	if (Quest.IsValid())
	{
		return FEGQuestEditorUtilities::OpenEditorForAsset(Quest.Get()) ? FReply::Handled() : FReply::Unhandled();
	}

	return FReply::Unhandled();
}

TWeakObjectPtr<const UEGQuestGraph> FEGQuestSearchResult_QuestNode::GetParentQuest() const
{
	// Get the Quest from this.
	if (Quest.IsValid())
	{
		return Quest;
	}

	return Super::GetParentQuest();
}

TSharedRef<SWidget>	FEGQuestSearchResult_QuestNode::CreateIcon() const
{
	const FSlateBrush* Brush = FEGQuestStyle::Get()->GetBrush(FEGQuestStyle::PROPERTY_QuestGraphClassIcon);

	return SNew(SImage)
			.Image(Brush)
			.ColorAndOpacity(FSlateColor::UseForeground())
			.ToolTipText(GetCategory());
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FEGQuestSearchResult_GraphNode
FEGQuestSearchResult_GraphNode::FEGQuestSearchResult_GraphNode(const FText& InDisplayText, const TSharedPtr<FEGQuestSearchResult>& InParent) :
	Super(InDisplayText, InParent)
{
}

FReply FEGQuestSearchResult_GraphNode::OnClick()
{
	if (GraphNode.IsValid())
	{
		return FEGQuestEditorUtilities::OpenEditorAndJumpToGraphNode(GraphNode.Get()) ? FReply::Handled() : FReply::Unhandled();
	}

	return FReply::Unhandled();
}

TSharedRef<SWidget> FEGQuestSearchResult_GraphNode::CreateIcon() const
{
	if (GraphNode.IsValid())
	{
		FLinearColor Color;
		const FSlateIcon Icon = GraphNode.Get()->GetIconAndTint(Color);
		return SNew(SImage)
				.Image(Icon.GetOptionalIcon())
				.ColorAndOpacity(Color)
				.ToolTipText(GetCategory());
	}

	return Super::CreateIcon();
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FEGQuestSearchResult_CommentNode
FEGQuestSearchResult_CommentNode::FEGQuestSearchResult_CommentNode(const FText& InDisplayText, const TSharedPtr<FEGQuestSearchResult>& InParent) :
	Super(InDisplayText, InParent)
{
}

FReply FEGQuestSearchResult_CommentNode::OnClick()
{
	if (CommentNode.IsValid())
	{
		return FEGQuestEditorUtilities::OpenEditorAndJumpToGraphNode(CommentNode.Get()) ? FReply::Handled() : FReply::Unhandled();
	}

	return FReply::Unhandled();
}

TSharedRef<SWidget>	FEGQuestSearchResult_CommentNode::CreateIcon() const
{
	if (CommentNode.IsValid())
	{
		const FSlateIcon Icon = FSlateIcon(FEGQuestStyle::GetStyleSetName(), FEGQuestStyle::PROPERTY_CommentBubbleOn);
		return SNew(SImage)
			.Image(Icon.GetIcon())
			.ColorAndOpacity(FColorList::White)
			.ToolTipText(GetCategory());
	}

	return Super::CreateIcon();
}

#undef LOCTEXT_NAMESPACE
