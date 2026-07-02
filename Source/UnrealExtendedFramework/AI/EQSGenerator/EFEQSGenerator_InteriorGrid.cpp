// Fill out your copyright notice in the Description page of Project Settings.

#include "EFEQSGenerator_InteriorGrid.h"
#include "NavigationSystem.h"
#include "Components/BoxComponent.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"

UEFEQSGenerator_InteriorGrid::UEFEQSGenerator_InteriorGrid(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ItemType = UEnvQueryItemType_Point::StaticClass();
	SpaceBetween.DefaultValue = 200.0f;
	ProjectionVerticalOffset.DefaultValue = 500.0f;
	GenerateIn = UEnvQueryContext_Querier::StaticClass();
}

void UEFEQSGenerator_InteriorGrid::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
	UObject* BindOwner = QueryInstance.Owner.Get();
	SpaceBetween.BindData(BindOwner, QueryInstance.QueryID);
	ProjectionVerticalOffset.BindData(BindOwner, QueryInstance.QueryID);

	const float GridSpacing = FMath::Max(SpaceBetween.GetValue(), 10.0f);
	const float VerticalOffset = ProjectionVerticalOffset.GetValue();

	TArray<AActor*> ContextActors;
	QueryInstance.PrepareContext(GenerateIn, ContextActors);

	UNavigationSystemV1* NavSys = nullptr;
	if (QueryInstance.World)
	{
		NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(QueryInstance.World);
	}

	TArray<FNavLocation> ProjectedPoints;

	for (AActor* ContextActor : ContextActors)
	{
		if (!ContextActor)
		{
			continue;
		}

		UBoxComponent* InteriorBox = ContextActor->FindComponentByClass<UBoxComponent>();
		if (!InteriorBox)
		{
			continue;
		}

		// Get the box transform in world space
		const FTransform BoxTransform = InteriorBox->GetComponentTransform();
		const FVector BoxExtent = InteriorBox->GetUnscaledBoxExtent();
		const FVector BoxScale = BoxTransform.GetScale3D();

		// Calculate local spacing to guarantee uniform World Space grid spacing
		const float LocalSpacingX = GridSpacing / FMath::Max(0.01f, FMath::Abs(BoxScale.X));
		const float LocalSpacingY = GridSpacing / FMath::Max(0.01f, FMath::Abs(BoxScale.Y));

		// Calculate steps
		const int32 StepsX = FMath::CeilToInt((BoxExtent.X * 2.0f) / LocalSpacingX);
		const int32 StepsY = FMath::CeilToInt((BoxExtent.Y * 2.0f) / LocalSpacingY);

		for (int32 X = 0; X <= StepsX; ++X)
		{
			for (int32 Y = 0; Y <= StepsY; ++Y)
			{
				// Calculate local position within the box (from -Extent to +Extent)
				const FVector LocalPoint(
					-BoxExtent.X + (X * LocalSpacingX),
					-BoxExtent.Y + (Y * LocalSpacingY),
					0.0f // Z is 0 in local space, NavMesh projection will handle height
				);

				// Transform to world space
				const FVector WorldPoint = BoxTransform.TransformPosition(LocalPoint);

				// Project onto NavMesh
				if (NavSys)
				{
					FNavLocation ProjectedPoint;
					const FVector ProjectionExtent(0.0f, 0.0f, VerticalOffset);

					if (NavSys->ProjectPointToNavigation(WorldPoint, ProjectedPoint, ProjectionExtent))
					{
						ProjectedPoints.Add(ProjectedPoint);
					}
				}
			}
		}
	}

	// Store generated items
	QueryInstance.ReserveItemData(ProjectedPoints.Num());
	for (const FNavLocation& Point : ProjectedPoints)
	{
		QueryInstance.AddItemData<UEnvQueryItemType_Point>(Point.Location);
	}
}

FText UEFEQSGenerator_InteriorGrid::GetDescriptionTitle() const
{
	return FText::FromString(TEXT("Points: Interior Grid"));
}

FText UEFEQSGenerator_InteriorGrid::GetDescriptionDetails() const
{
	return FText::FromString(FString::Printf(TEXT("Space Between: %.0f"), SpaceBetween.DefaultValue));
}
