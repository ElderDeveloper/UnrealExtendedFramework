// Fill out your copyright notice in the Description page of Project Settings.


#include "BTComposite_SelectRandomWeight.h"
#include "Kismet/KismetArrayLibrary.h"



UBTComposite_SelectRandomWeight::UBTComposite_SelectRandomWeight(const FObjectInitializer& ObjectInitializer)
{
	NodeName = "Select Random Weighted";
	DecayFactor = 0.1f;
}


// In BTComposite_SelectRandomWeight.cpp
int32 UBTComposite_SelectRandomWeight::GetNextChildHandler(FBehaviorTreeSearchData& SearchData, int32 PrevChild, EBTNodeResult::Type LastResult) const
{
    // Debug log
    UE_LOG(LogTemp, Warning, TEXT("=== SelectRandomWeight ==="));
    UE_LOG(LogTemp, Warning, TEXT("PrevChild: %d, ReturnToParent: %d, NotInitialized: %d"),
        PrevChild, BTSpecialChild::ReturnToParent, BTSpecialChild::NotInitialized);

    // Geçerli bir child çalýþýp bittiyse parent'a dön
    if (PrevChild >= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Child %d finished, returning to parent"), PrevChild);
        return BTSpecialChild::ReturnToParent;
    }

    // Ýlk çalýþma - random child seç
    ResizeArrays();
    DecayedChildWeights = ChildWeights;

    for (int32 i = 0; i < GetChildrenNum(); i++)
    {
        DecayedChildWeights[i] = ChildWeights[i] - (ExecutionCounts[i] * DecayFactor);
        if (DecayedChildWeights[i] < 0)
        {
            DecayedChildWeights[i] = 0;
        }
    }

    float TotalWeight = 0;
    for (int32 ChildIndex = 0; ChildIndex < GetChildrenNum(); ChildIndex++)
    {
        TotalWeight += DecayedChildWeights.IsValidIndex(ChildIndex) ? DecayedChildWeights[ChildIndex] : 0;
    }

    if (TotalWeight <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("TotalWeight is 0, returning last child"));
        return GetChildrenNum() - 1;
    }

    const float RandomNumber = FMath::FRandRange(0.0f, TotalWeight);
    float CurrentWeight = 0;

    for (int32 ChildIndex = 0; ChildIndex < GetChildrenNum(); ChildIndex++)
    {
        CurrentWeight += DecayedChildWeights[ChildIndex];
        if (RandomNumber <= CurrentWeight)
        {
            AddOneToExecutionCount(ChildIndex);
            UE_LOG(LogTemp, Warning, TEXT("Selected child %d (Random: %f, Weight: %f)"),
                ChildIndex, RandomNumber, CurrentWeight);
            return ChildIndex;
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("Fallback to last child"));
    return GetChildrenNum() - 1;
}
void UBTComposite_SelectRandomWeight::CleanupMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory,EBTMemoryClear::Type CleanupType) const{}


void UBTComposite_SelectRandomWeight::InitializeMemory(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory,EBTMemoryInit::Type InitType) const{}


void UBTComposite_SelectRandomWeight::AddOneToExecutionCount(int32 ChildIndex) const
{
	// Apply decay to all counts before incrementing
	ExecutionCounts[ChildIndex] += 1;
}


void UBTComposite_SelectRandomWeight::ResizeArrays() const
{
	if (ExecutionCounts.Num() != GetChildrenNum())
	{
		ExecutionCounts.Empty();
		for (int32 ChildIndex = 0; ChildIndex < GetChildrenNum(); ChildIndex++)
		{
			ExecutionCounts.Add(ChildIndex,0);
		}
	}

	if (ChildWeights.Num() != GetChildrenNum())
	{
		ChildWeights.SetNum(GetChildrenNum(),EAllowShrinking::Yes);
		
		for (int32 ChildIndex = 0; ChildIndex < GetChildrenNum(); ChildIndex++)
		{
			if (ChildWeights[ChildIndex] == 0)
			{
				ChildWeights[ChildIndex] = 1;
			}
		}
	}
}


#if WITH_EDITOR
FName UBTComposite_SelectRandomWeight::GetNodeIconName() const
{
	return "Select Random Task";
}


FString UBTComposite_SelectRandomWeight::GetStaticDescription() const
{
	FString TaskDescription = "Select Random Task";
	for (const auto i : ChildWeights)
	{
		TaskDescription += "\nWeight: " + FString::SanitizeFloat(i);
	}
	TaskDescription += "\nDecay Factor: " + FString::SanitizeFloat(DecayFactor);
	return TaskDescription;
}

#endif