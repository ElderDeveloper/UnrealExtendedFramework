// Fill out your copyright notice in the Description page of Project Settings.


#include "EFEQSGeneratorCube.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"

UEFEQSGeneratorCube::UEFEQSGeneratorCube()
{
	Center = UEnvQueryContext_Querier::StaticClass();
	CubeHalfExtents = FVector(500.f);
	SpaceBetween = 100.f;
}

void UEFEQSGeneratorCube::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
	UObject* QueryOwner = QueryInstance.Owner.Get();
	if (QueryOwner == nullptr)
	{
		return;
	}

	if (SpaceBetween <= 0.f)
	{
		return;
	}

	TArray<FVector> ContextLocations;
	if (!QueryInstance.PrepareContext(Center, ContextLocations))
	{
		return;
	}
	
	const int32 PointCountX = FMath::FloorToInt(CubeHalfExtents.X * 2 / SpaceBetween);
	const int32 PointCountY = FMath::FloorToInt(CubeHalfExtents.Y * 2 / SpaceBetween);
	const int32 PointCountZ = FMath::FloorToInt(CubeHalfExtents.Z * 2 / SpaceBetween);
	
	const float StartOffsetX = -PointCountX * 0.5f * SpaceBetween;
	const float StartOffsetY = -PointCountY * 0.5f * SpaceBetween;
	const float StartOffsetZ = -PointCountZ * 0.5f * SpaceBetween;
	
	TArray<FNavLocation> GeneratedPoints;
	GeneratedPoints.Reserve((PointCountX + 1) * (PointCountY + 1) * (PointCountZ + 1) * ContextLocations.Num());

	for (const FVector& ContextLocation : ContextLocations)
	{
		for (int32 IndexZ = 0; IndexZ <= PointCountZ; ++IndexZ)
		{
			const float OffsetZ = StartOffsetZ + IndexZ * SpaceBetween;
			for (int32 IndexY = 0; IndexY <= PointCountY; ++IndexY)
			{
				const float OffsetY = StartOffsetY + IndexY * SpaceBetween;
				for (int32 IndexX = 0; IndexX <= PointCountX; ++IndexX)
				{
					const float OffsetX = StartOffsetX + IndexX * SpaceBetween;
					const FVector GeneratedPoint = ContextLocation + FVector(OffsetX, OffsetY, OffsetZ);
					GeneratedPoints.Add(FNavLocation(GeneratedPoint));
				}
			}
		}
	}

	ProjectAndFilterNavPoints(GeneratedPoints, QueryInstance);
	StoreNavPoints(GeneratedPoints, QueryInstance);
}

FText UEFEQSGeneratorCube::GetDescriptionTitle() const
{
	return FText::FromString("Extended Framework PathingCube: generate around Querier");
}

FText UEFEQSGeneratorCube::GetDescriptionDetails() const
{
	// Add details of variables
	return FText::FromString("Cube Half Extend: " + FString::SanitizeFloat(CubeHalfExtents.X) + ", " + FString::SanitizeFloat(CubeHalfExtents.Y) + ", " + FString::SanitizeFloat(CubeHalfExtents.Z) + "\n" + "Space Between: " + FString::SanitizeFloat(SpaceBetween));
}
