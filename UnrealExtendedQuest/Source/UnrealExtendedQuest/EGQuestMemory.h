// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "EGQuestMemory.generated.h"


/**
 * The nodes one quest context has entered.
 *
 * Context-local scratch, not save data: a run's durable history travels as
 * FEGQuestRunRecord::VisitedNodeGuids, and UEGQuestContext::History is a plain member rather than a
 * UPROPERTY, so nothing here is ever reflected or serialized.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDQUEST_API FEGQuestHistory
{
	GENERATED_USTRUCT_BODY()
public:
	FEGQuestHistory() {}

	void Add(const FGuid& NodeGUID);

public:
	// Set of already visited node GUIDs
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quest|History")
	TSet<FGuid> VisitedNodeGUIDs;
};
