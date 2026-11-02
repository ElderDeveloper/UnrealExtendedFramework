// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementPoseSolver.h"

#include "Components/PrimitiveComponent.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementMath.h"
#include "UnrealExtendedGameplay/Systems/Placement/Profile/EGPlacementProfile.h"

void UEGPlacementPoseSolver::Solve_Implementation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
	const FEGPlacementContext& Context, FEGPlacementState& State) const
{
}

void UEGPlacementPoseSolver::Refuse(FEGPlacementState& State, EEGPlacementStatus Status, const FGameplayTag& Tag, FText Text) const
{
	State.SolveStatus = Status;
	State.SolveReasons.Emplace(Tag, MoveTemp(Text), this);
}

FVector UEGPlacementPoseSolver::GetIntentContact(const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent, float Scale)
{
	return Intent.Pose.GetLocation() - Intent.Pose.GetRotation().GetUpVector() * Spec.ContactOffset * Scale;
}

FVector UEGPlacementPoseSolver::GetIntentNormal(const FEGPlacementIntent& Intent)
{
	if (Intent.bHasSurfaceHint && !Intent.SurfaceNormal.IsNearlyZero())
	{
		return Intent.SurfaceNormal.GetSafeNormal();
	}
	if (Intent.Target.Kind == EEGPlacementTargetKind::Hit && Intent.Target.Hit.bBlockingHit)
	{
		return Intent.Target.Hit.ImpactNormal.GetSafeNormal();
	}
	return Intent.Pose.GetRotation().GetUpVector();
}

bool UEGPlacementPoseSolver::TraceSurface(const UEGPlacementProfile* Profile, const FEGPlacementIntent& Intent, const FEGPlacementContext& Context,
	const FVector& Start, const FVector& End, FHitResult& OutHit)
{
	if (Intent.Target.Kind == EEGPlacementTargetKind::Component)
	{
		UPrimitiveComponent* Component = Cast<UPrimitiveComponent>(Intent.Target.Component.Get());
		if (!Component)
		{
			return false;
		}
		FCollisionQueryParams Params = Profile->MakeQueryParams(Context, TEXT("EGPlacementSolve"), Profile->SurfaceTagMap.Num() > 0);
		return Component->LineTraceComponent(OutHit, Start, End, Params);
	}
	return Profile->LineTrace(Context, Start, End, OutHit, TEXT("EGPlacementSolve"));
}

FQuat UEGPlacementPoseSolver::SolveRotation(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent,
	const FTransform& GridFrame, const FVector& Up)
{
	const FEGPlacementRotationPolicy& Policy = Spec.Rotation;
	float Spin = 0.0f;
	if (Policy.bUseFixedSpin)
	{
		Spin = FRotator::NormalizeAxis(Policy.FixedSpinDegrees);
	}
	else
	{
		Spin = EGPlacementMath::ExtractSpin(Intent.Pose.GetRotation(), GridFrame.GetRotation(), Up) + Intent.SpinDegrees;
		const bool bFree = Intent.bFreeRotation && Policy.bAllowFreeSpin;
		Spin = bFree ? FRotator::NormalizeAxis(Spin) : EGPlacementMath::SnapAngle(Spin, Policy.SpinStepDegrees);
	}
	return EGPlacementMath::MakeRotation(GridFrame.GetRotation(), Up, Spin);
}
