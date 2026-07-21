// Copyright Epic Games, Inc. All Rights Reserved.

#include "EFEQSTest_PathLength.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "NavigationSystem.h"
#include "AI/Navigation/NavigationTypes.h"

UEFEQSTest_PathLength::UEFEQSTest_PathLength(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Cost = EEnvTestCost::High;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(true);

	PathTo = UEnvQueryContext_Querier::StaticClass();
	bIncludeHeight = true;
	bAllowPartialPath = true;
}

void UEFEQSTest_PathLength::RunTest(FEnvQueryInstance& QueryInstance) const
{
    UObject* QueryOwner = QueryInstance.Owner.Get();
    if (QueryOwner == nullptr)
    {
        return;
    }

    TArray<FVector> ContextLocations;
    QueryInstance.PrepareContext(PathTo, ContextLocations);

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
        float MinPathLength = FLT_MAX;
        bool bIsValid = false;

        for (const FVector& ContextLocation : ContextLocations)
        {
            FPathFindingQuery PathQuery;
            PathQuery.StartLocation = ItemLocation;
            PathQuery.EndLocation = ContextLocation;
            PathQuery.NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
            PathQuery.SetAllowPartialPaths(bAllowPartialPath);

            FPathFindingResult PathResult = NavSys->FindPathSync(PathQuery);
            if (PathResult.IsSuccessful() || (bAllowPartialPath && PathResult.IsPartial()))
            {
                float PathLength = 0.0f;
                const TArray<FNavPathPoint>& PathPoints = PathResult.Path->GetPathPoints();
                for (int32 i = 1; i < PathPoints.Num(); ++i)
                {
                    PathLength += FVector::Dist(PathPoints[i - 1].Location, PathPoints[i].Location);
                }

                if (!bIncludeHeight)
                {
                    PathLength = FVector2D(PathPoints[0].Location - PathPoints.Last().Location).Size();
                }

                MinPathLength = FMath::Min(MinPathLength, PathLength);
                bIsValid = true;
            }
        }

        It.SetScore(TestPurpose, FilterType, bIsValid, !bIsValid);
    }
}

FText UEFEQSTest_PathLength::GetDescriptionTitle() const
{
	return FText::FromString(FString::Printf(TEXT("%s: path length"), *UEnvQueryTypes::GetShortTypeName(PathTo).ToString()));
}

FText UEFEQSTest_PathLength::GetDescriptionDetails() const
{
	return FText::FromString(FString::Printf(TEXT("include height: %s\nallow partial path: %s"),
		bIncludeHeight ? TEXT("yes") : TEXT("no"),
		bAllowPartialPath ? TEXT("yes") : TEXT("no")));
}
