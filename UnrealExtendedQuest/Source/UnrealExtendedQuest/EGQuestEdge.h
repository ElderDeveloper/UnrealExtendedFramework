// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "UnrealExtendedQuest/EGQuestTypes.h"

#include "EGQuestEdge.generated.h"

class UEGQuestPluginSettings;
class UEGQuestContext;
class UEGQuestNode;
class UEGQuestGraph;

/**
 * An arrow: pure direction plus an outcome type.
 *
 * It carries no text, conditions or events. An arrow is satisfied when its source objective reached
 * the matching outcome, and a node fires when every arrow pointing into it is satisfied - so the
 * arrow itself never decides anything. Convergence is the AND; multiple paths are the OR.
 *
 * The same struct also wires a stage to the objectives it owns. Those edges are ownership, not
 * routing, and their Outcome is ignored.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDQUEST_API FEGQuestEdge
{
	GENERATED_USTRUCT_BODY()

public:
	FEGQuestEdge(int32 InTargetIndex = INDEX_NONE) : TargetIndex(InTargetIndex) {}

	//
	// ICppStructOps Interface
	//

	bool operator==(const FEGQuestEdge& Other) const
	{
		return TargetIndex == Other.TargetIndex && Outcome == Other.Outcome;
	}

	bool operator!=(const FEGQuestEdge& Other) const
	{
		return !(*this == Other);
	}

	//
	// Own methods
	//

	// Returns if the Edge is valid, has the TargetIndex non negative
	bool IsValid() const { return TargetIndex > INDEX_NONE; }

	static const FEGQuestEdge& GetInvalidEdge()
	{
		static FEGQuestEdge QuestEdge;
		return QuestEdge;
	}

	// Helper functions to get the names of some properties. Used by the QuestPluginEditor module.
	static FName GetMemberNameOutcome() { return GET_MEMBER_NAME_CHECKED(FEGQuestEdge, Outcome); }

public:
	// Index of the node in the Nodes TArray of the quest this edge is leading to
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "QuestEdge", Meta = (ClampMin = -1))
	int32 TargetIndex = INDEX_NONE;

	// The outcome of the source objective this arrow routes on. Ignored on a stage's edges to the
	// objectives it owns.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuestEdge")
	EEGQuestArrowOutcome Outcome = EEGQuestArrowOutcome::Success;
};

template<>
struct TStructOpsTypeTraits<FEGQuestEdge> : public TStructOpsTypeTraitsBase2<FEGQuestEdge>
{
	enum
	{
		WithIdenticalViaEquality = true
	};
};
