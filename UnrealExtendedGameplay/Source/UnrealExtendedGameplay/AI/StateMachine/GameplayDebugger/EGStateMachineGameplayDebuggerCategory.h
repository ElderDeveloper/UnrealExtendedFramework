// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#if WITH_GAMEPLAY_DEBUGGER

#include "CoreMinimal.h"
#include "GameplayDebuggerCategory.h"

class AActor;
class APlayerController;
class FGameplayDebuggerCanvasContext;

/**
 * Gameplay debugger view of UEGStateMachineComponent.
 *
 * Collected on the machine running the debugger, so on a listen server this shows the host's own
 * simulation while a client shows its mirror — which is exactly what you need side by side to spot
 * a mirroring divergence.
 */
class UNREALEXTENDEDGAMEPLAY_API FEGStateMachineGameplayDebuggerCategory : public FGameplayDebuggerCategory
{
public:
	FEGStateMachineGameplayDebuggerCategory();

	static TSharedRef<FGameplayDebuggerCategory> MakeInstance() { return MakeShared<FEGStateMachineGameplayDebuggerCategory>(); }

	virtual void CollectData(APlayerController* OwnerPC, AActor* DebugActor) override;
	virtual void DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext) override;

protected:
	struct FRepData
	{
		/** Summary for the selected actor's machine: side, state, sub-phase, stack. */
		FString SelectedSummary;

		/** Ring-buffer lines from the selected actor's machine. */
		TArray<FString> SelectedLog;

		/** One line per other machine in the world, so a whole encounter is visible at once. */
		TArray<FString> WorldSummaries;

		int32 MachineCount = 0;

		void Serialize(FArchive& Ar);
	};

	FRepData DataPack;
};

#endif // WITH_GAMEPLAY_DEBUGGER
