// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementRules_Geometry.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementMath.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementSupportInterface.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementTags.h"
#include "UnrealExtendedGameplay/Systems/Placement/Profile/EGPlacementProfile.h"

#define LOCTEXT_NAMESPACE "EGPlacementRules"

namespace EGPlacementGeometryRulePrivate
{
	/**
	 * The part of Box at least Height above the plane through Point with normal Up, along the box
	 * axis closest to Up. Exact when that axis is parallel to Up, conservative otherwise. False when
	 * nothing is left.
	 */
	bool TrimBelow(const FEGPlacementOrientedBox& Box, const FVector& Point, const FVector& Up, double Height, FEGPlacementOrientedBox& OutBox)
	{
		const int32 Axis = EGPlacementMath::DominantAxis(Box.Rotation, Up);
		FVector Direction = Box.GetAxis(Axis);
		if (FVector::DotProduct(Direction, Up) < 0.0)
		{
			Direction = -Direction;
		}
		const double Extent = Box.Extent[Axis];
		const double Cut = FVector::DotProduct(Point + Up * Height - Box.Center, Direction);
		const double Low = FMath::Max(-Extent, Cut);
		if (Low >= Extent)
		{
			return false;
		}
		OutBox = Box;
		OutBox.Extent[Axis] = (Extent - Low) * 0.5;
		OutBox.Center = Box.Center + Direction * ((Low + Extent) * 0.5);
		return true;
	}

	bool HasAnyTag(const AActor* Actor, const UPrimitiveComponent* Component, const TArray<FName>& Tags)
	{
		for (const FName& Tag : Tags)
		{
			if ((Actor && Actor->ActorHasTag(Tag)) || (Component && Component->ComponentHasTag(Tag)))
			{
				return true;
			}
		}
		return false;
	}
}

// ---------------------------------------------------------------------------------------------
// Clearance
// ---------------------------------------------------------------------------------------------

EEGPlacementRuleResult UEGPlacementRule_Clearance::Check_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
	const FEGPlacementContext& Context, const FEGPlacementState& State, FEGPlacementRuleOutput& Output) const
{
	if (!Profile || !Context.World)
	{
		return EEGPlacementRuleResult::Pass;
	}
	const UPrimitiveComponent* Support = State.Support.Component.Get();
	const FVector Up = State.GetUp();
	const FVector Contact = State.GetContactPoint(Spec.ContactOffset);
	const FCollisionQueryParams Params = Profile->MakeQueryParams(Context, TEXT("EGPlacementClearance"));

	for (const FEGPlacementOrientedBox& RawBox : State.ClearanceBoxes)
	{
		const FEGPlacementOrientedBox Box = EGPlacementMath::Inflate(RawBox, -Skin);
		if (Box.Extent.GetMin() <= 0.0)
		{
			continue;
		}
		TArray<FOverlapResult> Overlaps;
		if (bUseProfileQuery)
		{
			Profile->OverlapBox(Context, Box, Overlaps, TEXT("EGPlacementClearance"));
		}
		else
		{
			Context.World->OverlapMultiByChannel(Overlaps, Box.Center, Box.Rotation, Channel, FCollisionShape::MakeBox(FVector3f(Box.Extent)), Params);
		}
		for (const FOverlapResult& Overlap : Overlaps)
		{
			const UPrimitiveComponent* Component = Overlap.GetComponent();
			if (!Overlap.bBlockingHit || !Component)
			{
				continue;
			}
			const AActor* Owner = Component->GetOwner();
			if (bIgnorePawns && Cast<APawn>(Owner))
			{
				continue;
			}
			if (Component == Support)
			{
				// The support may only touch the band just above the contact face.
				FEGPlacementOrientedBox Above;
				if (!EGPlacementGeometryRulePrivate::TrimBelow(Box, Contact, Up, Profile->ContactTolerance, Above))
				{
					continue;
				}
				if (!Component->OverlapComponent(Above.Center, Above.Rotation, FCollisionShape::MakeBox(FVector3f(Above.Extent))))
				{
					continue;
				}
			}
			return Fail(Output, TAG_EGPlacement_Reason_Blocked,
				FText::Format(LOCTEXT("Blocked", "Blocked by {0}."), FText::FromString(Owner ? Owner->GetActorNameOrLabel() : Component->GetName())));
		}
	}
	return EEGPlacementRuleResult::Pass;
}

// ---------------------------------------------------------------------------------------------
// Support coverage
// ---------------------------------------------------------------------------------------------

EEGPlacementRuleResult UEGPlacementRule_SupportCoverage::Check_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
	const FEGPlacementContext& Context, const FEGPlacementState& State, FEGPlacementRuleOutput& Output) const
{
	if (!Profile || (bPassAnchored && State.Anchor.IsValid()) || !State.bHasSurface)
	{
		return EEGPlacementRuleResult::Pass;
	}
	const FVector Up = State.GetUp();
	const FVector Contact = State.GetContactPoint(Spec.ContactOffset);
	const double Tolerance = Profile->ContactTolerance;
	const UPrimitiveComponent* MainSupport = State.Support.Component.Get();

	int32 Probes = 0;
	int32 Supported = 0;
	for (const FEGPlacementOrientedBox& Box : State.FootprintBoxes)
	{
		const int32 Normal = EGPlacementMath::DominantAxis(Box.Rotation, Up);
		const int32 AxisU = (Normal + 1) % 3;
		const int32 AxisV = (Normal + 2) % 3;
		const FVector DirU = Box.GetAxis(AxisU);
		const FVector DirV = Box.GetAxis(AxisV);
		const double ExtentU = Box.Extent[AxisU];
		const double ExtentV = Box.Extent[AxisV];

		const int32 Cap = FMath::Max(2, FMath::FloorToInt32(FMath::Sqrt(static_cast<float>(MaxProbesPerBox))));
		const int32 CountU = FMath::Clamp(FMath::CeilToInt32(2.0 * ExtentU / Spacing) + 1, 2, Cap);
		const int32 CountV = FMath::Clamp(FMath::CeilToInt32(2.0 * ExtentV / Spacing) + 1, 2, Cap);
		for (int32 IndexU = 0; IndexU < CountU; ++IndexU)
		{
			for (int32 IndexV = 0; IndexV < CountV; ++IndexV)
			{
				// Inset half a centimetre from the edges so edge probes do not graze the rim.
				const double U = FMath::Lerp(-ExtentU + 0.5, ExtentU - 0.5, IndexU / double(CountU - 1));
				const double V = FMath::Lerp(-ExtentV + 0.5, ExtentV - 0.5, IndexV / double(CountV - 1));
				FVector Point = Box.Center + DirU * U + DirV * V;
				Point -= Up * FVector::DotProduct(Point - Contact, Up);
				FHitResult Hit;
				++Probes;
				if (Profile->LineTrace(Context, Point + Up * Tolerance, Point - Up * (Tolerance * 2.0 + 1.0), Hit, TEXT("EGPlacementCoverage"))
					&& (!bRequireSameSupport || Hit.GetComponent() == MainSupport))
				{
					++Supported;
				}
			}
		}
	}
	if (Probes == 0 || Supported >= FMath::CeilToInt32(MinFraction * Probes - UE_KINDA_SMALL_NUMBER))
	{
		return EEGPlacementRuleResult::Pass;
	}
	return Fail(Output, TAG_EGPlacement_Reason_Overhang,
		FText::Format(LOCTEXT("Overhang", "Only {0} of the footprint rests on support."), FText::AsPercent(double(Supported) / double(Probes))));
}

// ---------------------------------------------------------------------------------------------
// Support filter
// ---------------------------------------------------------------------------------------------

EEGPlacementRuleResult UEGPlacementRule_SupportFilter::Check_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
	const FEGPlacementContext& Context, const FEGPlacementState& State, FEGPlacementRuleOutput& Output) const
{
	using namespace EGPlacementGeometryRulePrivate;

	const UPrimitiveComponent* Component = State.Support.Component.Get();
	const AActor* Actor = State.Support.Actor.Get();
	if (!Component && !Actor)
	{
		return bRequireSupport && !State.Anchor.IsValid()
			? Fail(Output, TAG_EGPlacement_Reason_SupportRejected, LOCTEXT("NoSupport", "The object would rest on nothing."))
			: EEGPlacementRuleResult::Pass;
	}
	if (bRefusePawns && Cast<APawn>(Actor))
	{
		return Fail(Output, TAG_EGPlacement_Reason_SupportRejected, LOCTEXT("Pawn", "Nothing can be placed on a character."));
	}
	if (bRefuseMovable && Component && Component->Mobility == EComponentMobility::Movable)
	{
		return Fail(Output, TAG_EGPlacement_Reason_SupportRejected, LOCTEXT("Movable", "The support moves."));
	}
	if (Actor && AllowedClasses.Num() > 0 && !AllowedClasses.ContainsByPredicate([Actor](const TSubclassOf<AActor>& Class) { return Class && Actor->IsA(Class); }))
	{
		return Fail(Output, TAG_EGPlacement_Reason_SupportRejected, LOCTEXT("NotAllowed", "The object does not go on this."));
	}
	if (Actor && RefusedClasses.ContainsByPredicate([Actor](const TSubclassOf<AActor>& Class) { return Class && Actor->IsA(Class); }))
	{
		return Fail(Output, TAG_EGPlacement_Reason_SupportRejected, LOCTEXT("Refused", "The object does not go on this."));
	}
	if (RequiredTags.Num() > 0 && !HasAnyTag(Actor, Component, RequiredTags))
	{
		return Fail(Output, TAG_EGPlacement_Reason_SupportRejected, LOCTEXT("MissingTag", "The support lacks a required tag."));
	}
	if (HasAnyTag(Actor, Component, RefusedTags))
	{
		return Fail(Output, TAG_EGPlacement_Reason_SupportRejected, LOCTEXT("RefusedTag", "The support refuses placement."));
	}
	if (bAskSupport && Actor && Actor->GetClass()->ImplementsInterface(UEGPlacementSupportInterface::StaticClass())
		&& !IEGPlacementSupportInterface::Execute_CanSupportPlacement(const_cast<AActor*>(Actor), Component, Spec, Context))
	{
		return Fail(Output, TAG_EGPlacement_Reason_SupportRejected, LOCTEXT("Declined", "The support declines the object."));
	}
	return EEGPlacementRuleResult::Pass;
}

#undef LOCTEXT_NAMESPACE
