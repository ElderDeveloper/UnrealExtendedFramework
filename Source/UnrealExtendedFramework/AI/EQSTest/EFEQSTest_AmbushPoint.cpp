// Copyright Epic Games, Inc. All Rights Reserved.

#include "EFEQSTest_AmbushPoint.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "NavigationSystem.h"
#include "AI/Navigation/NavigationTypes.h"
#include "CollisionQueryParams.h"

UEFEQSTest_AmbushPoint::UEFEQSTest_AmbushPoint(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Cost = EEnvTestCost::High;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(true);

	TargetPathContext = UEnvQueryContext_Querier::StaticClass();
	OptimalDistance = 800.0f;
	MaxDistance = 2000.0f;
	SightCheckHeight = 180.0f;
	DistanceWeight = 0.3f;
	LineOfSightWeight = 0.2f;
	CoverWeight = 0.3f;
	EscapeWeight = 0.2f;
	CoverCheckRadius = 200.0f;
	NumCoverChecks = 8;
	EscapeCheckRadius = 300.0f;
	NumEscapeChecks = 6;
}

void UEFEQSTest_AmbushPoint::RunTest(FEnvQueryInstance& QueryInstance) const
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

    TArray<FVector> PathPoints;
    QueryInstance.PrepareContext(TargetPathContext, PathPoints);

    if (PathPoints.IsEmpty())
    {
        return;
    }

    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (NavSys == nullptr)
    {
        return;
    }

    for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
    {
        const FVector ItemLocation = GetItemLocation(QueryInstance, It.GetIndex());
        float BestScore = -1.0f;
        bool bIsValid = false;

        // Calculate distance and line of sight score for each path point
        for (const FVector& PathPoint : PathPoints)
        {
            // Distance scoring
            const float Distance = FVector::Distance(ItemLocation, PathPoint);
            if (Distance > MaxDistance)
            {
                continue;
            }

            // Score is best at optimal distance
            const float DistanceScore = 1.0f - FMath::Abs(Distance - OptimalDistance) / MaxDistance;

            // Line of sight scoring
            const FVector StartTrace = FVector(ItemLocation.X, ItemLocation.Y, ItemLocation.Z + SightCheckHeight);
            const FVector EndTrace = FVector(PathPoint.X, PathPoint.Y, PathPoint.Z + SightCheckHeight);

            FCollisionQueryParams QueryParams;
            QueryParams.AddIgnoredActor(Cast<AActor>(QueryOwner));

            bool bHasLineOfSight = !World->LineTraceTestByChannel(
                StartTrace,
                EndTrace,
                ECC_Visibility,
                QueryParams
            );

            const float LineOfSightScore = bHasLineOfSight ? 1.0f : 0.0f;

            // Cover quality scoring
            float CoverScore = 0.0f;
            int32 CoveredDirections = 0;

            for (int32 i = 0; i < NumCoverChecks; ++i)
            {
                const float Angle = (2.0f * PI * i) / NumCoverChecks;
                const FVector CheckDir = FRotator(0.0f, FMath::RadiansToDegrees(Angle), 0.0f).Vector();
                const FVector CheckPoint = ItemLocation + CheckDir * CoverCheckRadius;

                bool bHasCover = World->LineTraceTestByChannel(
                    StartTrace,
                    CheckPoint,
                    ECC_Visibility,
                    QueryParams
                );

                if (bHasCover)
                {
                    ++CoveredDirections;
                }
            }

            CoverScore = static_cast<float>(CoveredDirections) / NumCoverChecks;

            // Escape options scoring
            float EscapeScore = 0.0f;
            int32 ValidEscapeRoutes = 0;

            for (int32 i = 0; i < NumEscapeChecks; ++i)
            {
                const float Angle = (2.0f * PI * i) / NumEscapeChecks;
                const FVector CheckDir = FRotator(0.0f, FMath::RadiansToDegrees(Angle), 0.0f).Vector();
                const FVector CheckPoint = ItemLocation + CheckDir * EscapeCheckRadius;

                FPathFindingQuery PathQuery;
                PathQuery.StartLocation = ItemLocation;
                PathQuery.EndLocation = CheckPoint;
                PathQuery.NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);

                FPathFindingResult PathResult = NavSys->FindPathSync(PathQuery);
                if (PathResult.IsSuccessful())
                {
                    ++ValidEscapeRoutes;
                }
            }

            EscapeScore = static_cast<float>(ValidEscapeRoutes) / NumEscapeChecks;

            // Combine scores using weights
            const float CombinedScore = (DistanceScore * DistanceWeight) +
                                        (LineOfSightScore * LineOfSightWeight) +
                                        (CoverScore * CoverWeight) +
                                        (EscapeScore * EscapeWeight);

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

FText UEFEQSTest_AmbushPoint::GetDescriptionTitle() const
{
	return FText::FromString(FString::Printf(TEXT("%s: ambush point"), *UEnvQueryTypes::GetShortTypeName(TargetPathContext).ToString()));
}

FText UEFEQSTest_AmbushPoint::GetDescriptionDetails() const
{
	return FText::FromString(FString::Printf(TEXT("optimal distance: %.0f\nmax distance: %.0f\ndist weight: %.1f\nLoS weight: %.1f\ncover weight: %.1f\nescape weight: %.1f"),
		OptimalDistance, MaxDistance, DistanceWeight, LineOfSightWeight, CoverWeight, EscapeWeight));
}