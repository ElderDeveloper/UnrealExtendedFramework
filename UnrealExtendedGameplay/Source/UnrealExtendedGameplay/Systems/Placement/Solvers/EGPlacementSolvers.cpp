// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementSolvers.h"

#include "Components/PrimitiveComponent.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementEvaluator.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementMath.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementTags.h"
#include "UnrealExtendedGameplay/Systems/Placement/Profile/EGPlacementProfile.h"
#include "UnrealExtendedGameplay/Systems/Placement/Spatial/EGPlacementAnchorComponent.h"
#include "UnrealExtendedGameplay/Systems/Placement/Spatial/EGPlacementSpatialSubsystem.h"

#define LOCTEXT_NAMESPACE "EGPlacementSolvers"

// ---------------------------------------------------------------------------------------------
// Surface
// ---------------------------------------------------------------------------------------------

void UEGPlacementSolver_Surface::Solve_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
	const FEGPlacementContext& Context, FEGPlacementState& State) const
{
	const float Scale = State.Scale;
	const FVector ProbeNormal = GetIntentNormal(Intent);
	const FVector Contact = GetIntentContact(Spec, Intent, Scale);
	const double Probe = Profile->Query.ProbeDistance + FMath::Abs(Spec.ContactOffset) * Scale;

	FHitResult Hit;
	if (!TraceSurface(Profile, Intent, Context, Contact + ProbeNormal * Probe, Contact - ProbeNormal * Probe, Hit))
	{
		Refuse(State, EEGPlacementStatus::Invalid, TAG_EGPlacement_Reason_NoSurface, LOCTEXT("NoSurface", "No surface under the aim."));
		return;
	}

	FVector Point = Hit.ImpactPoint;
	FVector Normal = Hit.ImpactNormal.GetSafeNormal();
	const FEGPlacementGrid& Grid = Profile->GetGrid(Spec);
	if (Grid.bEnabled && !Intent.bFreePosition)
	{
		// Snap across the surface and ground again along the remaining grid axis, so snapping and
		// grounding never undo each other and a re-solve lands on the same cell.
		const FTransform Frame = EGPlacement::GetGridFrame(Grid, Hit.GetComponent());
		const int32 Axis = EGPlacementMath::DominantAxis(Frame.GetRotation(), Normal);
		const FVector Snapped = EGPlacementMath::SnapToGrid(Point, Frame, Grid.Step, Grid.Origin, Axis);
		FVector GroundAxis = Axis == 0 ? Frame.GetRotation().GetAxisX() : (Axis == 1 ? Frame.GetRotation().GetAxisY() : Frame.GetRotation().GetAxisZ());
		if (FVector::DotProduct(GroundAxis, Normal) < 0.0)
		{
			GroundAxis = -GroundAxis;
		}
		FHitResult Grounded;
		if (!TraceSurface(Profile, Intent, Context, Snapped + GroundAxis * Probe, Snapped - GroundAxis * Probe, Grounded))
		{
			State.Pose.SetLocation(Snapped);
			Refuse(State, EEGPlacementStatus::Invalid, TAG_EGPlacement_Reason_NoSurface, LOCTEXT("OffGrid", "The grid cell is off the surface."));
			return;
		}
		Hit = Grounded;
		Point = Grounded.ImpactPoint;
		Normal = Grounded.ImpactNormal.GetSafeNormal();
	}

	const EEGPlacementOrientation Orientation = EGPlacementMath::ClassifyNormal(Normal, Profile->SurfaceBands);
	const FVector Up = EGPlacementMath::AlignedUp(Normal, Orientation, Spec.Rotation.Alignment);
	const FTransform SpinFrame = EGPlacement::GetGridFrame(Grid, Hit.GetComponent());
	const FQuat Rotation = SolveRotation(Profile, Spec, Intent, SpinFrame, Up);

	State.Pose = FTransform(Rotation, Point + Up * Spec.ContactOffset * Scale, FVector(Scale));
	State.bHasSurface = true;
	State.SurfaceHit = Hit;
	State.Normal = Normal;
	State.Support.Component = Hit.GetComponent();
}

// ---------------------------------------------------------------------------------------------
// Exact pose
// ---------------------------------------------------------------------------------------------

void UEGPlacementSolver_ExactPose::Solve_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
	const FEGPlacementContext& Context, FEGPlacementState& State) const
{
	State.Pose = Intent.Pose;
	State.Normal = Intent.Pose.GetRotation().GetUpVector();
	if (Intent.Target.Kind == EEGPlacementTargetKind::Component)
	{
		State.Support.Component = Cast<UPrimitiveComponent>(Intent.Target.Component.Get());
	}
	if (bMeasureSupport)
	{
		const FVector Up = State.GetUp();
		const FVector Contact = State.GetContactPoint(Spec.ContactOffset);
		const double Tolerance = Profile->ContactTolerance;
		FHitResult Hit;
		if (TraceSurface(Profile, Intent, Context, Contact + Up * Tolerance, Contact - Up * MeasureDistance, Hit))
		{
			State.bHasSurface = true;
			State.SurfaceHit = Hit;
			State.Normal = Hit.ImpactNormal.GetSafeNormal();
			if (!State.Support.Component.IsValid())
			{
				State.Support.Component = Hit.GetComponent();
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Anchor
// ---------------------------------------------------------------------------------------------

FTransform UEGPlacementSolver_Anchor::MakeAnchorPose(const UEGPlacementAnchorComponent& Anchor, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent, float Scale) const
{
	const FTransform AnchorTransform = Anchor.GetAnchorTransform();
	const FQuat Frame = AnchorTransform.GetRotation();
	const FVector Up = Frame.GetUpVector();
	FQuat Rotation = Frame;
	if (Anchor.bAllowSpin)
	{
		float Spin = EGPlacementMath::ExtractSpin(Intent.Pose.GetRotation(), Frame, Up) + Intent.SpinDegrees;
		const bool bFree = Intent.bFreeRotation && Spec.Rotation.bAllowFreeSpin;
		Spin = bFree ? FRotator::NormalizeAxis(Spin) : EGPlacementMath::SnapAngle(Spin, Anchor.SpinStepDegrees);
		Rotation = EGPlacementMath::MakeRotation(Frame, Up, Spin);
	}
	FVector Location = AnchorTransform.GetLocation();
	if (bAnchorsMarkContact)
	{
		Location += Up * Spec.ContactOffset * Scale;
	}
	return FTransform(Rotation, Location, FVector(Scale));
}

void UEGPlacementSolver_Anchor::Solve_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
	const FEGPlacementContext& Context, FEGPlacementState& State) const
{
	const float Scale = State.Scale;
	const FVector Pivot = Intent.Pose.GetLocation();

	UEGPlacementAnchorComponent* Chosen = nullptr;
	UEGPlacementAnchorComponent* Named = Intent.Target.Kind == EEGPlacementTargetKind::Anchor ? Intent.Target.Anchor.Get() : Intent.Claims.Anchor.Get();
	if (Named)
	{
		if (!Named->Accepts(Spec) || !Named->IsAvailableFor(Spec, Context))
		{
			State.Anchor = Named;
			State.Pose = MakeAnchorPose(*Named, Spec, Intent, Scale);
			Refuse(State, EEGPlacementStatus::Invalid, TAG_EGPlacement_Reason_AnchorUnavailable, LOCTEXT("AnchorTaken", "The anchor does not take this object now."));
			return;
		}
		Chosen = Named;
	}
	else if (UEGPlacementSpatialSubsystem* Spatial = EGPlacement::ResolveSpatial(Context))
	{
		TArray<UEGPlacementAnchorComponent*> Candidates;
		Spatial->QueryAnchors(Pivot, MaxSearchRadius, Candidates);
		double BestDistance = TNumericLimits<double>::Max();
		for (UEGPlacementAnchorComponent* Candidate : Candidates)
		{
			const double Distance = FVector::Dist(Candidate->GetAnchorTransform().GetLocation(), Pivot);
			if (Distance <= Candidate->SnapRadius && Distance < BestDistance && Candidate->Accepts(Spec) && Candidate->IsAvailableFor(Spec, Context))
			{
				BestDistance = Distance;
				Chosen = Candidate;
			}
		}
	}

	if (!Chosen)
	{
		Refuse(State, EEGPlacementStatus::Invalid, TAG_EGPlacement_Reason_AnchorUnavailable, LOCTEXT("NoAnchor", "No compatible free anchor here."));
		return;
	}
	State.Anchor = Chosen;
	State.Pose = MakeAnchorPose(*Chosen, Spec, Intent, Scale);
	State.Normal = Chosen->GetAnchorTransform().GetRotation().GetUpVector();
	State.Support.Component = Chosen->GetSupportPrimitive();
}

// ---------------------------------------------------------------------------------------------
// Ground conform
// ---------------------------------------------------------------------------------------------

namespace EGPlacementGroundConformPrivate
{
	void AddUniqueXY(TArray<FVector2D>& Points, const FVector& Point)
	{
		const FVector2D XY(Point.X, Point.Y);
		for (const FVector2D& Existing : Points)
		{
			if (FVector2D::DistSquared(Existing, XY) < 0.25)
			{
				return;
			}
		}
		Points.Add(XY);
	}
}

void UEGPlacementSolver_GroundConform::Solve_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
	const FEGPlacementContext& Context, FEGPlacementState& State) const
{
	using namespace EGPlacementGroundConformPrivate;

	const float Scale = State.Scale;
	const double Lift = Profile->Query.ProbeLift;
	const FVector Down = -FVector::UpVector;
	FVector Contact = GetIntentContact(Spec, Intent, Scale);

	auto ProbeAt = [&](const FVector2D& XY, double FromZ, FHitResult& OutHit)
	{
		const FVector Start(XY.X, XY.Y, FromZ + Lift);
		const FVector End(XY.X, XY.Y, FromZ - ProbeDepth);
		return TraceSurface(Profile, Intent, Context, Start, End, OutHit);
	};

	FHitResult CenterHit;
	bool bCenter = ProbeAt(FVector2D(Contact.X, Contact.Y), Contact.Z, CenterHit);

	const FEGPlacementGrid& Grid = Profile->GetGrid(Spec);
	if (Grid.bEnabled && !Intent.bFreePosition)
	{
		const FTransform Frame = EGPlacement::GetGridFrame(Grid, bCenter ? CenterHit.GetComponent() : nullptr);
		const int32 Axis = EGPlacementMath::DominantAxis(Frame.GetRotation(), FVector::UpVector);
		Contact = EGPlacementMath::SnapToGrid(Contact, Frame, Grid.Step, Grid.Origin, Axis);
		bCenter = ProbeAt(FVector2D(Contact.X, Contact.Y), Contact.Z, CenterHit);
	}

	// Spin is measured level, about world up.
	const FTransform SpinFrame = EGPlacement::GetGridFrame(Grid, bCenter ? CenterHit.GetComponent() : nullptr);
	const FQuat LevelRotation = SolveRotation(Profile, Spec, Intent, SpinFrame, FVector::UpVector);
	const float Spin = EGPlacementMath::ExtractSpin(LevelRotation, SpinFrame.GetRotation(), FVector::UpVector);

	const FTransform LevelPose(LevelRotation, Contact + FVector::UpVector * Spec.ContactOffset * Scale, FVector(Scale));
	TArray<FVector2D> ProbePoints;
	ProbePoints.Add(FVector2D(Contact.X, Contact.Y));
	for (const FEGPlacementBox& Box : Spec.Shape.Footprint)
	{
		FVector Corners[8];
		EGPlacementMath::TransformBox(Box, LevelPose).GetCorners(Corners);
		for (int32 Index = 0; Index < 8; ++Index)
		{
			AddUniqueXY(ProbePoints, Corners[Index]);
			if (bProbeEdgeMidpoints)
			{
				for (int32 Bit = 1; Bit < 8; Bit <<= 1)
				{
					if ((Index & Bit) == 0)
					{
						AddUniqueXY(ProbePoints, (Corners[Index] + Corners[Index | Bit]) * 0.5);
					}
				}
			}
		}
	}

	TArray<FVector> Hits;
	FHitResult FirstHit;
	bool bAnyHit = bCenter;
	if (bCenter)
	{
		Hits.Add(CenterHit.ImpactPoint);
		FirstHit = CenterHit;
	}
	for (int32 Index = 1; Index < ProbePoints.Num(); ++Index)
	{
		FHitResult Hit;
		if (ProbeAt(ProbePoints[Index], Contact.Z, Hit))
		{
			Hits.Add(Hit.ImpactPoint);
			if (!bAnyHit)
			{
				FirstHit = Hit;
				bAnyHit = true;
			}
		}
	}
	if (!bAnyHit || (!bCenter && Hits.Num() < 3))
	{
		State.Pose = LevelPose;
		Refuse(State, EEGPlacementStatus::Invalid, TAG_EGPlacement_Reason_NoSurface, LOCTEXT("NoGround", "No ground under the object."));
		return;
	}

	FVector Centroid;
	FVector Normal;
	if (!EGPlacementMath::FitPlane(Hits, Centroid, Normal))
	{
		Centroid = FirstHit.ImpactPoint;
		Normal = FirstHit.ImpactNormal.GetSafeNormal();
		if (Normal.Z <= UE_KINDA_SMALL_NUMBER)
		{
			Normal = FVector::UpVector;
		}
	}
	if (bRestOnHighestContact)
	{
		double Raise = 0.0;
		for (const FVector& Point : Hits)
		{
			Raise = FMath::Max(Raise, FVector::DotProduct(Point - Centroid, Normal));
		}
		Centroid += Normal * Raise;
	}
	// The point of the fitted plane straight below (or above) the contact.
	const double PlaneZ = Centroid.Z - (Normal.X * (Contact.X - Centroid.X) + Normal.Y * (Contact.Y - Centroid.Y)) / Normal.Z;
	const FVector Rest(Contact.X, Contact.Y, PlaneZ);

	const EEGPlacementOrientation Orientation = EGPlacementMath::ClassifyNormal(Normal, Profile->SurfaceBands);
	const FVector Up = EGPlacementMath::AlignedUp(Normal, Orientation, Spec.Rotation.Alignment);
	const FQuat Rotation = EGPlacementMath::MakeRotation(SpinFrame.GetRotation(), Up, Spin);

	State.Pose = FTransform(Rotation, Rest + Up * Spec.ContactOffset * Scale, FVector(Scale));
	State.bHasSurface = true;
	State.SurfaceHit = FirstHit;
	State.Normal = Normal;
	State.Support.Component = FirstHit.GetComponent();
}

#undef LOCTEXT_NAMESPACE
