// Copyright Epic Games, Inc. All Rights Reserved.

#include "EFEQSTest_JumpScarePosition.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "CollisionQueryParams.h"

UEFEQSTest_JumpScarePosition::UEFEQSTest_JumpScarePosition(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Cost = EEnvTestCost::High;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(true);

	TargetContext = UEnvQueryContext_Querier::StaticClass();
	MinDistance = 500.0f;
	MaxDistance = 2000.0f;
	OptimalDistance = 1000.0f;
	DistanceWeight = 0.4f;
	ApproachAngleWeight = 0.3f;
	VisibilityWeight = 0.3f;
	OptimalApproachAngle = 150.0f; // Slightly from behind for maximum scare
	SightCheckHeight = 180.0f;
	bCheckTargetVisibility = true;
	VisibilityPenalty = 0.3f; // Visible positions score 30% of their original score
}

void UEFEQSTest_JumpScarePosition::RunTest(FEnvQueryInstance& QueryInstance) const
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

	// Get target locations and rotations (usually the player)
	TArray<FVector> TargetLocations;
	TArray<FRotator> TargetRotations;
	QueryInstance.PrepareContext(TargetContext, TargetLocations);
	QueryInstance.PrepareContext(TargetContext, TargetRotations);

	if (TargetLocations.IsEmpty() || TargetRotations.IsEmpty())
	{
		return;
	}

	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		const FVector ItemLocation = GetItemLocation(QueryInstance, It.GetIndex());
		float BestScore = -1.0f;
		bool bIsValid = false;

		for (int32 ContextIndex = 0; ContextIndex < TargetLocations.Num(); ++ContextIndex)
		{
			const FVector& TargetLocation = TargetLocations[ContextIndex];
			const FRotator& TargetRotation = TargetRotations[ContextIndex];

			// Calculate distance score
			const float Distance = FVector::Distance(ItemLocation, TargetLocation);
			if (Distance > MaxDistance || Distance < MinDistance)
			{
				continue;
			}

			// Score is best at optimal distance
			float DistanceScore;
			if (Distance <= OptimalDistance)
			{
				DistanceScore = FMath::Lerp(0.0f, 1.0f, Distance / OptimalDistance);
			}
			else
			{
				DistanceScore = FMath::Lerp(1.0f, 0.0f, (Distance - OptimalDistance) / (MaxDistance - OptimalDistance));
			}

			// Calculate approach angle score
			const FVector ToItem = (ItemLocation - TargetLocation).GetSafeNormal();
			const FVector TargetForward = TargetRotation.Vector();
			const float AngleRadians = FMath::Acos(FVector::DotProduct(ToItem, TargetForward));
			const float AngleDegrees = FMath::RadiansToDegrees(AngleRadians);
			
			// Score is highest when angle matches the optimal approach angle
			const float AngleScore = 1.0f - FMath::Abs(AngleDegrees - OptimalApproachAngle) / 180.0f;

			// Calculate visibility score
			float VisibilityScore = 1.0f;
			if (bCheckTargetVisibility)
			{
				FCollisionQueryParams QueryParams;
				QueryParams.AddIgnoredActor(Cast<AActor>(QueryOwner));

				// Check if position is visible to target
				const FVector StartTrace = FVector(TargetLocation.X, TargetLocation.Y, TargetLocation.Z + SightCheckHeight);
				const FVector EndTrace = FVector(ItemLocation.X, ItemLocation.Y, ItemLocation.Z + SightCheckHeight);

				bool bIsVisible = !World->LineTraceTestByChannel(
					StartTrace,
					EndTrace,
					ECC_Visibility,
					QueryParams
				);

				// Apply visibility penalty if position is visible
				if (bIsVisible)
				{
					VisibilityScore = VisibilityPenalty;
				}
			}

			// Combine scores using weights
			const float CombinedScore = (DistanceScore * DistanceWeight) +
								  (AngleScore * ApproachAngleWeight) +
								  (VisibilityScore * VisibilityWeight);

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

FText UEFEQSTest_JumpScarePosition::GetDescriptionTitle() const
{
	return FText::FromString(FString::Printf(TEXT("%s: jump scare position"), *UEnvQueryTypes::GetShortTypeName(TargetContext).ToString()));
}

FText UEFEQSTest_JumpScarePosition::GetDescriptionDetails() const
{
	return FText::FromString(FString::Printf(TEXT("optimal distance: %.0f\noptimal angle: %.0f\ndist weight: %.1f\nangle weight: %.1f\nvisibility weight: %.1f"),
		OptimalDistance, OptimalApproachAngle, DistanceWeight, ApproachAngleWeight, VisibilityWeight));
}