// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementAimProvider.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementEvaluator.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementMath.h"
#include "UnrealExtendedGameplay/Systems/Placement/Profile/EGPlacementProfile.h"
#include "UnrealExtendedGameplay/Systems/Placement/Spatial/EGPlacementAnchorComponent.h"
#include "UnrealExtendedGameplay/Systems/Placement/Spatial/EGPlacementSpatialSubsystem.h"

FQuat UEGPlacementAimProvider::MakeFacingRotation(const FVector& Up, const FVector& ViewDirection, bool bFaceViewer)
{
	const FVector UnitUp = Up.GetSafeNormal().IsNearlyZero() ? FVector::UpVector : Up.GetSafeNormal();
	FVector Forward = bFaceViewer ? -ViewDirection : ViewDirection;
	Forward = FVector::VectorPlaneProject(Forward, UnitUp);
	if (Forward.IsNearlyZero(1.e-4))
	{
		// Looking straight along the normal: face along any direction in the plane.
		FVector Unused;
		UnitUp.FindBestAxisVectors(Forward, Unused);
	}
	return FRotationMatrix::MakeFromZX(UnitUp, Forward.GetSafeNormal()).ToQuat();
}

// ---------------------------------------------------------------------------------------------
// View trace
// ---------------------------------------------------------------------------------------------

bool UEGPlacementAim_ViewTrace::BuildIntent_Implementation(const FEGPlacementView& View, const FEGPlacementSpec& Spec, const UEGPlacementProfile* Profile, const FEGPlacementContext& Context, FEGPlacementIntent& OutIntent)
{
	if (!Profile || !Context.World)
	{
		return false;
	}
	const FVector Direction = View.Direction.GetSafeNormal();
	const FVector End = View.Origin + Direction * GetCurrentDistance();

	auto Allowed = [&](const FHitResult& Hit)
	{
		return Spec.AllowsOrientation(EGPlacementMath::ClassifyNormal(Hit.ImpactNormal, Profile->SurfaceBands));
	};
	auto Search = [&](const FVector& From, FHitResult& OutHit)
	{
		if (SurfaceSearchDistance <= 0.0f)
		{
			return false;
		}
		FVector SearchDirection = FVector::ZeroVector;
		if (Spec.AllowsOrientation(EEGPlacementOrientation::Floor))
		{
			SearchDirection = -FVector::UpVector;
		}
		else if (Spec.AllowsOrientation(EEGPlacementOrientation::Ceiling))
		{
			SearchDirection = FVector::UpVector;
		}
		return !SearchDirection.IsZero()
			&& Profile->LineTrace(Context, From, From + SearchDirection * SurfaceSearchDistance, OutHit, TEXT("EGPlacementAim"))
			&& Allowed(OutHit);
	};

	FHitResult Hit;
	bool bFound = Profile->LineTrace(Context, View.Origin, End, Hit, TEXT("EGPlacementAim"));
	if (bFound && !Allowed(Hit))
	{
		// Step back off the refused surface before searching, so the search does not start inside it.
		FHitResult Searched;
		bFound = Search(FVector(Hit.ImpactPoint) + Hit.ImpactNormal * 2.0 - Direction * 2.0, Searched);
		if (bFound)
		{
			Hit = Searched;
		}
		else
		{
			// Keep the refused surface so the preview shows a refused ghost on it.
			bFound = true;
		}
	}
	else if (!bFound)
	{
		bFound = Search(End, Hit);
	}

	if (!bFound)
	{
		// Nothing to rest on: a pose in the air where the aim ends, which the solver refuses.
		OutIntent.Pose = FTransform(MakeFacingRotation(FVector::UpVector, Direction, bFaceViewer), End);
		OutIntent.bHasSurfaceHint = false;
		OutIntent.Target = FEGPlacementTarget();
		return true;
	}

	const FVector Normal = Hit.ImpactNormal.GetSafeNormal();
	const double Scale = OutIntent.Pose.GetScale3D().X > 0.0 ? OutIntent.Pose.GetScale3D().X : 1.0;
	OutIntent.Pose = FTransform(MakeFacingRotation(Normal, Direction, bFaceViewer), FVector(Hit.ImpactPoint) + Normal * Spec.ContactOffset * Scale, FVector(Scale));
	OutIntent.bHasSurfaceHint = true;
	OutIntent.SurfaceNormal = Normal;
	OutIntent.Target.Kind = EEGPlacementTargetKind::Hit;
	OutIntent.Target.Hit = Hit;
	return true;
}

void UEGPlacementAim_ViewTrace::AdjustDistance_Implementation(float Delta)
{
	CurrentDistance = FMath::Clamp(GetCurrentDistance() + Delta * DistanceStep, MinDistance, MaxDistance);
}

void UEGPlacementAim_ViewTrace::ResetAim_Implementation()
{
	CurrentDistance = MaxDistance;
}

// ---------------------------------------------------------------------------------------------
// Anchor pick
// ---------------------------------------------------------------------------------------------

double UEGPlacementAim_AnchorPick::Score(const UEGPlacementAnchorComponent& Anchor, const FEGPlacementView& View, const FEGPlacementSpec& Spec, const UEGPlacementProfile* Profile, const FEGPlacementContext& Context) const
{
	if (!Anchor.Accepts(Spec) || (!AnchorFilter.IsEmpty() && !AnchorFilter.Matches(Anchor.AnchorTags)))
	{
		return -1.0;
	}
	const FVector Direction = View.Direction.GetSafeNormal();
	const FVector Location = Anchor.GetAnchorTransform().GetLocation();
	const FVector ToAnchor = Location - View.Origin;
	const double Along = FVector::DotProduct(ToAnchor, Direction);
	if (Along <= 0.0 || Along > MaxDistance)
	{
		return -1.0;
	}
	const double Tolerance = FMath::Max(0.1, double(Anchor.SnapRadius) * AimToleranceScale);
	const double Off = (ToAnchor - Direction * Along).Size();
	if (Off > Tolerance)
	{
		return -1.0;
	}
	if (bSkipUnavailable && !Anchor.IsAvailableFor(Spec, Context))
	{
		return -1.0;
	}
	if (bRequireLineOfSight && Profile && Context.World)
	{
		FHitResult Hit;
		if (Profile->LineTrace(Context, View.Origin, Location, Hit, TEXT("EGPlacementAnchorSight"))
			&& Hit.GetActor() != Anchor.GetOwner()
			&& Hit.Distance < ToAnchor.Size() - Anchor.SnapRadius)
		{
			return -1.0;
		}
	}
	// Mostly how close to the ray, a little how close to the viewer.
	return Off / Tolerance + 0.1 * Along / MaxDistance;
}

bool UEGPlacementAim_AnchorPick::BuildIntent_Implementation(const FEGPlacementView& View, const FEGPlacementSpec& Spec, const UEGPlacementProfile* Profile, const FEGPlacementContext& Context, FEGPlacementIntent& OutIntent)
{
	UEGPlacementSpatialSubsystem* Spatial = EGPlacement::ResolveSpatial(Context);
	if (!Spatial)
	{
		Current.Reset();
		return false;
	}
	const FVector Direction = View.Direction.GetSafeNormal();
	TArray<UEGPlacementAnchorComponent*> Candidates;
	Spatial->QueryAnchors(View.Origin + Direction * (MaxDistance * 0.5f), MaxDistance * 0.5f + 100.0f, Candidates);

	UEGPlacementAnchorComponent* Best = nullptr;
	double BestScore = TNumericLimits<double>::Max();
	for (UEGPlacementAnchorComponent* Candidate : Candidates)
	{
		const double CandidateScore = Score(*Candidate, View, Spec, Profile, Context);
		if (CandidateScore >= 0.0 && CandidateScore < BestScore)
		{
			BestScore = CandidateScore;
			Best = Candidate;
		}
	}
	if (UEGPlacementAnchorComponent* Held = Current.Get(); Held && Best && Held != Best)
	{
		const double HeldScore = Score(*Held, View, Spec, Profile, Context);
		if (HeldScore >= 0.0 && BestScore > HeldScore * (1.0 - Hysteresis))
		{
			Best = Held;
		}
	}
	Current = Best;
	if (!Best)
	{
		return false;
	}
	OutIntent.Pose = Best->GetAnchorTransform();
	OutIntent.bHasSurfaceHint = false;
	OutIntent.Target = FEGPlacementTarget();
	OutIntent.Target.Kind = EEGPlacementTargetKind::Anchor;
	OutIntent.Target.Anchor = Best;
	return true;
}

void UEGPlacementAim_AnchorPick::ResetAim_Implementation()
{
	Current.Reset();
}
