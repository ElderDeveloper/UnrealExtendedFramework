// Fill out your copyright notice in the Description page of Project Settings.


#include "BTComposite_SelectRandom.h"

UBTComposite_SelectRandom::UBTComposite_SelectRandom(const FObjectInitializer& ObjectInitializer)
{
	NodeName = "Select Random";
}

int32 UBTComposite_SelectRandom::GetNextChildHandler(FBehaviorTreeSearchData& SearchData, int32 PrevChild,EBTNodeResult::Type LastResult) const
{
	const int32 RandomIndex = FMath::RandRange(0, GetChildrenNum() - 1);
	return RandomIndex;
}

#if WITH_EDITOR
FName UBTComposite_SelectRandom::GetNodeIconName() const
{
	return "Select Random";
}

FString UBTComposite_SelectRandom::GetStaticDescription() const
{
	return "Select Random Task";
}
#endif
