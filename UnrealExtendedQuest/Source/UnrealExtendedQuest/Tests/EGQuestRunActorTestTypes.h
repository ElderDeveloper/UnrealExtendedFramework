// Copyright Devil of the Plague. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedQuest/EGQuestRunActor.h"
#include "EGQuestRunActorTestTypes.generated.h"

/**
 * Records the lifecycle hooks it receives so a test can assert the order they arrived in.
 *
 * Lives in the runtime module rather than the test target because a run actor class has to be
 * reflected and spawnable, the same reason EGQuestIOTesterTypes does.
 */
UCLASS(NotPlaceable)
class UNREALEXTENDEDQUEST_API AEGQuestTestRunActor : public AEGQuestRunActor
{
	GENERATED_BODY()

public:
	/** Hook names in arrival order, across every instance. Reset() before each test. */
	static TArray<FName> Recorded;
	static int32 SpawnCount;
	static EEGQuestResult LastResult;

	static void Reset()
	{
		Recorded.Reset();
		SpawnCount = 0;
		LastResult = EEGQuestResult::Completed;
	}

	AEGQuestTestRunActor() { ++SpawnCount; }

protected:
	virtual void OnQuestStarted_Implementation() override;
	virtual void OnStageEntered_Implementation(const FGuid& StageGuid, FName StageId, const FText& StageTitle) override;
	virtual void OnStageExited_Implementation(const FGuid& StageGuid, FName StageId, EEGQuestStageExitReason Reason) override;
	virtual void OnObjectiveResolved_Implementation(const FEGQuestSnapshotObjective& Objective) override;
	virtual void OnQuestEnded_Implementation(EEGQuestResult Result) override;
};
