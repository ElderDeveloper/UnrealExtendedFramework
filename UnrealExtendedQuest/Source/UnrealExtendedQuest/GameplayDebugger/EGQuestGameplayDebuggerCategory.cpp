// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#if WITH_GAMEPLAY_DEBUGGER
#include "EGQuestGameplayDebuggerCategory.h"

#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "UnrealExtendedQuest/EGQuestComponent.h"
#include "UnrealExtendedQuest/EGQuestManager.h"

FEGQuestGameplayDebuggerCategory::FEGQuestGameplayDebuggerCategory()
{
	bShowOnlyWithDebugActor = false;
}

void FEGQuestGameplayDebuggerCategory::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	Data.NumLoadedQuests = UEGQuestManager::GetAllQuestsFromMemory().Num();
	Data.SnapshotLines.Reset();

	const UWorld* World = OwnerPC ? OwnerPC->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	const UEnum* StateEnum = StaticEnum<EEGQuestLifecycleState>();
	for (TActorIterator<AActor> It(const_cast<UWorld*>(World)); It; ++It)
	{
		const UEGQuestComponent* Component = It->FindComponentByClass<UEGQuestComponent>();
		if (!Component)
		{
			continue;
		}
		auto AppendSnapshots = [this, StateEnum, &It, World](const TCHAR* Scope, const TArray<FEGQuestRuntimeSnapshot>& Snapshots)
		{
			for (const FEGQuestRuntimeSnapshot& Snapshot : Snapshots)
			{
				const FString State = StateEnum ? StateEnum->GetNameStringByValue(static_cast<int64>(Snapshot.LifecycleState)) : TEXT("?");
				Data.SnapshotLines.Add(FString::Printf(
					TEXT("{yellow}%s {white}%s %s Rev=%d Stage='%s' Node=%s Owner=%s"),
					Scope, *Snapshot.QuestAssetId.PrimaryAssetName.ToString(), *State, Snapshot.Revision,
					*Snapshot.ActiveStageTitle.ToString(),
					*Snapshot.ActiveNodeGuid.ToString(EGuidFormats::Short), *It->GetName()));

				Data.SnapshotLines.Add(FString::Printf(TEXT("    {cyan}Tracked=%s Suspended=%s Tracks=%d Roles=%d"),
					Snapshot.bTracked ? TEXT("yes") : TEXT("no"), Snapshot.bSuspendedByRoleLoss ? TEXT("yes") : TEXT("no"),
					Snapshot.Tracks.Num(), Snapshot.RoleMarkers.Num()));
				const UEnum* OutcomeEnum = StaticEnum<EEGQuestObjectiveOutcome>();
				for (const FEGQuestTrackState& Track : Snapshot.Tracks)
				{
					Data.SnapshotLines.Add(FString::Printf(
						TEXT("    {cyan}Track %s (%s) Stage='%s' Node=%s"), *Track.TrackName.ToString(),
						Track.TrackType == EEGQuestTrackType::Main ? TEXT("Main") : TEXT("Sentinel"),
						*Track.StageTitle.ToString(), *Track.ActiveNodeGuid.ToString(EGuidFormats::Short)));
					for (const FEGQuestSnapshotObjective& Objective : Track.ActiveObjectives)
					{
						const FString Outcome = OutcomeEnum ? OutcomeEnum->GetNameStringByValue(static_cast<int64>(Objective.Outcome)) : TEXT("?");
						const double Remaining = Objective.Progress.EndServerTime > 0.0
							? FMath::Max(0.0, Objective.Progress.EndServerTime - World->GetTimeSeconds()) : 0.0;
						Data.SnapshotLines.Add(FString::Printf(
							TEXT("        {grey}- {white}%s [%s] %d/%d Ratio=%.2f Time=%.1f Keys=%d UI=%s"),
							*Objective.Text.ToString(), *Outcome, Objective.Progress.Count,
							Objective.Progress.RequiredCount, Objective.Progress.Ratio, Remaining,
							Objective.Progress.UniqueSet.Num(), *Objective.UITag.ToString()));
					}
				}
				for (const FEGQuestRoleMarker& Marker : Snapshot.RoleMarkers)
					Data.SnapshotLines.Add(FString::Printf(TEXT("    {magenta}Role %s -> %s (%s)"),
						*Marker.RoleName.ToString(), *Marker.Handle.StableId.ToString(), Marker.bResolved ? TEXT("loaded") : TEXT("unloaded")));
			}
		};
		AppendSnapshots(TEXT("Shared"), Component->GetSharedQuestSnapshots());
		AppendSnapshots(TEXT("Private"), Component->GetPrivateQuestSnapshots());
	}
}

void FEGQuestGameplayDebuggerCategory::DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	CanvasContext.Printf(TEXT("{green}Number loaded Quests: %s"), *FString::FromInt(Data.NumLoadedQuests));
	if (Data.SnapshotLines.IsEmpty())
	{
		CanvasContext.Printf(TEXT("{grey}No quest snapshots in this world."));
		return;
	}
	for (const FString& Line : Data.SnapshotLines)
	{
		CanvasContext.Print(Line);
	}
}

#endif // WITH_GAMEPLAY_DEBUGGER
