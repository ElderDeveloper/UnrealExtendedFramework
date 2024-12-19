// Fill out your copyright notice in the Description page of Project Settings.


#include "BTComposite_SelectRandomWeight.h"
#include "Kismet/KismetArrayLibrary.h"



UBTComposite_SelectRandomWeight::UBTComposite_SelectRandomWeight(const FObjectInitializer& ObjectInitializer)
{
	NodeName = "Select Random Weighted";
	DecayFactor = 0.1f;
}

int32 UBTComposite_SelectRandomWeight::GetNextChildHandler(FBehaviorTreeSearchData& SearchData, int32 PrevChild,EBTNodeResult::Type LastResult) const
{
	// Store Base Weights
	ResizeArrays();
	DecayedChildWeights = ChildWeights;
	
	
	// Apply decay factor to weights and update execution counts
	for (int32 i = 0; i < GetChildrenNum(); i++)
	{
		DecayedChildWeights[i] = ChildWeights[i] - (ExecutionCounts[i] * DecayFactor);
		if (DecayedChildWeights[i] < 0)
		{
			DecayedChildWeights[i] = 0;
		}
	}
	
	// Calculate total weight
	float TotalWeight = 0;
	for (int32 ChildIndex = 0; ChildIndex < GetChildrenNum(); ChildIndex++)
	{
		TotalWeight += DecayedChildWeights.IsValidIndex(ChildIndex) ? DecayedChildWeights[ChildIndex] : 0;
	}
	
	// Generate a random number between 0 and total weight
	const float RandomNumber = FMath::RandRange(0.0f, TotalWeight);

	
	// Iterate through the children and find the child whose weight range contains the random number
	float CurrentWeight = 0;
	TArray<int32> ChildWeightRanges;
	
	for (int32 ChildIndex = 0; ChildIndex < GetChildrenNum(); ChildIndex++)
	{
		CurrentWeight +=  DecayedChildWeights[ChildIndex];
		
		if (RandomNumber < CurrentWeight)
		{
			ChildWeightRanges.Add(ChildIndex);
		}
	}
	
	if (ChildWeightRanges.Num() > 0)
	{
		/*
		for (const auto i : ChildWeightRanges)
		{
			GEngine->AddOnScreenDebugMessage(-1 , 5.0f , FColor::Emerald , FString::Printf(TEXT("The Child Weight Range: %d") , i));
		}
		*/
		
		const int32 RandomIndex = FMath::RandRange(0 , ChildWeightRanges.Num() - 1);
		if (ChildWeightRanges.IsValidIndex(RandomIndex))
		{
			const int32 RandomChild = ChildWeightRanges[RandomIndex];
			AddOneToExecutionCount(RandomChild);
			DetailedDebug();
			return RandomChild;
		}
	}
	
	// This should never happen, but just in case return the last child index
	return GetChildrenNum() - 1;
	
}





void UBTComposite_SelectRandomWeight::AddOneToExecutionCount(int32 ChildIndex) const
{
	ExecutionCounts[ChildIndex] += 1;
	
	for (int32 i = 0; i < ExecutionCounts.Num(); i++)
	{
		if (i != ChildIndex)
		{
			ExecutionCounts[i] = 0; 
		}
	}
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




void UBTComposite_SelectRandomWeight::DetailedDebug() const
{
	/*
	for (int32 i = 0 ; i < ExecutionCounts.Num(); i++)
	{
		GEngine->AddOnScreenDebugMessage( -1, 5.f, FColor::Orange, FString::Printf(TEXT("%d , ExecutionCounts[%d] "), i, ExecutionCounts[i]));
	}
	
	for (int32 i = 0 ; i < DecayedChildWeights.Num(); i++)
	{
		GEngine->AddOnScreenDebugMessage( -1, 5.f, FColor::Green, FString::Printf(TEXT("%d , Weighed[%f] "), i, DecayedChildWeights[i]));
	}
	*/
}
