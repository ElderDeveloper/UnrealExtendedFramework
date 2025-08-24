// EFEQSTest_PathCost.cpp
#include "EFEQSTest_NavMeshCost.h"

#include "EngineUtils.h"
#include "NavModifierVolume.h"
#include "Components/BrushComponent.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"


UEFEQSTest_NavMeshCost::UEFEQSTest_NavMeshCost()
{
    Cost = EEnvTestCost::Medium;
    ValidItemType = UEnvQueryItemType_Point::StaticClass();
    SetWorkOnFloatValues(true);
    ScoringEquation = EEnvTestScoreEquation::Constant;
    FilterType = EEnvTestFilterType::Range;
}

void UEFEQSTest_NavMeshCost::RunTest(FEnvQueryInstance& QueryInstance) const
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

    // Get querier location
    TArray<FVector> ContextLocations;
    if (!QueryInstance.PrepareContext(UEnvQueryContext_Querier::StaticClass(), ContextLocations))
    {
        return;
    }

    if (ContextLocations.Num() == 0)
    {
        return;
    }

    const FVector QuerierLocation = ContextLocations[0];

    // Find all nav modifiers within search distance
    TArray<ANavModifierVolume*> NearbyModifiers;
    for (TActorIterator<ANavModifierVolume> ActorIterator(World); ActorIterator; ++ActorIterator)
    {
        ANavModifierVolume* NavModifier = *ActorIterator;
        if (NavModifier && NavModifier->IsValidLowLevel())
        {
            float Distance = FVector::Dist(QuerierLocation, NavModifier->GetActorLocation());
            if (Distance <= SearchDistance)
            {
                NearbyModifiers.Add(NavModifier);
            }
        }
    }

    for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
    {
        const FVector ItemLocation = GetItemLocation(QueryInstance, It.GetIndex());
        float LocationCost = 1.0f; // Default nav mesh cost

        // Check if point is within any nav modifier bounds
        for (ANavModifierVolume* NavModifier : NearbyModifiers)
        {
            if (IsPointInNavModifierBounds(ItemLocation, NavModifier))
            {
                // Get the nav area class and its cost modifier
                if (IsValid(NavModifier->GetAreaClass()))
                {
                   LocationCost = NavModifier->GetAreaClass().GetDefaultObject()->DefaultCost;
                }
            }
        }

        It.SetScore(TestPurpose, FilterType, LocationCost, FloatValueMin.GetValue(),  FloatValueMax.GetValue());
    }
}

bool UEFEQSTest_NavMeshCost::IsPointInNavModifierBounds(const FVector& Point, ANavModifierVolume* NavModifier) const
{
    if (!NavModifier || !NavModifier->GetBrushComponent())
    {
        return false;
    }

    // Use brush component bounds scaled by actor scale
    FVector ModifierLocation = NavModifier->GetActorLocation();
    FVector ModifierScale = NavModifier->GetActorScale3D();
    
    // Get brush bounds (assuming box shape for simplicity)
    FBoxSphereBounds Bounds = NavModifier->GetBrushComponent()->CalcBounds(FTransform::Identity);
    FVector ScaledExtent = Bounds.BoxExtent * ModifierScale;

    // Check if point is within scaled bounds
    FVector RelativeLocation = Point - ModifierLocation;
    return (FMath::Abs(RelativeLocation.X) <= ScaledExtent.X &&
            FMath::Abs(RelativeLocation.Y) <= ScaledExtent.Y &&
            FMath::Abs(RelativeLocation.Z) <= ScaledExtent.Z);
}

FText UEFEQSTest_NavMeshCost::GetDescriptionTitle() const
{
	return FText::FromString(TEXT("Nav Mesh Cost"));
}

FText UEFEQSTest_NavMeshCost::GetDescriptionDetails() const
{
	FString MinValue = FString::SanitizeFloat(FloatValueMin.DefaultValue);
	FString MaxValue = FString::SanitizeFloat(FloatValueMax.DefaultValue);

	return FText::FromString(FString::Printf(TEXT("Tests nav mesh cost,\n filtered between %s and %s"), *MinValue, *MaxValue));
}