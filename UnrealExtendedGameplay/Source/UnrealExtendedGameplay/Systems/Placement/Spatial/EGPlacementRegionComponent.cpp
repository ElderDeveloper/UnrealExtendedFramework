// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementRegionComponent.h"

#include "Engine/World.h"
#include "UnrealExtendedGameplay/Systems/Placement/Spatial/EGPlacementSpatialSubsystem.h"

UEGPlacementRegionComponent::UEGPlacementRegionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsOnUpdateTransform = true;
	SetIsReplicatedByDefault(false);
}

void UEGPlacementRegionComponent::SetBoxExtent(FVector InExtent)
{
	BoxExtent = InExtent.ComponentMax(FVector::ZeroVector);
	Refresh();
}

void UEGPlacementRegionComponent::SetMode(EEGPlacementRegionMode InMode)
{
	Mode = InMode;
	Refresh();
}

void UEGPlacementRegionComponent::SetRegionTags(const FGameplayTagContainer& InTags)
{
	RegionTags = InTags;
	Refresh();
}

FEGPlacementOrientedBox UEGPlacementRegionComponent::GetWorldBox() const
{
	const FTransform& Transform = GetComponentTransform();
	return FEGPlacementOrientedBox(Transform.GetLocation(), Transform.GetRotation(), BoxExtent * Transform.GetScale3D().GetAbs());
}

void UEGPlacementRegionComponent::OnRegister()
{
	Super::OnRegister();
	bRegistered = true;
	Refresh();
}

void UEGPlacementRegionComponent::OnUnregister()
{
	bRegistered = false;
	if (UEGPlacementSpatialSubsystem* Spatial = GetWorld() ? GetWorld()->GetSubsystem<UEGPlacementSpatialSubsystem>() : nullptr)
	{
		Spatial->UnregisterRegion(this);
	}
	Super::OnUnregister();
}

void UEGPlacementRegionComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);
	Refresh();
}

#if WITH_EDITOR
void UEGPlacementRegionComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	Refresh();
}
#endif

void UEGPlacementRegionComponent::Refresh()
{
	uint32 Hash = EGPlacementSpatial::HashTransform(GetComponentTransform());
	Hash = HashCombine(Hash, GetTypeHash(FIntVector(FMath::RoundToInt32(BoxExtent.X), FMath::RoundToInt32(BoxExtent.Y), FMath::RoundToInt32(BoxExtent.Z))));
	Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(Mode)));
	for (const FGameplayTag& Tag : RegionTags)
	{
		Hash = HashCombine(Hash, GetTypeHash(Tag.GetTagName().ToString()));
	}
	Revision = EGPlacementSpatial::ToRevision(Hash);

	if (bRegistered)
	{
		if (UEGPlacementSpatialSubsystem* Spatial = GetWorld() ? GetWorld()->GetSubsystem<UEGPlacementSpatialSubsystem>() : nullptr)
		{
			Spatial->RegisterRegion(this);
		}
	}
}
