// Copyright Epic Games, Inc. All Rights Reserved.

#include "EFEQSTest_ConeShape.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"

UEFEQSTest_ConeShape::UEFEQSTest_ConeShape(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Cost = EEnvTestCost::Low;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(true);

	ConeTarget = UEnvQueryContext_Querier::StaticClass();
	ConeAngle = 90.0f;
	MaxDistance = 1000.0f;
}

void UEFEQSTest_ConeShape::RunTest(FEnvQueryInstance& QueryInstance) const
{
	UObject* QueryOwner = QueryInstance.Owner.Get();
	if (QueryOwner == nullptr)
	{
		return;
	}

	TArray<FVector> ContextLocations;
	QueryInstance.PrepareContext(ConeTarget, ContextLocations);

	if (ContextLocations.IsEmpty())
	{
		return;
	}

	const float HalfConeAngleRad = FMath::DegreesToRadians(ConeAngle * 0.5f);
	const float CosHalfConeAngle = FMath::Cos(HalfConeAngleRad);

	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		const FVector ItemLocation = GetItemLocation(QueryInstance, It.GetIndex());
		float MinAngleCos = -1.0f; // Initialize with lowest possible cosine value

		for (const FVector& ContextLocation : ContextLocations)
		{
			const FVector ToTarget = (ContextLocation - ItemLocation).GetSafeNormal();
			const FVector ToItem = (ItemLocation - ContextLocation).GetSafeNormal();
			const float Distance = FVector::Distance(ItemLocation, ContextLocation);

			// Calculate the cosine of the angle between vectors
			const float AngleCos = FVector::DotProduct(ToTarget, -ToItem);

			// Update the minimum angle cosine if this one is larger (smaller angle)
			if (Distance <= MaxDistance)
			{
				MinAngleCos = FMath::Max(MinAngleCos, AngleCos);
			}
		}

		// Determine if the item is within the cone
		const bool bIsInCone = MinAngleCos >= CosHalfConeAngle;

		It.SetScore(TestPurpose, FilterType, bIsInCone, !bIsInCone);
	}
}

FText UEFEQSTest_ConeShape::GetDescriptionTitle() const
{
	return FText::FromString(FString::Printf(TEXT("%s: cone shape"), *UEnvQueryTypes::GetShortTypeName(ConeTarget).ToString()));
}

FText UEFEQSTest_ConeShape::GetDescriptionDetails() const
{
	return FText::FromString(FString::Printf(TEXT("angle: %.1f degrees\nmax distance: %.0f"),
		ConeAngle, MaxDistance));
}
