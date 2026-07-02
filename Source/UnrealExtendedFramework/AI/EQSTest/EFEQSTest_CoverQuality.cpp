// Copyright Epic Games, Inc. All Rights Reserved.

#include "EFEQSTest_CoverQuality.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "CollisionQueryParams.h"

UEFEQSTest_CoverQuality::UEFEQSTest_CoverQuality(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Cost = EEnvTestCost::High;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(true);

	ThreatContext = UEnvQueryContext_Querier::StaticClass();
	MinCoverHeight = 150.0f;
	MaxDistance = 2000.0f;
	LineOfSightWeight = 0.4f;
	HeightWeight = 0.3f;
	DistanceWeight = 0.3f;
	SightCheckHeight = 180.0f;
	NumHeightChecks = 5;
}

void UEFEQSTest_CoverQuality::RunTest(FEnvQueryInstance& QueryInstance) const
{
    UObject* QueryOwner = QueryInstance.Owner.Get();
    if (QueryOwner == nullptr)
    {
        return;
    }

    TArray<FVector> ThreatLocations;
    QueryInstance.PrepareContext(ThreatContext, ThreatLocations);

    if (ThreatLocations.IsEmpty())
    {
        return;
    }

    UWorld* World = QueryOwner->GetWorld();
    if (World == nullptr)
    {
        return;
    }

    for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
    {
        const FVector ItemLocation = GetItemLocation(QueryInstance, It.GetIndex());
        float BestScore = -1.0f;
        bool bIsValid = false;

        for (const FVector& ThreatLocation : ThreatLocations)
        {
            // Calculate distance score
            const float Distance = FVector::Distance(ItemLocation, ThreatLocation);
            if (Distance > MaxDistance)
            {
                continue;
            }
            const float DistanceScore = 1.0f - FMath::Min(1.0f, Distance / MaxDistance);

            // Calculate line of sight score
            float LineOfSightScore = 0.0f;
            const FVector ThreatEyeLocation = FVector(ThreatLocation.X, ThreatLocation.Y, ThreatLocation.Z + SightCheckHeight);
            const FVector CoverLocation = FVector(ItemLocation.X, ItemLocation.Y, ItemLocation.Z + SightCheckHeight);

            FCollisionQueryParams QueryParams;
            QueryParams.AddIgnoredActor(Cast<AActor>(QueryOwner));

            bool bHasLineOfSight = World->LineTraceTestByChannel(
                ThreatEyeLocation,
                CoverLocation,
                ECC_Visibility,
                QueryParams
            );

            // Invert the result since we want cover positions that block line of sight
            LineOfSightScore = bHasLineOfSight ? 1.0f : 0.0f;

            // Calculate height score
            float HeightScore = 0.0f;
            int32 ValidHeightPoints = 0;

            // Check multiple height points
            for (int32 i = 0; i < NumHeightChecks; ++i)
            {
                const float CheckHeight = (static_cast<float>(i) / NumHeightChecks) * MinCoverHeight;
                const FVector CheckLocation = FVector(ItemLocation.X, ItemLocation.Y, ItemLocation.Z + CheckHeight);

                bool bHeightBlocked = World->LineTraceTestByChannel(
                    ThreatEyeLocation,
                    CheckLocation,
                    ECC_Visibility,
                    QueryParams
                );

                if (bHeightBlocked)
                {
                    ++ValidHeightPoints;
                }
            }

            HeightScore = static_cast<float>(ValidHeightPoints) / NumHeightChecks;

            // Combine scores using weights
            const float CombinedScore = (LineOfSightScore * LineOfSightWeight) +
                                        (HeightScore * HeightWeight) +
                                        (DistanceScore * DistanceWeight);

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

FText UEFEQSTest_CoverQuality::GetDescriptionTitle() const
{
	return FText::FromString(FString::Printf(TEXT("%s: cover quality"), *UEnvQueryTypes::GetShortTypeName(ThreatContext).ToString()));
}

FText UEFEQSTest_CoverQuality::GetDescriptionDetails() const
{
	return FText::FromString(FString::Printf(TEXT("min height: %.0f\nmax distance: %.0f\nLoS weight: %.1f\nheight weight: %.1f\ndist weight: %.1f"),
		MinCoverHeight, MaxDistance, LineOfSightWeight, HeightWeight, DistanceWeight));
}