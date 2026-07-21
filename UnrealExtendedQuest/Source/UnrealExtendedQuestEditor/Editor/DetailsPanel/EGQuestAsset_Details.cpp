// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestAsset_Details.h"

#include "DetailLayoutBuilder.h"

#include "UnrealExtendedQuest/EGQuestGraph.h"

#define LOCTEXT_NAMESPACE "Quest_Details"

void FEGQuestAsset_Details::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> ObjectsBeingCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsBeingCustomized);
	// Only support one object being customized
	if (ObjectsBeingCustomized.Num() != 1)
	{
		return;
	}

	// The asset is edited through the graph, never through this panel. Everything below is
	// bookkeeping the designer cannot meaningfully edit: the start/node arrays and GUID map only
	// mirror the compiled graph, and without these hides they surface as read-only rows and an
	// Advanced expander. Name and GUID stay visible as the asset's identity.
	UClass* QuestClass = UEGQuestGraph::StaticClass();
	DetailBuilder.HideProperty(UEGQuestGraph::GetMemberNameStartNodes(), QuestClass);
	DetailBuilder.HideProperty(UEGQuestGraph::GetMemberNameNodes(), QuestClass);
	DetailBuilder.HideProperty(UEGQuestGraph::GetMemberNameNodesGUIDToIndexMap(), QuestClass);
	DetailBuilder.HideProperty(UEGQuestGraph::GetMemberNameAssetUserData(), QuestClass);
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
