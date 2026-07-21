// Copyright Epic Games, Inc. All Rights Reserved.

#include "EFEQSTest_HeightDifference.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"

UEFEQSTest_HeightDifference::UEFEQSTest_HeightDifference(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Cost = EEnvTestCost::Low;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(true);

	HeightReferenceTo = UEnvQueryContext_Querier::StaticClass();
	bUseAbsoluteValue = true;
}

void UEFEQSTest_HeightDifference::RunTest(FEnvQueryInstance& QueryInstance) const
{
	UObject* QueryOwner = QueryInstance.Owner.Get();
	if (QueryOwner == nullptr)
	{
		return;
	}

	TArray<FVector> ContextLocations;
	QueryInstance.PrepareContext(HeightReferenceTo, ContextLocations);

	if (ContextLocations.IsEmpty())
	{
		return;
	}

	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		const FVector ItemLocation = GetItemLocation(QueryInstance, It.GetIndex());
		float MinHeightDifference = FLT_MAX;
		bool bIsValid = false;

		for (const FVector& ContextLocation : ContextLocations)
		{
			const float HeightDifference = bUseAbsoluteValue ?
				FMath::Abs(ItemLocation.Z - ContextLocation.Z) :
				ItemLocation.Z - ContextLocation.Z;

			MinHeightDifference = FMath::Min(MinHeightDifference, HeightDifference);
			bIsValid = true;
		}

		It.SetScore(TestPurpose, FilterType, bIsValid, !bIsValid);
	}
}


FText UEFEQSTest_HeightDifference::GetDescriptionTitle() const
{
	return FText::FromString(FString::Printf(TEXT("%s: height difference"), *UEnvQueryTypes::GetShortTypeName(HeightReferenceTo).ToString()));
}

FText UEFEQSTest_HeightDifference::GetDescriptionDetails() const
{
	return FText::FromString(FString::Printf(TEXT("use absolute value: %s"),
		bUseAbsoluteValue ? TEXT("yes") : TEXT("no")));
}
