// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementNetTypes.h"

#include "Components/SceneComponent.h"
#include "Engine/NetSerialization.h"
#include "UObject/CoreNet.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementMath.h"
#include "UnrealExtendedGameplay/Systems/Placement/Spatial/EGPlacementAnchorComponent.h"

namespace EGPlacementNetPrivate
{
	constexpr double LocationQuantum = 0.01;
	constexpr int32 MaxRevisions = 64;

	void SerializeObject(FArchive& Ar, UPackageMap* Map, TWeakObjectPtr<UObject>& Weak, bool& bOutSuccess)
	{
		UObject* Object = Weak.Get();
		if (Map)
		{
			bOutSuccess &= Map->SerializeObject(Ar, UObject::StaticClass(), Object);
		}
		else
		{
			// Local copies (demo recording, tests) keep the path instead.
			FSoftObjectPath Path(Object);
			Ar << Path;
			if (Ar.IsLoading())
			{
				Object = Path.ResolveObject();
			}
		}
		if (Ar.IsLoading())
		{
			Weak = Object;
		}
	}

	void SerializeHandle(FArchive& Ar, FEGPlacementHandle& Handle)
	{
		UPackageMap::StaticSerializeName(Ar, Handle.Namespace);
		Ar << Handle.Value;
	}
}

FEGPlacementProposal FEGPlacementProposal::FromEvaluation(const FEGPlacementEvaluation& Evaluation, const FEGPlacementIntent& Intent, FGameplayTag InModeTag, const FEGPlacementHandle& InSource)
{
	FEGPlacementProposal Proposal;
	Proposal.Location = EGPlacementMath::QuantizeVector(Evaluation.Pose.GetLocation(), EGPlacementNetPrivate::LocationQuantum);
	Proposal.Rotation = Evaluation.Pose.GetRotation().GetNormalized();
	Proposal.Scale = static_cast<float>(Evaluation.Pose.GetScale3D().X);
	Proposal.ModeTag = InModeTag;
	Proposal.Source = InSource;
	Proposal.Claims.bHasClaims = true;
	Proposal.Claims.Orientation = Evaluation.Orientation;
	Proposal.Claims.Support = Evaluation.Support.Handle;
	Proposal.Claims.Anchor = Evaluation.Anchor;
	Proposal.Claims.Revisions = Evaluation.Revisions;
	if (Evaluation.Anchor.IsValid())
	{
		Proposal.Target = Evaluation.Anchor.Get();
	}
	else if (Intent.Target.Kind == EEGPlacementTargetKind::Component)
	{
		Proposal.Target = Intent.Target.Component.Get();
	}
	Proposal.bFreePosition = Intent.bFreePosition;
	Proposal.bFreeRotation = Intent.bFreeRotation;
	return Proposal;
}

FEGPlacementIntent FEGPlacementProposal::ToIntent() const
{
	FEGPlacementIntent Intent;
	Intent.Pose = GetPose();
	Intent.bFreePosition = bFreePosition;
	Intent.bFreeRotation = bFreeRotation;
	Intent.Claims = Claims;
	if (UEGPlacementAnchorComponent* Anchor = Cast<UEGPlacementAnchorComponent>(Target.Get()))
	{
		Intent.Target.Kind = EEGPlacementTargetKind::Anchor;
		Intent.Target.Anchor = Anchor;
	}
	else if (USceneComponent* Component = Cast<USceneComponent>(Target.Get()))
	{
		Intent.Target.Kind = EEGPlacementTargetKind::Component;
		Intent.Target.Component = Component;
	}
	return Intent;
}

bool FEGPlacementProposal::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	using namespace EGPlacementNetPrivate;

	bOutSuccess = true;
	if (Ar.IsSaving())
	{
		Location = EGPlacementMath::QuantizeVector(Location, LocationQuantum);
	}
	bOutSuccess &= SerializePackedVector<100, 30>(Location, Ar);

	bool bRotationOk = true;
	Rotation.NetSerialize(Ar, Map, bRotationOk);
	bOutSuccess &= bRotationOk;
	Ar << Scale;

	bool bTagOk = true;
	ModeTag.NetSerialize(Ar, Map, bTagOk);
	bOutSuccess &= bTagOk;
	SerializeHandle(Ar, Source);

	uint8 Flags = (bFreePosition ? 1 : 0) | (bFreeRotation ? 2 : 0) | (Claims.bHasClaims ? 4 : 0);
	Ar << Flags;
	bFreePosition = (Flags & 1) != 0;
	bFreeRotation = (Flags & 2) != 0;
	Claims.bHasClaims = (Flags & 4) != 0;

	uint8 Orientation = static_cast<uint8>(Claims.Orientation);
	Ar << Orientation;
	Claims.Orientation = static_cast<EEGPlacementOrientation>(Orientation);
	SerializeHandle(Ar, Claims.Support);

	SerializeObject(Ar, Map, Target, bOutSuccess);
	TWeakObjectPtr<UObject> Anchor = Claims.Anchor.Get();
	SerializeObject(Ar, Map, Anchor, bOutSuccess);
	if (Ar.IsLoading())
	{
		Claims.Anchor = Cast<UEGPlacementAnchorComponent>(Anchor.Get());
	}

	uint8 Count = static_cast<uint8>(FMath::Min(Claims.Revisions.Num(), MaxRevisions));
	Ar << Count;
	if (Ar.IsLoading())
	{
		if (Count > MaxRevisions)
		{
			bOutSuccess = false;
			return false;
		}
		Claims.Revisions.SetNum(Count);
	}
	for (int32 Index = 0; Index < Count; ++Index)
	{
		SerializeObject(Ar, Map, Claims.Revisions[Index].Source, bOutSuccess);
		Ar << Claims.Revisions[Index].Revision;
	}
	return bOutSuccess;
}
