// Copyright Epic Games, Inc. All Rights Reserved.

#include "EFEQSTest_EscapeRoute.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "NavigationSystem.h"
#include "AI/Navigation/NavigationTypes.h"

UEFEQSTest_EscapeRoute::UEFEQSTest_EscapeRoute(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Cost = EEnvTestCost::High;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(true);

	DistanceContext = UEnvQueryContext_Querier::StaticClass();
	MinDesiredDistance = 1000.0f;
	MaxDistance = 2000.0f;
	DirectDistanceWeight = 0.4f;
	PathOptionsWeight = 0.6f;
	PathCheckRadius = 300.0f;
	NumPathChecks = 8;
}

void UEFEQSTest_EscapeRoute::RunTest(FEnvQueryInstance& QueryInstance) const
{
    UObject* QueryOwner = QueryInstance.Owner.Get();
    if (QueryOwner == nullptr)
    {
        return;
    }

    TArray<FVector> ContextLocations;
    QueryInstance.PrepareContext(DistanceContext, ContextLocations);

    if (ContextLocations.IsEmpty())
    {
        return;
    }

    UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(QueryOwner->GetWorld());
    if (NavSys == nullptr)
    {
        return;
    }

    for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
    {
        const FVector ItemLocation = GetItemLocation(QueryInstance, It.GetIndex());
        float BestScore = -1.0f;
        bool bIsValid = false;

        for (const FVector& ContextLocation : ContextLocations)
        {
            // Calculate direct distance score
            const float Distance = FVector::Distance(ItemLocation, ContextLocation);
            if (Distance > MaxDistance)
            {
                continue;
            }

            // Normalize distance score (1.0 at MinDesiredDistance, decreasing as we get closer or further)
            const float DistanceScore = Distance < MinDesiredDistance ?
                Distance / MinDesiredDistance :
                1.0f - FMath::Min(1.0f, (Distance - MinDesiredDistance) / (MaxDistance - MinDesiredDistance));

            // Calculate path options score
            float PathOptionsScore = 0.0f;
            int32 ValidPathCount = 0;

            // Check multiple directions for available paths
            for (int32 i = 0; i < NumPathChecks; ++i)
            {
                // Calculate check point in a circle around the item location
                const float Angle = (2.0f * PI * i) / NumPathChecks;
                const FVector CheckDir = FRotator(0.0f, FMath::RadiansToDegrees(Angle), 0.0f).Vector();
                const FVector CheckPoint = ItemLocation + CheckDir * PathCheckRadius;

                // Test if we can path to this point
                FPathFindingQuery PathQuery;
                PathQuery.StartLocation = ItemLocation;
                PathQuery.EndLocation = CheckPoint;
                PathQuery.NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);

                FPathFindingResult PathResult = NavSys->FindPathSync(PathQuery);
                if (PathResult.IsSuccessful())
                {
                    ++ValidPathCount;
                }
            }

            // Normalize path options score
            PathOptionsScore = static_cast<float>(ValidPathCount) / NumPathChecks;

            // Combine scores using weights
            const float CombinedScore = (DistanceScore * DirectDistanceWeight) +
                                        (PathOptionsScore * PathOptionsWeight);

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

FText UEFEQSTest_EscapeRoute::GetDescriptionTitle() const
{
	return FText::FromString(FString::Printf(TEXT("%s: escape route"), *UEnvQueryTypes::GetShortTypeName(DistanceContext).ToString()));
}

FText UEFEQSTest_EscapeRoute::GetDescriptionDetails() const
{
	return FText::FromString(FString::Printf(TEXT("min distance: %.0f\nmax distance: %.0f\ndirect weight: %.1f\npath weight: %.1f"),
		MinDesiredDistance, MaxDistance, DirectDistanceWeight, PathOptionsWeight));
}