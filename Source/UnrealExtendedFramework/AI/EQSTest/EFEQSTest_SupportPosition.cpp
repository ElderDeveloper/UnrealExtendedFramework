// Copyright Epic Games, Inc. All Rights Reserved.

#include "EFEQSTest_SupportPosition.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "CollisionQueryParams.h"

UEFEQSTest_SupportPosition::UEFEQSTest_SupportPosition(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Cost = EEnvTestCost::High;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(true);

	AllyContext = UEnvQueryContext_Querier::StaticClass();
	ThreatContext = UEnvQueryContext_Querier::StaticClass();
	OptimalSupportDistance = 800.0f;
	MaxSupportDistance = 2000.0f;
	DistanceWeight = 0.4f;
	LineOfSightWeight = 0.3f;
	ThreatAvoidanceWeight = 0.3f;
	SightCheckHeight = 180.0f;
	MinThreatDistance = 1000.0f;
}

void UEFEQSTest_SupportPosition::RunTest(FEnvQueryInstance& QueryInstance) const
{
	UObject* QueryOwner = QueryInstance.Owner.Get();
	if (QueryOwner == nullptr)
	{
		return;
	}

	UWorld* World = QueryOwner->GetWorld();
	if (World == nullptr)
	{
		return;
	}

	// Get ally locations
	TArray<FVector> AllyLocations;
	QueryInstance.PrepareContext(AllyContext, AllyLocations);

	if (AllyLocations.IsEmpty())
	{
		return;
	}

	// Get threat locations
	TArray<FVector> ThreatLocations;
	QueryInstance.PrepareContext(ThreatContext, ThreatLocations);

	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		const FVector ItemLocation = GetItemLocation(QueryInstance, It.GetIndex());
		float BestScore = -1.0f;
		bool bIsValid = false;

		// Calculate scores for each ally
		for (const FVector& AllyLocation : AllyLocations)
		{
			// Calculate distance score
			const float Distance = FVector::Distance(ItemLocation, AllyLocation);
			if (Distance > MaxSupportDistance)
			{
				continue;
			}

			// Score is best at optimal distance, decreasing as we get closer or further
			const float DistanceScore = 1.0f - FMath::Abs(Distance - OptimalSupportDistance) / MaxSupportDistance;

			// Calculate line of sight score
			float LineOfSightScore = 0.0f;
			const FVector StartTrace = FVector(ItemLocation.X, ItemLocation.Y, ItemLocation.Z + SightCheckHeight);
			const FVector EndTrace = FVector(AllyLocation.X, AllyLocation.Y, AllyLocation.Z + SightCheckHeight);

			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(Cast<AActor>(QueryOwner));

			// Check if we have line of sight to ally
			bool bHasLineOfSight = !World->LineTraceTestByChannel(
				StartTrace,
				EndTrace,
				ECC_Visibility,
				QueryParams
			);

			LineOfSightScore = bHasLineOfSight ? 1.0f : 0.0f;

			// Calculate threat avoidance score
			float ThreatScore = 1.0f;
			if (!ThreatLocations.IsEmpty())
			{
				for (const FVector& ThreatLocation : ThreatLocations)
				{
					const float ThreatDistance = FVector::Distance(ItemLocation, ThreatLocation);
					if (ThreatDistance < MinThreatDistance)
					{
						// Reduce score based on proximity to threat
						ThreatScore = FMath::Min(ThreatScore, ThreatDistance / MinThreatDistance);
					}
				}
			}

			// Combine scores using weights
			const float CombinedScore = (DistanceScore * DistanceWeight) +
								  (LineOfSightScore * LineOfSightWeight) +
								  (ThreatScore * ThreatAvoidanceWeight);

			// Update best score
			if (CombinedScore > BestScore)
			{
				BestScore = CombinedScore;
				bIsValid = true;
			}
		}

		It.SetScore(TestPurpose, FilterType, bIsValid, !bIsValid);
	}
}

FText UEFEQSTest_SupportPosition::GetDescriptionTitle() const
{
	return FText::FromString(FString::Printf(TEXT("%s: support position"), *UEnvQueryTypes::GetShortTypeName(AllyContext).ToString()));
}

FText UEFEQSTest_SupportPosition::GetDescriptionDetails() const
{
	return FText::FromString(FString::Printf(TEXT("optimal distance: %.0f\nmax distance: %.0f\ndist weight: %.1f\nLoS weight: %.1f\nthreat weight: %.1f"),
		OptimalSupportDistance, MaxSupportDistance, DistanceWeight, LineOfSightWeight, ThreatAvoidanceWeight));
}