// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "UnrealExtendedQuest/Nodes/EGQuestNode.h"
#include "UnrealExtendedQuest/EGQuestTypes.h"

#include "EGQuestNode_End.generated.h"


/**
 * A terminal that stamps a result on the quest.
 *
 * It is not a join and has no behaviour of its own: like any destination it fires when every arrow
 * pointing into it is satisfied, and when it does, the quest is over. Its enter events still fire.
 */
UCLASS(BlueprintType, ClassGroup = "Quest")
class UNREALEXTENDEDQUEST_API UEGQuestNode_End : public UEGQuestNode
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Quest|End")
	EEGQuestResult GetQuestResult() const { return QuestResult; }
	void SetQuestResult(EEGQuestResult InResult) { QuestResult = InResult; }
	// Begin UObject Interface.

	/** @return a one line description of an object. */
	FString GetDesc() override;

#if WITH_EDITOR
	FString GetNodeTypeString() const override { return TEXT("End"); }
#endif

	// Helper functions to get the names of some properties. Used by the QuestPluginEditor module.
	static FName GetMemberNameQuestResult() { return GET_MEMBER_NAME_CHECKED(UEGQuestNode_End, QuestResult); }

protected:
	UPROPERTY(EditAnywhere, Category = "End")
	EEGQuestResult QuestResult = EEGQuestResult::Completed;
};
