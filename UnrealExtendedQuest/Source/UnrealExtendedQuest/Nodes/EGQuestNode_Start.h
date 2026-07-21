// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "UnrealExtendedQuest/Nodes/EGQuestNode.h"
#include "UnrealExtendedQuest/EGQuestTypes.h"

#include "EGQuestNode_Start.generated.h"


/**
 * Possible entry point of the Quest.
 * Does not have text, the first satisfied child is picked if there is any.
 * Start nodes are tried in EntryPriority order.
 */
UCLASS(BlueprintType, ClassGroup = "Quest")
class UNREALEXTENDEDQUEST_API UEGQuestNode_Start : public UEGQuestNode
{
	GENERATED_BODY()

public:
	// Begin UObject Interface.

	/** @return a one line description of an object. */
	FString GetDesc() override;

#if WITH_EDITOR
	FString GetNodeTypeString() const override { return TEXT("Start"); }
#endif

	//
	// Begin own functions.
	//

	/**
	 * Ascending order among the graph's start nodes: lower is tried first.
	 *
	 * This is semantic, not cosmetic. UEGQuestGraph::StartNodes is compiler output sorted by this
	 * value, and UEGQuestComponent walks that array taking the first start node whose entry enters
	 * (via UEGQuestContext::StartFromEntry) - so this decides which entry point wins when several
	 * could start the quest. It used to be the node's canvas X.
	 */
	UFUNCTION(BlueprintPure, Category = "Quest|Start")
	int32 GetEntryPriority() const { return EntryPriority; }
	void SetEntryPriority(const int32 InPriority) { EntryPriority = FMath::Max(0, InPriority); }

	UFUNCTION(BlueprintPure, Category = "Quest|Start")
	FName GetTrackName() const { return TrackName.IsNone() ? FName(TEXT("Main")) : TrackName; }
	void SetTrackName(FName InName) { TrackName = InName.IsNone() ? FName(TEXT("Main")) : InName; }

	UFUNCTION(BlueprintPure, Category = "Quest|Start")
	EEGQuestTrackType GetTrackType() const { return TrackType; }
	void SetTrackType(EEGQuestTrackType InType) { TrackType = InType; }

	// Helper functions to get the names of some properties. Used by the QuestPluginEditor module.
	static FName GetMemberNameEntryPriority() { return GET_MEMBER_NAME_CHECKED(UEGQuestNode_Start, EntryPriority); }

protected:
	// See GetEntryPriority. Two start nodes sharing a value is an authoring error (the compiler falls
	// back to GUID order so a compile stays deterministic, and raises Quest.Start.DuplicateEntryPriority).
	UPROPERTY(EditAnywhere, Category = "Start", Meta = (ClampMin = "0"))
	int32 EntryPriority = 0;

	UPROPERTY(EditAnywhere, Category = "Start")
	FName TrackName = TEXT("Main");

	UPROPERTY(EditAnywhere, Category = "Start")
	EEGQuestTrackType TrackType = EEGQuestTrackType::Main;
};
