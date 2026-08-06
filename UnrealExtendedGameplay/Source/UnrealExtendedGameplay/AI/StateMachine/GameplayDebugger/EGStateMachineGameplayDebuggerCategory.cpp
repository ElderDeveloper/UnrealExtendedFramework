// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGStateMachineGameplayDebuggerCategory.h"

#if WITH_GAMEPLAY_DEBUGGER

#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "UnrealExtendedGameplay/AI/StateMachine/EGStateMachineComponent.h"

FEGStateMachineGameplayDebuggerCategory::FEGStateMachineGameplayDebuggerCategory()
{
	bShowOnlyWithDebugActor = false;
	SetDataPackReplication(&DataPack);
}

void FEGStateMachineGameplayDebuggerCategory::FRepData::Serialize(FArchive& Ar)
{
	Ar << SelectedSummary;
	Ar << SelectedLog;
	Ar << WorldSummaries;
	Ar << MachineCount;
}

void FEGStateMachineGameplayDebuggerCategory::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	DataPack.SelectedSummary.Reset();
	DataPack.SelectedLog.Reset();
	DataPack.WorldSummaries.Reset();
	DataPack.MachineCount = 0;

	if (const AActor* Selected = DebugActor)
	{
		if (const UEGStateMachineComponent* Machine = Selected->FindComponentByClass<UEGStateMachineComponent>())
		{
			DataPack.SelectedSummary = Machine->BuildDebugSummary();
			DataPack.SelectedLog = Machine->GetDebugLog();
		}
	}

	const UWorld* World = OwnerPC ? OwnerPC->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	for (TActorIterator<AActor> It(const_cast<UWorld*>(World)); It; ++It)
	{
		const AActor* Actor = *It;
		const UEGStateMachineComponent* Machine = Actor ? Actor->FindComponentByClass<UEGStateMachineComponent>() : nullptr;
		if (!Machine)
		{
			continue;
		}

		++DataPack.MachineCount;

		if (Actor == DebugActor)
		{
			continue;
		}

		DataPack.WorldSummaries.Add(FString::Printf(TEXT("%s: %s (stack %d)"),
			*Actor->GetName(),
			*GetNameSafe(Machine->GetCurrentStateClass().Get()),
			Machine->GetStackDepth()));
	}
}

void FEGStateMachineGameplayDebuggerCategory::DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	CanvasContext.Printf(TEXT("{yellow}State machines in world: {white}%d"), DataPack.MachineCount);

	if (!DataPack.SelectedSummary.IsEmpty())
	{
		CanvasContext.Printf(TEXT("{yellow}Selected:"));
		CanvasContext.Printf(TEXT("{green}%s"), *DataPack.SelectedSummary);

		if (DataPack.SelectedLog.Num() > 0)
		{
			CanvasContext.Printf(TEXT("{yellow}Log:"));
			for (const FString& Line : DataPack.SelectedLog)
			{
				CanvasContext.Printf(TEXT("{cyan}  %s"), *Line);
			}
		}
	}
	else
	{
		CanvasContext.Printf(TEXT("{grey}No state machine on the selected actor."));
	}

	if (DataPack.WorldSummaries.Num() > 0)
	{
		CanvasContext.Printf(TEXT("{yellow}Other machines:"));
		for (const FString& Line : DataPack.WorldSummaries)
		{
			CanvasContext.Printf(TEXT("{white}  %s"), *Line);
		}
	}
}

#endif // WITH_GAMEPLAY_DEBUGGER
