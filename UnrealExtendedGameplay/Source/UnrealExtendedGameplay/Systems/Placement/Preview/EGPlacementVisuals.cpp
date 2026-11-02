// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementVisuals.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/BodySetup.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementTags.h"

namespace EGPlacementVisualsPrivate
{
	TMap<TWeakObjectPtr<UClass>, FEGPlacementVisual>& ClassCache()
	{
		static TMap<TWeakObjectPtr<UClass>, FEGPlacementVisual> Cache;
		return Cache;
	}

	/** Transform of a component relative to its root, from relative transforms only (works on templates). */
	FTransform RelativeToRoot(const USceneComponent* Component)
	{
		FTransform Result = FTransform::Identity;
		for (const USceneComponent* Current = Component; Current && Current->GetAttachParent(); Current = Current->GetAttachParent())
		{
			Result = Result * Current->GetRelativeTransform();
		}
		return Result;
	}

	void AddMesh(const UActorComponent* Component, const FTransform& Relative, bool bLiveMaterials, FEGPlacementVisual& Out)
	{
		const UMeshComponent* MeshComponent = Cast<UMeshComponent>(Component);
		if (!MeshComponent || MeshComponent->bHiddenInGame)
		{
			return;
		}
		FEGPlacementVisualMesh Mesh;
		Mesh.RelativeTransform = Relative;
		if (const UStaticMeshComponent* Static = Cast<UStaticMeshComponent>(MeshComponent))
		{
			if (!Static->GetStaticMesh())
			{
				return;
			}
			Mesh.StaticMesh = Static->GetStaticMesh();
		}
		else if (const USkeletalMeshComponent* Skeletal = Cast<USkeletalMeshComponent>(MeshComponent))
		{
			if (!Skeletal->GetSkeletalMeshAsset())
			{
				return;
			}
			Mesh.SkeletalMesh = Skeletal->GetSkeletalMeshAsset();
		}
		else
		{
			return;
		}
		if (bLiveMaterials)
		{
			for (int32 Slot = 0; Slot < MeshComponent->GetNumMaterials(); ++Slot)
			{
				Mesh.Materials.Add(MeshComponent->GetMaterial(Slot));
			}
		}
		else
		{
			for (UMaterialInterface* Material : MeshComponent->OverrideMaterials)
			{
				Mesh.Materials.Add(Material);
			}
		}
		Out.Meshes.Add(MoveTemp(Mesh));
	}

	USCS_Node* FindNodeByVariable(const TArray<USimpleConstructionScript*>& Scripts, FName Name)
	{
		for (const USimpleConstructionScript* Script : Scripts)
		{
			for (USCS_Node* Node : Script->GetAllNodes())
			{
				if (Node && Node->GetVariableName() == Name)
				{
					return Node;
				}
			}
		}
		return nullptr;
	}

	FTransform NodeToRoot(USCS_Node* Node, UBlueprintGeneratedClass* ActualClass, const TArray<USimpleConstructionScript*>& Scripts, const AActor* Defaults, int32 Depth)
	{
		const USceneComponent* Template = Cast<USceneComponent>(Node->GetActualComponentTemplate(ActualClass));
		const FTransform Own = Template ? Template->GetRelativeTransform() : FTransform::Identity;
		if (Depth > 32)
		{
			return Own;
		}
		for (const USimpleConstructionScript* Script : Scripts)
		{
			if (USCS_Node* Parent = Script->FindParentNode(Node))
			{
				return Own * NodeToRoot(Parent, ActualClass, Scripts, Defaults, Depth + 1);
			}
		}
		if (!Node->ParentComponentOrVariableName.IsNone())
		{
			if (Node->bIsParentComponentNative)
			{
				TInlineComponentArray<USceneComponent*> Natives(Defaults);
				for (const USceneComponent* Native : Natives)
				{
					if (Native->GetFName() == Node->ParentComponentOrVariableName)
					{
						return Own * RelativeToRoot(Native);
					}
				}
			}
			else if (USCS_Node* Parent = FindNodeByVariable(Scripts, Node->ParentComponentOrVariableName))
			{
				return Own * NodeToRoot(Parent, ActualClass, Scripts, Defaults, Depth + 1);
			}
		}
		// No parent: attached to the native root, or the actor's root itself.
		return Defaults->GetRootComponent() ? Own : FTransform::Identity;
	}
}

namespace EGPlacementVisuals
{
	const FEGPlacementVisual& BuildFromClass(UClass* ActorClass)
	{
		using namespace EGPlacementVisualsPrivate;

		static const FEGPlacementVisual Empty;
		if (!ActorClass || !ActorClass->IsChildOf(AActor::StaticClass()))
		{
			return Empty;
		}
		if (const FEGPlacementVisual* Cached = ClassCache().Find(ActorClass))
		{
			return *Cached;
		}

		FEGPlacementVisual Visual;
		const AActor* Defaults = ActorClass->GetDefaultObject<AActor>();
		TInlineComponentArray<UActorComponent*> Natives(Defaults);
		for (const UActorComponent* Component : Natives)
		{
			if (const USceneComponent* Scene = Cast<USceneComponent>(Component))
			{
				AddMesh(Scene, RelativeToRoot(Scene), false, Visual);
			}
		}

		UBlueprintGeneratedClass* ActualClass = Cast<UBlueprintGeneratedClass>(ActorClass);
		TArray<USimpleConstructionScript*> Scripts;
		for (UClass* Class = ActorClass; Class; Class = Class->GetSuperClass())
		{
			if (const UBlueprintGeneratedClass* Generated = Cast<UBlueprintGeneratedClass>(Class))
			{
				if (Generated->SimpleConstructionScript)
				{
					Scripts.Add(Generated->SimpleConstructionScript);
				}
			}
		}
		for (const USimpleConstructionScript* Script : Scripts)
		{
			for (USCS_Node* Node : Script->GetAllNodes())
			{
				if (Node && ActualClass)
				{
					AddMesh(Node->GetActualComponentTemplate(ActualClass), NodeToRoot(Node, ActualClass, Scripts, Defaults, 0), false, Visual);
				}
			}
		}
		if (Visual.IsEmpty())
		{
			UE_LOG(LogEGPlacement, Verbose, TEXT("Placement preview: %s has no visible mesh components."), *ActorClass->GetName());
		}
		return ClassCache().Add(ActorClass, MoveTemp(Visual));
	}

	FEGPlacementVisual BuildFromActor(const AActor* Actor)
	{
		FEGPlacementVisual Visual;
		if (!Actor)
		{
			return Visual;
		}
		const FTransform ActorTransform = Actor->GetActorTransform();
		TInlineComponentArray<UMeshComponent*> Meshes(Actor);
		for (const UMeshComponent* Mesh : Meshes)
		{
			if (Mesh->IsRegistered() && Mesh->IsVisible())
			{
				EGPlacementVisualsPrivate::AddMesh(Mesh, Mesh->GetComponentTransform().GetRelativeTransform(ActorTransform), true, Visual);
			}
		}
		return Visual;
	}

	bool FitBoxes(const UStaticMesh* Mesh, const FTransform& Relative, EEGPlacementFitSource Source, float Padding, TArray<FEGPlacementBox>& OutBoxes)
	{
		if (!Mesh)
		{
			return false;
		}
		const FVector Scale = Relative.GetScale3D().GetAbs();
		auto AddBox = [&](const FVector& Center, const FRotator& Rotation, const FVector& Extent)
		{
			const FQuat World = Relative.GetRotation() * Rotation.Quaternion();
			OutBoxes.Emplace(Relative.TransformPosition(Center), Extent * Scale + FVector(Padding), World.Rotator());
		};

		const int32 Before = OutBoxes.Num();
		const UBodySetup* Body = Source == EEGPlacementFitSource::SimpleCollision ? Mesh->GetBodySetup() : nullptr;
		if (Body)
		{
			const FKAggregateGeom& Geometry = Body->AggGeom;
			for (const FKBoxElem& Box : Geometry.BoxElems)
			{
				AddBox(Box.Center, Box.Rotation, FVector(Box.X, Box.Y, Box.Z) * 0.5);
			}
			for (const FKSphereElem& Sphere : Geometry.SphereElems)
			{
				AddBox(Sphere.Center, FRotator::ZeroRotator, FVector(Sphere.Radius));
			}
			for (const FKSphylElem& Capsule : Geometry.SphylElems)
			{
				AddBox(Capsule.Center, Capsule.Rotation, FVector(Capsule.Radius, Capsule.Radius, Capsule.Length * 0.5 + Capsule.Radius));
			}
			for (const FKConvexElem& Convex : Geometry.ConvexElems)
			{
				if (Convex.ElemBox.IsValid)
				{
					AddBox(Convex.ElemBox.GetCenter(), FRotator::ZeroRotator, Convex.ElemBox.GetExtent());
				}
			}
		}
		if (OutBoxes.Num() == Before)
		{
			const FBox Bounds = Mesh->GetBoundingBox();
			AddBox(Bounds.GetCenter(), FRotator::ZeroRotator, Bounds.GetExtent());
		}
		return true;
	}

	bool FitSpec(FEGPlacementSpec& Spec, const FEGPlacementVisual& Visual, EEGPlacementFitSource Source, float Padding)
	{
		TArray<FEGPlacementBox> Boxes;
		for (const FEGPlacementVisualMesh& Mesh : Visual.Meshes)
		{
			if (const UStaticMesh* Static = Mesh.StaticMesh.LoadSynchronous())
			{
				FitBoxes(Static, Mesh.RelativeTransform, Source, Padding, Boxes);
			}
			else if (const USkeletalMesh* Skeletal = Mesh.SkeletalMesh.LoadSynchronous())
			{
				const FBox Bounds = Skeletal->GetBounds().GetBox();
				Boxes.Emplace(Mesh.RelativeTransform.TransformPosition(Bounds.GetCenter()),
					Bounds.GetExtent() * Mesh.RelativeTransform.GetScale3D().GetAbs() + FVector(Padding), Mesh.RelativeTransform.Rotator());
			}
		}
		if (Boxes.Num() == 0)
		{
			return false;
		}
		Spec.Shape.Footprint = Boxes;
		Spec.Shape.Clearance.Reset();
		Spec.ContactOffset = static_cast<float>(-Spec.Shape.GetLocalBounds().Min.Z);
		return true;
	}

	void ClearClassCache()
	{
		EGPlacementVisualsPrivate::ClassCache().Reset();
	}
}
