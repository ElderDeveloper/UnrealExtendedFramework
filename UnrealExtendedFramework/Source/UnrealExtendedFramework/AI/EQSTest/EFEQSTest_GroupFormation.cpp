// Copyright Epic Games, Inc. All Rights Reserved.

#include "EFEQSTest_GroupFormation.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "CollisionQueryParams.h"

UEFEQSTest_GroupFormation::UEFEQSTest_GroupFormation(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Cost = EEnvTestCost::High;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	SetWorkOnFloatValues(true);

	GroupMembersContext = UEnvQueryContext_Querier::StaticClass();
	MinGroupDistance = 300.0f;
	MaxGroupDistance = 1000.0f;
	DistanceWeight = 0.4f;
	FormationWeight = 0.4f;
	LineOfSightWeight = 0.2f;
	bCheckLineOfSight = true;
	SightCheckHeight = 180.0f;
}

void UEFEQSTest_GroupFormation::RunTest(FEnvQueryInstance& QueryInstance) const
{
    UObject* QueryOwner = QueryInstance.Owner.Get();
    if (QueryOwner == nullptr)
    {
        return;
    }

    TArray<FVector> GroupLocations;
    QueryInstance.PrepareContext(GroupMembersContext, GroupLocations);

    if (GroupLocations.IsEmpty())
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
        float DistanceScore = 0.0f;
        float FormationScore = 0.0f;
        float LineOfSightScore = 0.0f;
        bool bIsValid = false;

        // Calculate average distance to group members
        float TotalDistance = 0.0f;
        int32 ValidDistances = 0;

        for (const FVector& GroupLocation : GroupLocations)
        {
            const float Distance = FVector::Distance(ItemLocation, GroupLocation);
            if (Distance > 0.0f && Distance <= MaxGroupDistance)
            {
                TotalDistance += Distance;
                ++ValidDistances;
            }
        }

        if (ValidDistances > 0)
        {
            const float AvgDistance = TotalDistance / ValidDistances;

            // Score is best when distance is close to MinGroupDistance
            DistanceScore = 1.0f - FMath::Min(1.0f, FMath::Abs(AvgDistance - MinGroupDistance) / (MaxGroupDistance - MinGroupDistance));
            bIsValid = true;
        }

        // Calculate formation score based on relative positions
        if (GroupLocations.Num() >= 2)
        {
            // Calculate center point of existing group members
            FVector GroupCenter = FVector::ZeroVector;
            for (const FVector& GroupLocation : GroupLocations)
            {
                GroupCenter += GroupLocation;
            }
            GroupCenter /= GroupLocations.Num();

            // Score based on how well the position maintains a balanced formation
            const FVector ToItem = (ItemLocation - GroupCenter).GetSafeNormal();
            float AngleSum = 0.0f;

            for (const FVector& GroupLocation : GroupLocations)
            {
                const FVector ToMember = (GroupLocation - GroupCenter).GetSafeNormal();
                AngleSum += FMath::Abs(FVector::DotProduct(ToItem, ToMember));
            }

            // Lower angle sum means better spacing in the formation
            FormationScore = 1.0f - (AngleSum / GroupLocations.Num());
        }

        // Calculate line of sight score
        if (bCheckLineOfSight)
        {
            int32 VisibleMembers = 0;
            FCollisionQueryParams QueryParams;
            QueryParams.AddIgnoredActor(Cast<AActor>(QueryOwner));

            for (const FVector& GroupLocation : GroupLocations)
            {
                const FVector StartTrace = FVector(ItemLocation.X, ItemLocation.Y, ItemLocation.Z + SightCheckHeight);
                const FVector EndTrace = FVector(GroupLocation.X, GroupLocation.Y, GroupLocation.Z + SightCheckHeight);

                bool bHasLineOfSight = !World->LineTraceTestByChannel(
                    StartTrace,
                    EndTrace,
                    ECC_Visibility,
                    QueryParams
                );

                if (bHasLineOfSight)
                {
                    ++VisibleMembers;
                }
            }

            LineOfSightScore = static_cast<float>(VisibleMembers) / GroupLocations.Num();
        }
        else
        {
            LineOfSightScore = 1.0f;
        }

        // Combine scores using weights
        const float FinalScore = (DistanceScore * DistanceWeight) +
                                 (FormationScore * FormationWeight) +
                                 (LineOfSightScore * LineOfSightWeight);

        It.SetScore(TestPurpose, FilterType, bIsValid, !bIsValid);
    }
}

FText UEFEQSTest_GroupFormation::GetDescriptionTitle() const
{
	return FText::FromString(FString::Printf(TEXT("%s: group formation"), *UEnvQueryTypes::GetShortTypeName(GroupMembersContext).ToString()));
}

FText UEFEQSTest_GroupFormation::GetDescriptionDetails() const
{
	return FText::FromString(FString::Printf(TEXT("min distance: %.0f\nmax distance: %.0f\ndist weight: %.1f\nformation weight: %.1f\nLoS weight: %.1f"),
		MinGroupDistance, MaxGroupDistance, DistanceWeight, FormationWeight, LineOfSightWeight));
}