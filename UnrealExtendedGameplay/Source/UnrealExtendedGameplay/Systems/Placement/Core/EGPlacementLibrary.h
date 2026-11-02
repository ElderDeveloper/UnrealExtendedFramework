// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementNetTypes.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementTypes.h"

#include "EGPlacementLibrary.generated.h"

class UEGPlacementProfile;
class UPrimitiveComponent;

/** The evaluator for Blueprint: scripts, AI and editor tools place through it without a session. */
UCLASS(meta = (ScriptName = "EGPlacementLibrary"))
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** SolvePose, then ValidatePose. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	static FEGPlacementEvaluation Evaluate(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent, const FEGPlacementContext& Context);

	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	static FEGPlacementState SolvePose(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent, const FEGPlacementContext& Context);

	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	static FEGPlacementEvaluation ValidatePose(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FEGPlacementIntent& Intent, const FEGPlacementState& State, const FEGPlacementContext& Context);

	/** Runs every rule on a pose obtained elsewhere: a slot, a scripted spawn, a project solver. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Placement")
	static FEGPlacementEvaluation ValidateExactPose(const UEGPlacementProfile* Profile, const FEGPlacementSpec& Spec, const FTransform& Pose, UPrimitiveComponent* Support, const FEGPlacementContext& Context);

	UFUNCTION(BlueprintPure, Category = "Extended|Placement", meta = (WorldContext = "WorldContextObject"))
	static FEGPlacementContext MakePlacementContext(const UObject* WorldContextObject, AActor* Requester, EEGPlacementScope Scope = EEGPlacementScope::Preview);

	/** An intent at Pose, optionally with the surface normal the caller saw. */
	UFUNCTION(BlueprintPure, Category = "Extended|Placement")
	static FEGPlacementIntent MakeIntentAtPose(const FTransform& Pose, bool bFreePosition = false, bool bFreeRotation = false, float SpinDegrees = 0.0f);

	UFUNCTION(BlueprintPure, Category = "Extended|Placement")
	static bool IsPlacementValid(const FEGPlacementEvaluation& Evaluation) { return Evaluation.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Extended|Placement")
	static bool HasPlacementReason(const FEGPlacementEvaluation& Evaluation, FGameplayTag Tag) { return Evaluation.HasReason(Tag); }

	UFUNCTION(BlueprintPure, Category = "Extended|Placement")
	static FEGPlacementHandle GetSupportHandle(const UPrimitiveComponent* Component);

	UFUNCTION(BlueprintPure, Category = "Extended|Placement")
	static FEGPlacementProposal MakeProposal(const FEGPlacementEvaluation& Evaluation, const FEGPlacementIntent& Intent, FGameplayTag ModeTag, const FEGPlacementHandle& Source);

	UFUNCTION(BlueprintPure, Category = "Extended|Placement")
	static FEGPlacementIntent ProposalToIntent(const FEGPlacementProposal& Proposal) { return Proposal.ToIntent(); }

	UFUNCTION(BlueprintPure, Category = "Extended|Placement")
	static FEGPlacementHandle MakePlacementHandle(FName Namespace, const FString& Value) { return FEGPlacementHandle(Namespace, Value); }
};

/** The stateless placement math for Blueprint. */
UCLASS(meta = (ScriptName = "EGPlacementMathLibrary"))
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementMathLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Extended|Placement|Math")
	static EEGPlacementOrientation ClassifyNormal(FVector Normal, const FEGPlacementSurfaceBands& Bands);

	UFUNCTION(BlueprintPure, Category = "Extended|Placement|Math")
	static float SlopeDegrees(FVector Normal, EEGPlacementOrientation Orientation);

	UFUNCTION(BlueprintPure, Category = "Extended|Placement|Math")
	static FRotator MakePlacementRotation(FRotator Frame, FVector Up, float SpinDegrees);

	UFUNCTION(BlueprintPure, Category = "Extended|Placement|Math")
	static float ExtractSpin(FRotator Rotation, FRotator Frame, FVector Up);

	UFUNCTION(BlueprintPure, Category = "Extended|Placement|Math")
	static float SnapAngle(float Degrees, float Step);

	/** Snaps on every grid axis except SkipAxis (0 X, 1 Y, 2 Z, -1 none). */
	UFUNCTION(BlueprintPure, Category = "Extended|Placement|Math")
	static FVector SnapToGrid(FVector Location, const FTransform& Frame, FVector Step, FVector Origin, int32 SkipAxis = -1);

	UFUNCTION(BlueprintPure, Category = "Extended|Placement|Math")
	static FEGPlacementOrientedBox TransformPlacementBox(const FEGPlacementBox& Box, const FTransform& Pose);

	UFUNCTION(BlueprintPure, Category = "Extended|Placement|Math")
	static bool BoxesIntersect(const FEGPlacementOrientedBox& A, const FEGPlacementOrientedBox& B);

	UFUNCTION(BlueprintPure, Category = "Extended|Placement|Math")
	static bool BoxContainsBox(const FEGPlacementOrientedBox& Outer, const FEGPlacementOrientedBox& Inner);

	UFUNCTION(BlueprintPure, Category = "Extended|Placement|Math")
	static bool BoxContainsPoint(const FEGPlacementOrientedBox& Box, FVector Point);

	UFUNCTION(BlueprintPure, Category = "Extended|Placement|Math")
	static FBox GetOrientedBoxBounds(const FEGPlacementOrientedBox& Box) { return Box.GetBounds(); }

	/** Least-squares plane through Points; false for fewer than three non-collinear points. */
	UFUNCTION(BlueprintPure, Category = "Extended|Placement|Math")
	static bool FitPlane(const TArray<FVector>& Points, FVector& Centroid, FVector& Normal);
};
