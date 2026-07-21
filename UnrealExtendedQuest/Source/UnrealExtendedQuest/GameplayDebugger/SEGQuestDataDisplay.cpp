// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "SEGQuestDataDisplay.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "UnrealExtendedQuest/EGQuestComponent.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

DEFINE_LOG_CATEGORY(LogEGQuestPluginDataDisplay)
#define LOCTEXT_NAMESPACE "SEGQuestDataDisplay"

void SEGQuestDataDisplay::Construct(const FArguments& InArgs, const TWeakObjectPtr<const UObject>& InWorldContextObjectPtr)
{
	WorldContextObjectPtr = InWorldContextObjectPtr;
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(4.f)
		[
			SNew(SButton).Text(LOCTEXT("Refresh", "Refresh Quest Snapshots")).OnClicked(this, &SEGQuestDataDisplay::HandleRefresh)
		]
		+ SVerticalBox::Slot().FillHeight(1.f).Padding(4.f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SAssignNew(SnapshotList, SVerticalBox)
			]
		]
	];
	RefreshTree(false);
}

void SEGQuestDataDisplay::RefreshTree(bool bPreserveExpansion)
{
	if (!SnapshotList.IsValid())
	{
		return;
	}
	SnapshotList->ClearChildren();

	const UObject* ContextObject = WorldContextObjectPtr.Get();
	UWorld* World = ContextObject ? ContextObject->GetWorld() : nullptr;
	if (!World)
	{
		SnapshotList->AddSlot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("NoWorld", "No game world is available."))];
		return;
	}

	if (AGameStateBase* GameState = World->GetGameState())
	{
		AddComponentSnapshots(GameState->FindComponentByClass<UEGQuestComponent>(), LOCTEXT("Shared", "Shared"));
	}
	if (APlayerController* PlayerController = World->GetFirstPlayerController())
	{
		if (APlayerState* PlayerState = PlayerController->GetPlayerState<APlayerState>())
		{
			AddComponentSnapshots(PlayerState->FindComponentByClass<UEGQuestComponent>(), LOCTEXT("Private", "Private"));
		}
	}
}

void SEGQuestDataDisplay::AddComponentSnapshots(UEGQuestComponent* Component, const FText& ScopeLabel)
{
	if (!Component)
	{
		return;
	}
	const TArray<FEGQuestRuntimeSnapshot>& Snapshots = ScopeLabel.EqualTo(LOCTEXT("Shared", "Shared"))
		? Component->GetSharedQuestSnapshots() : Component->GetPrivateQuestSnapshots();
	for (const FEGQuestRuntimeSnapshot& Snapshot : Snapshots)
	{
		AddSnapshot(Snapshot, ScopeLabel);
	}
}

void SEGQuestDataDisplay::AddSnapshot(const FEGQuestViewSnapshot& Snapshot, const FText& ScopeLabel)
{
	const UEnum* StateEnum = StaticEnum<EEGQuestLifecycleState>();
	const FString State = StateEnum ? StateEnum->GetNameStringByValue(static_cast<int64>(Snapshot.LifecycleState)) : TEXT("Unknown");
	const UEnum* OutcomeEnum = StaticEnum<EEGQuestObjectiveOutcome>();
	FString Checklist;
	for (const FEGQuestSnapshotObjective& Objective : Snapshot.ActiveObjectives)
	{
		const FString Outcome = OutcomeEnum ? OutcomeEnum->GetNameStringByValue(static_cast<int64>(Objective.Outcome)) : TEXT("Unknown");
		Checklist += FString::Printf(TEXT("\n  - %s [%s] %d/%d"),
			*Objective.Text.ToString(), *Outcome, Objective.Count, Objective.RequiredCount);
	}
	const FString Summary = FString::Printf(
		TEXT("%s | Instance %s | State %s | Revision %d\nGraph %s | Active %s\nStage '%s'%s"),
		*ScopeLabel.ToString(), *Snapshot.QuestInstanceGuid.ToString(), *State, Snapshot.Revision,
		*Snapshot.GraphGuid.ToString(), *Snapshot.ActiveNodeGuid.ToString(),
		*Snapshot.ActiveStageTitle.ToString(), *Checklist);
	SnapshotList->AddSlot().AutoHeight().Padding(2.f)
	[
		SNew(SBorder).Padding(5.f)[SNew(STextBlock).Text(FText::FromString(Summary))]
	];
}

FReply SEGQuestDataDisplay::HandleRefresh()
{
	RefreshTree(false);
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
