// Copyright Devil of the Plague. All Rights Reserved.
#include "EGQuestNode_Stage.h"

#include "UnrealExtendedQuest/EGQuestContext.h"
#include "UnrealExtendedQuest/EGQuestLocalizationHelper.h"

#if WITH_EDITOR
void UEGQuestNode_Stage::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property != nullptr ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == GetMemberNameDescription() || PropertyName == GetMemberNameTextArguments())
	{
		RebuildTextArguments();
	}
}
#endif

void UEGQuestNode_Stage::UpdateTextsValuesFromDefaultsAndRemappings(const UEGQuestPluginSettings& Settings, bool bUpdateGraphNode)
{
	FEGQuestLocalizationHelper::UpdateTextFromRemapping(Settings, Title);
	FEGQuestLocalizationHelper::UpdateTextFromRemapping(Settings, Description);
	Super::UpdateTextsValuesFromDefaultsAndRemappings(Settings, bUpdateGraphNode);
}

void UEGQuestNode_Stage::UpdateTextsNamespacesAndKeys(const UEGQuestPluginSettings& Settings, bool bUpdateGraphNode)
{
	FEGQuestLocalizationHelper::UpdateTextNamespaceAndKey(GetOuter(), Settings, Title);
	FEGQuestLocalizationHelper::UpdateTextNamespaceAndKey(GetOuter(), Settings, Description);
	Super::UpdateTextsNamespacesAndKeys(Settings, bUpdateGraphNode);
}

void UEGQuestNode_Stage::RebuildConstructedText(UEGQuestContext& Context) const
{
	if (TextArguments.Num() <= 0)
	{
		return;
	}

	FFormatNamedArguments OrderedArguments;
	for (const FEGQuestTextArgument& QuestArgument : TextArguments)
	{
		OrderedArguments.Add(QuestArgument.DisplayString, QuestArgument.ConstructFormatArgumentValue(Context));
	}
	// Stored per context - never on this (shared) node object.
	Context.SetConstructedNodeText(NodeGUID, FText::Format(Description, OrderedArguments));
}
