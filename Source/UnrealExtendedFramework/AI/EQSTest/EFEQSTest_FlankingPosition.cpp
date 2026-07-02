// Copyright Epic Games, Inc. All Rights Reserved.

#include "EFEQSTest_FlankingPosition.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "NavigationSystem.h"
#include "AI/Navigation/NavigationTypes.h"
#include "CollisionQueryParams.h"

UEFEQSTest_FlankingPosition::UEFEQSTest_FlankingPosition(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    Cost = EEnvTestCost::High;
    ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
    SetWorkOnFloatValues(true);

    TargetContext = UEnvQueryContext_Querier::StaticClass();
    MinDistance = 500.0f;
    MaxDistance = 2000.0f;
    OptimalDistance = 1000.0f;
    DistanceWeight = 0.4f;
    AngleWeight = 0.4f;
    OptimalFlankingAngle = 90.0f;
    bCheckLineOfSight = true;
    LineOfSightWeight = 0.2f;
}

void UEFEQSTest_FlankingPosition::RunTest(FEnvQueryInstance& QueryInstance) const
{
    UObject* QueryOwner = QueryInstance.Owner.Get();
    if (QueryOwner == nullptr)
    {
        return;
    }

    TArray<FVector> ContextLocations;
    TArray<FRotator> ContextRotations;
    QueryInstance.PrepareContext(TargetContext, ContextLocations);
    QueryInstance.PrepareContext(TargetContext, ContextRotations);

    if (ContextLocations.IsEmpty() || ContextRotations.IsEmpty())
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

        for (int32 ContextIndex = 0; ContextIndex < ContextLocations.Num(); ++ContextIndex)
        {
            const FVector& TargetLocation = ContextLocations[ContextIndex];
            const FRotator& TargetRotation = ContextRotations[ContextIndex];

            // Calculate distance score
            const float Distance = FVector::Distance(ItemLocation, TargetLocation);
            if (Distance > MaxDistance || Distance < MinDistance)
            {
                continue;
            }

            float DistanceScore;
            if (Distance <= OptimalDistance)
            {
                DistanceScore = FMath::Lerp(0.0f, 1.0f, Distance / OptimalDistance);
            }
            else
            {
                DistanceScore = FMath::Lerp(1.0f, 0.0f, (Distance - OptimalDistance) / (MaxDistance - OptimalDistance));
            }

            // Calculate angle score
            const FVector ToItem = (ItemLocation - TargetLocation).GetSafeNormal();
            const FVector TargetForward = TargetRotation.Vector();
            const float AngleRadians = FMath::Acos(FVector::DotProduct(ToItem, TargetForward));
            const float AngleDegrees = FMath::RadiansToDegrees(AngleRadians);

            // Score is highest when angle matches the optimal flanking angle
            const float AngleScore = 1.0f - FMath::Abs(AngleDegrees - OptimalFlankingAngle) / 180.0f;

            // Calculate line of sight score
            float LineOfSightScore = 1.0f;
            if (bCheckLineOfSight)
            {
                FCollisionQueryParams QueryParams;
                QueryParams.AddIgnoredActor(Cast<AActor>(QueryOwner));

                // Trace from item to target
                bool bHasLineOfSight = !World->LineTraceTestByChannel(
                    ItemLocation,
                    TargetLocation,
                    ECC_Visibility,
                    QueryParams
                );

                LineOfSightScore = bHasLineOfSight ? 1.0f : 0.0f;
            }

            // Combine scores using weights
            const float CombinedScore = (DistanceScore * DistanceWeight) +
                                      (AngleScore * AngleWeight) +
                                      (LineOfSightScore * LineOfSightWeight);

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

FText UEFEQSTest_FlankingPosition::GetDescriptionTitle() const
{
    return FText::FromString(FString::Printf(TEXT("%s: flanking position"), *UEnvQueryTypes::GetShortTypeName(TargetContext).ToString()));
}

FText UEFEQSTest_FlankingPosition::GetDescriptionDetails() const
{
    return FText::FromString(FString::Printf(TEXT("optimal distance: %.0f\noptimal angle: %.0f\ndist weight: %.1f\nangle weight: %.1f\nLoS weight: %.1f"),
        OptimalDistance, OptimalFlankingAngle, DistanceWeight, AngleWeight, LineOfSightWeight));
}
