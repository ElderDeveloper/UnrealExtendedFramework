// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "SEGQuestBrowser.h"

#include "UnrealExtendedQuest/EGQuestGraph.h"
#include "UnrealExtendedQuest/EGQuestManager.h"
#include "UnrealExtendedQuestEditor/EGQuestEditorUtilities.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SEGQuestBrowser"

void SEGQuestBrowser::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(4.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("Refresh", "Refresh Quest Assets"))
			.OnClicked(this, &SEGQuestBrowser::HandleRefresh)
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(4.f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SAssignNew(QuestList, SVerticalBox)
			]
		]
	];

	RefreshTree(false);
}

void SEGQuestBrowser::RefreshTree(bool bPreserveExpansion)
{
	UEGQuestManager::LoadAllQuestsIntoMemory(false);
	RebuildQuestList();
}

void SEGQuestBrowser::RebuildQuestList()
{
	if (!QuestList.IsValid())
	{
		return;
	}

	QuestList->ClearChildren();
	TArray<UEGQuestGraph*> Quests = UEGQuestManager::GetAllQuestsFromMemory();
	Quests.Sort([](const UEGQuestGraph& Left, const UEGQuestGraph& Right)
	{
		return Left.GetPathName() < Right.GetPathName();
	});

	for (UEGQuestGraph* Quest : Quests)
	{
		if (!IsValid(Quest))
		{
			continue;
		}

		QuestList->AddSlot().AutoHeight().Padding(1.f)
		[
			SNew(SButton)
			.Text(FText::FromString(Quest->GetPathName()))
			.ToolTipText(LOCTEXT("OpenQuestTooltip", "Open this quest in the graph editor."))
			.OnClicked(this, &SEGQuestBrowser::OpenQuest, Quest)
		];
	}

	if (Quests.IsEmpty())
	{
		QuestList->AddSlot().AutoHeight().Padding(4.f)
		[
			SNew(STextBlock).Text(LOCTEXT("NoQuests", "No Quest Graph assets found."))
		];
	}
}

FReply SEGQuestBrowser::OpenQuest(UEGQuestGraph* Quest)
{
	if (IsValid(Quest))
	{
		FEGQuestEditorUtilities::OpenEditorForAsset(Quest);
	}
	return FReply::Handled();
}

FReply SEGQuestBrowser::HandleRefresh()
{
	RefreshTree(false);
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
