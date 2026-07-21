#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BTComposite_SelectRandomCount.generated.h"

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UBTComposite_SelectRandomCount : public UBTCompositeNode
{
    GENERATED_BODY()

public:
    UBTComposite_SelectRandomCount(const FObjectInitializer& ObjectInitializer);

    UPROPERTY(EditAnywhere, Category = "Repeat Settings")
    TArray<int32> MinRepeat;

    UPROPERTY(EditAnywhere, Category = "Repeat Settings")
    TArray<int32> MaxRepeat;

    // Blackboard'da oluþturman gereken deðiþkenler (Integer)
    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector CurrentChildIndexKey;

    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector ConsecutiveCountKey;

    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector CurrentThresholdKey;

    virtual int32 GetNextChildHandler(struct FBehaviorTreeSearchData& SearchData, int32 PrevChild, EBTNodeResult::Type LastResult) const override;

#if WITH_EDITOR
    virtual FString GetStaticDescription() const override;
    virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif
};