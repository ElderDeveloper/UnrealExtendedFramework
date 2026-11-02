// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementPreviewRenderer.h"

#include "Assets/EFAssetLoading.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementSettings.h"
#include "UnrealExtendedGameplay/Systems/Placement/Preview/EGPlacementVisuals.h"

namespace EGPlacementPreviewPrivate
{
	const FName PreviewTag(TEXT("EGPlacementPreview"));
}

void UEGPlacementPreviewRenderer::GatherPreload(const FEGPlacementSelection& Selection, TArray<FSoftObjectPath>& OutPaths) const
{
	Selection.Visual.GetSoftPaths(OutPaths);
}

// ---------------------------------------------------------------------------------------------
// Mesh
// ---------------------------------------------------------------------------------------------

void UEGPlacementPreview_Mesh::GatherPreload(const FEGPlacementSelection& Selection, TArray<FSoftObjectPath>& OutPaths) const
{
	Super::GatherPreload(Selection, OutPaths);
	if (!Look.GhostMaterial.IsNull())
	{
		OutPaths.Add(Look.GhostMaterial.ToSoftObjectPath());
	}
	else if (!UEGPlacementSettings::Get()->DefaultGhostMaterial.IsNull())
	{
		OutPaths.Add(UEGPlacementSettings::Get()->DefaultGhostMaterial.ToSoftObjectPath());
	}
}

UMaterialInterface* UEGPlacementPreview_Mesh::ResolveGhostMaterial() const
{
	if (!Look.GhostMaterial.IsNull())
	{
		return EFAssets::Resolve(Look.GhostMaterial, TEXT("UEGPlacementPreview_Mesh"));
	}
	return EFAssets::Resolve(UEGPlacementSettings::Get()->DefaultGhostMaterial, TEXT("UEGPlacementPreview_Mesh"));
}

void UEGPlacementPreview_Mesh::Begin_Implementation(AActor* Owner, const FEGPlacementSelection& Selection)
{
	BeginVisual(Owner, Selection.Visual);
}

void UEGPlacementPreview_Mesh::BeginVisual(AActor* Owner, const FEGPlacementVisual& Visual)
{
	End_Implementation();
	if (!Owner)
	{
		return;
	}
	UMaterialInterface* Ghost = ResolveGhostMaterial();
	UMaterialInstanceDynamic* SharedGhost = Ghost ? UMaterialInstanceDynamic::Create(Ghost, this) : nullptr;
	if (SharedGhost)
	{
		Materials.Add(SharedGhost);
	}

	for (const FEGPlacementVisualMesh& Mesh : Visual.Meshes)
	{
		UMeshComponent* Component = nullptr;
		if (UStaticMesh* Static = EFAssets::Resolve(Mesh.StaticMesh, TEXT("UEGPlacementPreview_Mesh")))
		{
			UStaticMeshComponent* StaticComponent = NewObject<UStaticMeshComponent>(Owner, NAME_None, RF_Transient);
			StaticComponent->SetStaticMesh(Static);
			Component = StaticComponent;
		}
		else if (USkeletalMesh* Skeletal = EFAssets::Resolve(Mesh.SkeletalMesh, TEXT("UEGPlacementPreview_Mesh")))
		{
			USkeletalMeshComponent* SkeletalComponent = NewObject<USkeletalMeshComponent>(Owner, NAME_None, RF_Transient);
			SkeletalComponent->SetSkeletalMeshAsset(Skeletal);
			SkeletalComponent->PrimaryComponentTick.bStartWithTickEnabled = false;
			Component = SkeletalComponent;
		}
		if (!Component)
		{
			continue;
		}
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCanEverAffectNavigation(false);
		Component->SetGenerateOverlapEvents(false);
		Component->SetIsReplicated(false);
		Component->CastShadow = Look.bCastShadows;
		Component->bReceivesDecals = false;
		Component->PrimaryComponentTick.bCanEverTick = Component->IsA<USkeletalMeshComponent>();
		Component->SetUsingAbsoluteLocation(true);
		Component->SetUsingAbsoluteRotation(true);
		Component->SetUsingAbsoluteScale(true);
		Component->ComponentTags.Add(EGPlacementPreviewPrivate::PreviewTag);
		Component->SetVisibility(false);
		Component->RegisterComponent();

		for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
		{
			if (SharedGhost)
			{
				Component->SetMaterial(Slot, SharedGhost);
				continue;
			}
			UMaterialInterface* Base = Mesh.Materials.IsValidIndex(Slot) ? EFAssets::Resolve(Mesh.Materials[Slot], TEXT("UEGPlacementPreview_Mesh")) : nullptr;
			if (!Base)
			{
				Base = Component->GetMaterial(Slot);
			}
			if (Base)
			{
				UMaterialInstanceDynamic* Instance = UMaterialInstanceDynamic::Create(Base, this);
				Component->SetMaterial(Slot, Instance);
				Materials.Add(Instance);
			}
		}
		Components.Add(Component);
		RelativeTransforms.Add(Mesh.RelativeTransform);
	}
	AppliedStatus.Reset();
	bVisible = false;
}

void UEGPlacementPreview_Mesh::Update_Implementation(const FEGPlacementEvaluation& Evaluation, bool bHasPose)
{
	if (!bHasPose)
	{
		if (bVisible)
		{
			for (UMeshComponent* Component : Components)
			{
				if (Component)
				{
					Component->SetVisibility(false);
				}
			}
			bVisible = false;
		}
		return;
	}
	for (int32 Index = 0; Index < Components.Num(); ++Index)
	{
		if (UMeshComponent* Component = Components[Index])
		{
			Component->SetWorldTransform(RelativeTransforms[Index] * Evaluation.Pose);
			if (!bVisible)
			{
				Component->SetVisibility(true);
			}
		}
	}
	bVisible = true;
	ApplyStatus(Evaluation.Status);
}

void UEGPlacementPreview_Mesh::ApplyStatus(EEGPlacementStatus Status)
{
	if (AppliedStatus.IsSet() && AppliedStatus.GetValue() == Status)
	{
		return;
	}
	AppliedStatus = Status;
	const FLinearColor Color = Status == EEGPlacementStatus::Valid ? Look.ValidColor : (Status == EEGPlacementStatus::Pending ? Look.PendingColor : Look.InvalidColor);
	for (UMaterialInstanceDynamic* Material : Materials)
	{
		if (Material)
		{
			Material->SetVectorParameterValue(Look.ColorParameter, Color);
		}
	}
}

void UEGPlacementPreview_Mesh::End_Implementation()
{
	for (UMeshComponent* Component : Components)
	{
		if (Component)
		{
			Component->DestroyComponent();
		}
	}
	Components.Reset();
	Materials.Reset();
	RelativeTransforms.Reset();
	AppliedStatus.Reset();
	bVisible = false;
}

// ---------------------------------------------------------------------------------------------
// Actor class
// ---------------------------------------------------------------------------------------------

void UEGPlacementPreview_ActorClass::GatherPreload(const FEGPlacementSelection& Selection, TArray<FSoftObjectPath>& OutPaths) const
{
	Super::GatherPreload(Selection, OutPaths);
	if (Selection.Visual.IsEmpty() && !Selection.ActorClass.IsNull())
	{
		OutPaths.Add(Selection.ActorClass.ToSoftObjectPath());
	}
}

void UEGPlacementPreview_ActorClass::Begin_Implementation(AActor* Owner, const FEGPlacementSelection& Selection)
{
	if (!Selection.Visual.IsEmpty())
	{
		BeginVisual(Owner, Selection.Visual);
		return;
	}
	UClass* Class = EFAssets::Resolve(Selection.ActorClass, TEXT("UEGPlacementPreview_ActorClass"));
	BeginVisual(Owner, EGPlacementVisuals::BuildFromClass(Class));
}
