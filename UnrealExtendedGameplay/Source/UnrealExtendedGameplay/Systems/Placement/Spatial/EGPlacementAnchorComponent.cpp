// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementAnchorComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UnrealExtendedGameplay/Systems/Placement/Spatial/EGPlacementSpatialSubsystem.h"

UEGPlacementAnchorComponent::UEGPlacementAnchorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsOnUpdateTransform = true;
	SetIsReplicatedByDefault(false);
}

bool UEGPlacementAnchorComponent::Accepts(const FEGPlacementSpec& Spec) const
{
	return bAnchorEnabled && (AcceptedSpecs.IsEmpty() || AcceptedSpecs.Matches(Spec.Tags));
}

FTransform UEGPlacementAnchorComponent::GetAnchorTransform() const
{
	FTransform Transform = GetComponentTransform();
	Transform.SetScale3D(FVector::OneVector);
	return Transform;
}

UPrimitiveComponent* UEGPlacementAnchorComponent::GetSupportPrimitive() const
{
	for (USceneComponent* Parent = GetAttachParent(); Parent; Parent = Parent->GetAttachParent())
	{
		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Parent))
		{
			return Primitive;
		}
	}
	const AActor* Owner = GetOwner();
	return Owner ? Cast<UPrimitiveComponent>(Owner->GetRootComponent()) : nullptr;
}

void UEGPlacementAnchorComponent::SetAnchorEnabled(bool bEnabled)
{
	bAnchorEnabled = bEnabled;
	Refresh();
}

void UEGPlacementAnchorComponent::SetExternalRevision(int32 InExternalRevision)
{
	ExternalRevision = InExternalRevision;
	Refresh();
}

void UEGPlacementAnchorComponent::OnRegister()
{
	Super::OnRegister();
	bRegistered = true;
	Refresh();
}

void UEGPlacementAnchorComponent::OnUnregister()
{
	bRegistered = false;
	if (UEGPlacementSpatialSubsystem* Spatial = GetWorld() ? GetWorld()->GetSubsystem<UEGPlacementSpatialSubsystem>() : nullptr)
	{
		Spatial->UnregisterAnchor(this);
	}
	Super::OnUnregister();
}

void UEGPlacementAnchorComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);
	Refresh();
}

#if WITH_EDITOR
void UEGPlacementAnchorComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	Refresh();
}
#endif

void UEGPlacementAnchorComponent::Refresh()
{
	uint32 Hash = EGPlacementSpatial::HashTransform(GetAnchorTransform());
	Hash = HashCombine(Hash, GetTypeHash(bAnchorEnabled));
	Hash = HashCombine(Hash, GetTypeHash(ExternalRevision));
	Hash = HashCombine(Hash, GetTypeHash(FMath::RoundToInt32(SnapRadius)));
	for (const FGameplayTag& Tag : AnchorTags)
	{
		Hash = HashCombine(Hash, GetTypeHash(Tag.GetTagName().ToString()));
	}
	Revision = EGPlacementSpatial::ToRevision(Hash);

	if (bRegistered)
	{
		if (UEGPlacementSpatialSubsystem* Spatial = GetWorld() ? GetWorld()->GetSubsystem<UEGPlacementSpatialSubsystem>() : nullptr)
		{
			Spatial->RegisterAnchor(this);
		}
	}
}
