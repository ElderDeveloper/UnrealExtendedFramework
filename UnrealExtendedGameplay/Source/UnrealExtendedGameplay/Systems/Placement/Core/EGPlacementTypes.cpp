// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementTypes.h"

#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"

FVector FEGPlacementOrientedBox::GetAxis(int32 Index) const
{
	switch (Index)
	{
	case 0: return Rotation.GetAxisX();
	case 1: return Rotation.GetAxisY();
	default: return Rotation.GetAxisZ();
	}
}

void FEGPlacementOrientedBox::GetCorners(FVector OutCorners[8]) const
{
	const FVector X = GetAxis(0) * Extent.X;
	const FVector Y = GetAxis(1) * Extent.Y;
	const FVector Z = GetAxis(2) * Extent.Z;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		OutCorners[Index] = Center
			+ ((Index & 1) ? X : -X)
			+ ((Index & 2) ? Y : -Y)
			+ ((Index & 4) ? Z : -Z);
	}
}

FBox FEGPlacementOrientedBox::GetBounds() const
{
	// Projected half size of the box on each world axis.
	const FVector X = GetAxis(0) * Extent.X;
	const FVector Y = GetAxis(1) * Extent.Y;
	const FVector Z = GetAxis(2) * Extent.Z;
	const FVector Half(
		FMath::Abs(X.X) + FMath::Abs(Y.X) + FMath::Abs(Z.X),
		FMath::Abs(X.Y) + FMath::Abs(Y.Y) + FMath::Abs(Z.Y),
		FMath::Abs(X.Z) + FMath::Abs(Y.Z) + FMath::Abs(Z.Z));
	return FBox(Center - Half, Center + Half);
}

FBox FEGPlacementShape::GetLocalBounds() const
{
	FBox Bounds(ForceInit);
	auto AddBoxes = [&Bounds](const TArray<FEGPlacementBox>& Boxes)
	{
		for (const FEGPlacementBox& Box : Boxes)
		{
			Bounds += FEGPlacementOrientedBox(Box.Center, Box.Rotation.Quaternion(), Box.Extent).GetBounds();
		}
	};
	AddBoxes(Footprint);
	AddBoxes(Clearance);
	return Bounds;
}

FVector FEGPlacementContext::GetViewLocation() const
{
	if (bHasViewLocation)
	{
		return ViewLocation;
	}
	if (Requester)
	{
		FVector Location;
		FRotator Rotation;
		Requester->GetActorEyesViewPoint(Location, Rotation);
		return Location;
	}
	return FVector::ZeroVector;
}

bool FEGPlacementEvaluation::HasReason(const FGameplayTag& Tag) const
{
	for (const FEGPlacementReason& Reason : Reasons)
	{
		if (Reason.Tag.MatchesTag(Tag))
		{
			return true;
		}
	}
	return false;
}

FString FEGPlacementEvaluation::DescribeReasons() const
{
	TArray<FString> Parts;
	for (const FEGPlacementReason& Reason : Reasons)
	{
		Parts.Add(Reason.Text.IsEmpty() ? Reason.Tag.ToString() : FString::Printf(TEXT("%s (%s)"), *Reason.Tag.ToString(), *Reason.Text.ToString()));
	}
	return FString::Join(Parts, TEXT(", "));
}

void FEGPlacementVisual::GetSoftPaths(TArray<FSoftObjectPath>& OutPaths) const
{
	for (const FEGPlacementVisualMesh& Mesh : Meshes)
	{
		if (!Mesh.StaticMesh.IsNull())
		{
			OutPaths.Add(Mesh.StaticMesh.ToSoftObjectPath());
		}
		if (!Mesh.SkeletalMesh.IsNull())
		{
			OutPaths.Add(Mesh.SkeletalMesh.ToSoftObjectPath());
		}
		for (const TSoftObjectPtr<UMaterialInterface>& Material : Mesh.Materials)
		{
			if (!Material.IsNull())
			{
				OutPaths.Add(Material.ToSoftObjectPath());
			}
		}
	}
}
