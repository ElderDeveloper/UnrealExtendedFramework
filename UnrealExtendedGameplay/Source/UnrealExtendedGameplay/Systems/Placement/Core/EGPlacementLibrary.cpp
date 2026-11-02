// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementLibrary.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementEvaluator.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementMath.h"

FEGPlacementEvaluation UEGPlacementLibrary::Evaluate(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent, const FEGPlacementContext& Context)
{
	return EGPlacement::Evaluate(Profile, Spec, Intent, Context);
}

FEGPlacementState UEGPlacementLibrary::SolvePose(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent, const FEGPlacementContext& Context)
{
	return EGPlacement::SolvePose(Profile, Spec, Intent, Context);
}

FEGPlacementEvaluation UEGPlacementLibrary::ValidatePose(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent, const FEGPlacementState& State, const FEGPlacementContext& Context)
{
	return EGPlacement::ValidatePose(Profile, Spec, Intent, State, Context);
}

FEGPlacementEvaluation UEGPlacementLibrary::ValidateExactPose(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FTransform& Pose, UPrimitiveComponent* Support, const FEGPlacementContext& Context)
{
	const FEGPlacementState State = EGPlacement::MakeStateFromPose(Profile, Spec, Pose, Support, Context);
	FEGPlacementIntent Intent;
	Intent.Pose = Pose;
	return EGPlacement::ValidatePose(Profile, Spec, Intent, State, Context);
}

FEGPlacementContext UEGPlacementLibrary::MakePlacementContext(const UObject* WorldContextObject, AActor* Requester, EEGPlacementScope Scope)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	return EGPlacement::MakeContext(World, Requester, Scope);
}

FEGPlacementIntent UEGPlacementLibrary::MakeIntentAtPose(const FTransform& Pose, bool bFreePosition, bool bFreeRotation, float SpinDegrees)
{
	FEGPlacementIntent Intent;
	Intent.Pose = Pose;
	Intent.bFreePosition = bFreePosition;
	Intent.bFreeRotation = bFreeRotation;
	Intent.SpinDegrees = SpinDegrees;
	return Intent;
}

FEGPlacementHandle UEGPlacementLibrary::GetSupportHandle(const UPrimitiveComponent* Component)
{
	return EGPlacement::GetSupportHandle(Component);
}

FEGPlacementProposal UEGPlacementLibrary::MakeProposal(const FEGPlacementEvaluation& Evaluation, const FEGPlacementIntent& Intent, FGameplayTag ModeTag, const FEGPlacementHandle& Source)
{
	return FEGPlacementProposal::FromEvaluation(Evaluation, Intent, ModeTag, Source);
}

EEGPlacementOrientation UEGPlacementMathLibrary::ClassifyNormal(FVector Normal, const FEGPlacementSurfaceBands& Bands)
{
	return EGPlacementMath::ClassifyNormal(Normal, Bands);
}

float UEGPlacementMathLibrary::SlopeDegrees(FVector Normal, EEGPlacementOrientation Orientation)
{
	return EGPlacementMath::SlopeDegrees(Normal, Orientation);
}

FRotator UEGPlacementMathLibrary::MakePlacementRotation(FRotator Frame, FVector Up, float SpinDegrees)
{
	return EGPlacementMath::MakeRotation(Frame.Quaternion(), Up, SpinDegrees).Rotator();
}

float UEGPlacementMathLibrary::ExtractSpin(FRotator Rotation, FRotator Frame, FVector Up)
{
	return EGPlacementMath::ExtractSpin(Rotation.Quaternion(), Frame.Quaternion(), Up);
}

float UEGPlacementMathLibrary::SnapAngle(float Degrees, float Step)
{
	return EGPlacementMath::SnapAngle(Degrees, Step);
}

FVector UEGPlacementMathLibrary::SnapToGrid(FVector Location, const FTransform& Frame, FVector Step, FVector Origin, int32 SkipAxis)
{
	return EGPlacementMath::SnapToGrid(Location, Frame, Step, Origin, SkipAxis);
}

FEGPlacementOrientedBox UEGPlacementMathLibrary::TransformPlacementBox(const FEGPlacementBox& Box, const FTransform& Pose)
{
	return EGPlacementMath::TransformBox(Box, Pose);
}

bool UEGPlacementMathLibrary::BoxesIntersect(const FEGPlacementOrientedBox& A, const FEGPlacementOrientedBox& B)
{
	return EGPlacementMath::BoxesIntersect(A, B);
}

bool UEGPlacementMathLibrary::BoxContainsBox(const FEGPlacementOrientedBox& Outer, const FEGPlacementOrientedBox& Inner)
{
	return EGPlacementMath::BoxContainsBox(Outer, Inner);
}

bool UEGPlacementMathLibrary::BoxContainsPoint(const FEGPlacementOrientedBox& Box, FVector Point)
{
	return EGPlacementMath::BoxContainsPoint(Box, Point);
}

bool UEGPlacementMathLibrary::FitPlane(const TArray<FVector>& Points, FVector& Centroid, FVector& Normal)
{
	return EGPlacementMath::FitPlane(Points, Centroid, Normal);
}
