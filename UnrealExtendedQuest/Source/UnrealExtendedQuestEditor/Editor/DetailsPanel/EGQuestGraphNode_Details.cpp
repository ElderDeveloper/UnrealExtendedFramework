// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestGraphNode_Details.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"

#include "UnrealExtendedQuest/Nodes/EGQuestNode.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_End.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_Objective.h"
#include "UnrealExtendedQuest/Nodes/EGQuestNode_Stage.h"
#include "UnrealExtendedQuestEditor/Editor/Nodes/EGQuestGraphNode.h"
#include "UnrealExtendedQuestEditor/Editor/DetailsPanel/Widgets/SEGQuestTextPropertyPickList.h"
#include "UnrealExtendedQuestEditor/Editor/DetailsPanel/Widgets/EGQuestTextPropertyPickList_CustomRowHelper.h"
#include "UnrealExtendedQuestEditor/Editor/DetailsPanel/Widgets/EGQuestMultiLineEditableTextBox_CustomRowHelper.h"
#include "UnrealExtendedQuestEditor/Editor/DetailsPanel/Widgets/EGQuestObject_CustomRowHelper.h"

#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "DialoguGraphNode_Details"

void FEGQuestGraphNode_Details::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);

	// Only support one object being customized
	if (ObjectsBeingCustomized.Num() != 1)
	{
		return;
	}

	TWeakObjectPtr<UEGQuestGraphNode> WeakGraphNode = Cast<UEGQuestGraphNode>(ObjectsBeingCustomized[0].Get());
	if (!WeakGraphNode.IsValid())
	{
		return;
	}

	GraphNode = WeakGraphNode.Get();
	if (!IsValid(GraphNode))
	{
		return;
	}

	DetailLayoutBuilder = &DetailBuilder;
	Quest = GraphNode->GetQuest();
	const UEGQuestNode& QuestNode = GraphNode->GetQuestNode();
	const bool bIsRootNode = GraphNode->IsRootNode();
	const bool bIsEndNode = GraphNode->IsEndNode();
	const bool bIsStageNode = GraphNode->IsStageNode();
	const bool bIsCustomNode = GraphNode->IsCustomNode();

	// Everything lives in one flat category. The quest node is reached through a UObject property on
	// the graph node, which by default renders as its own category, then a class-picker row, then
	// the node's own categories nested inside - three headers wrapped around a handful of real
	// properties. Hiding that passthrough and adding each property explicitly below flattens it.
	// The details panel always draws one category header, so one is as far down as this goes.
	DetailLayoutBuilder->HideCategory(TEXT("QuestGraphNode"));
	DetailLayoutBuilder->HideCategory(UEGQuestGraphNode::StaticClass()->GetFName());

	const TSharedPtr<IPropertyHandle> PropertyQuestNode =
		DetailLayoutBuilder->GetProperty(UEGQuestGraphNode::GetMemberNameQuestNode(), UEGQuestGraphNode::StaticClass());

	// The root node owns nothing the designer may edit, so its panel shows nothing at all - without
	// this the default layout leaks the base-node properties (enter events, GUID, edges) into it.
	if (bIsRootNode)
	{
		DetailLayoutBuilder->HideProperty(PropertyQuestNode);
		DetailLayoutBuilder->HideProperty(UEGQuestGraphNode::GetMemberNameObjectives(), UEGQuestGraphNode::StaticClass());
		return;
	}

	// Only stage cards have checklist rows.
	if (!bIsStageNode)
	{
		DetailLayoutBuilder->HideProperty(UEGQuestGraphNode::GetMemberNameObjectives(), UEGQuestGraphNode::StaticClass());
	}

	// Bookkeeping the designer can never edit: the GUID and the edge list only mirror the graph.
	// Hidden rather than shown read-only - without this they surface in the Advanced expander.
	DetailLayoutBuilder->HideProperty(PropertyQuestNode->GetChildHandle(UEGQuestNode::GetMemberNameGUID()));
	DetailLayoutBuilder->HideProperty(PropertyQuestNode->GetChildHandle(UEGQuestNode::GetMemberNameChildren()));

	IDetailCategoryBuilder& BaseDataCategory =
		DetailLayoutBuilder->EditCategory(FName(*QuestNode.GetNodeTypeString()), FText::GetEmpty(), ECategoryPriority::Important);
	BaseDataCategory.InitiallyCollapsed(false);

	//
	// The node's own properties come first - they are what the node is for.
	//

	if (bIsStageNode)
	{
		BaseDataCategory.AddProperty(PropertyQuestNode->GetChildHandle(UEGQuestNode_Stage::GetMemberNameTitle()));

		DescriptionPropertyHandle = PropertyQuestNode->GetChildHandle(UEGQuestNode_Stage::GetMemberNameDescription());
		FDetailWidgetRow* DescriptionRow = &BaseDataCategory.AddCustomRow(LOCTEXT("DescriptionSearchKey", "Description"));
		DescriptionPropertyRow = MakeShared<FEGQuestMultiLineEditableTextBox_CustomRowHelper>(DescriptionRow, DescriptionPropertyHandle);
		DescriptionPropertyRow->SetPropertyUtils(DetailBuilder.GetPropertyUtilities());
		DescriptionPropertyRow->Update();
		DescriptionPropertyRow->OnTextCommittedEvent().AddRaw(this, &Self::HandleTextCommitted);
		DescriptionPropertyRow->OnTextChangedEvent().AddRaw(this, &Self::HandleTextChanged);

		BaseDataCategory.AddProperty(PropertyQuestNode->GetChildHandle(UEGQuestNode_Stage::GetMemberNameTextArguments()))
			.ShouldAutoExpand(true);

		// The script-facing stage name, passed to OnStageEntered/OnStageExited.
		BaseDataCategory.AddProperty(PropertyQuestNode->GetChildHandle(UEGQuestNode_Stage::GetMemberNameStageId()));

		// The checklist: each row is an objective with its text and conditions. Rows can also be
		// added straight on the card with its "+ add objective" button.
		BaseDataCategory.AddProperty(UEGQuestGraphNode::GetMemberNameObjectives(), UEGQuestGraphNode::StaticClass())
			.ShouldAutoExpand(true);
	}

	if (bIsEndNode)
	{
		BaseDataCategory.AddProperty(PropertyQuestNode->GetChildHandle(UEGQuestNode_End::GetMemberNameQuestResult()));
	}

	// A custom node's own properties are whatever its subclass declares.
	if (bIsCustomNode)
	{
		for (FProperty* Property = QuestNode.GetClass()->PropertyLink; Property != nullptr; Property = Property->PropertyLinkNext)
		{
			if (Property->GetOwnerClass() == UEGQuestNode_Custom::StaticClass() || Property->GetOwnerClass() == UEGQuestNode::StaticClass())
			{
				break;
			}
			TSharedPtr<IPropertyHandle> PropertyHandle = PropertyQuestNode->GetChildHandle(Property->GetFName());
			if (PropertyHandle.IsValid() && PropertyHandle->IsValidHandle())
			{
				BaseDataCategory.AddProperty(PropertyHandle);
			}
		}
	}

	//
	// Then what every node shares.
	//

	BaseDataCategory.AddProperty(PropertyQuestNode->GetChildHandle(UEGQuestNode::GetMemberNameEnterEvents()))
		.ShouldAutoExpand(true);
}

void FEGQuestGraphNode_Details::HandleTextCommitted(const FText& InText, ETextCommit::Type CommitInfo)
{
	// Text arguments already handled in node post change properties
}

void FEGQuestGraphNode_Details::HandleTextChanged(const FText& InText)
{
	if (GraphNode)
	{
		GraphNode->GetMutableQuestNode()->RebuildTextArgumentsFromPreview(InText);
	}
}


//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
