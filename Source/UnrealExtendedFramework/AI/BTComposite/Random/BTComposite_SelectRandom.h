// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BTComposite_SelectRandom.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UBTComposite_SelectRandom : public UBTCompositeNode
{
	GENERATED_BODY()

public:
	UBTComposite_SelectRandom(const FObjectInitializer& ObjectInitializer);
	virtual int32 GetNextChildHandler(struct FBehaviorTreeSearchData& SearchData, int32 PrevChild, EBTNodeResult::Type LastResult) const override;

#if WITH_EDITOR
	virtual FName GetNodeIconName() const override;
	virtual FString GetStaticDescription() const override;
#endif
};
