// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CollisionQueryParams.h"
#include "Engine/DataAsset.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementTypes.h"

#include "EGPlacementProfile.generated.h"

struct FOverlapResult;
class UEGPlacementPoseSolver;
class UEGPlacementRule;
class UPhysicalMaterial;

/**
 * One way of placing things: "world", "shelf slot", "portal socket". Holds the pose solver, the
 * ordered rules and the query settings they share. Native code can also build one at runtime
 * with NewObject.
 *
 * Solvers and rules are instanced into the profile and shared by everything that uses it, so they
 * keep no per-call state. Nothing turns on implicitly: a profile without a solver or without rules
 * answers NotConfigured.
 */
UCLASS(BlueprintType)
class UNREALEXTENDEDGAMEPLAY_API UEGPlacementProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Placement")
	TObjectPtr<UEGPlacementPoseSolver> Solver;

	/** Run in order. Any Fail makes the answer Invalid; otherwise any Pending makes it Pending. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Placement")
	TArray<TObjectPtr<UEGPlacementRule>> Rules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	FEGPlacementQuery Query;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	FEGPlacementSurfaceBands SurfaceBands;

	/** Default grid; a spec can override it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	FEGPlacementGrid Grid;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float DefaultMaxSlopeDegrees = 30.0f;

	/** How far from the contact face the support may touch the clearance boxes, in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement", meta = (ClampMin = "0.0"))
	float ContactTolerance = 2.0f;

	/** Physical material to surface tags, for surface rules. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	TMap<TObjectPtr<UPhysicalMaterial>, FGameplayTagContainer> SurfaceTagMap;

	/** Run every rule so UI can list all problems, instead of stopping at the first failure. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placement")
	bool bCollectAllReasons = false;

	const FEGPlacementGrid& GetGrid(const FEGPlacementSpec& Spec) const { return Spec.bOverrideGrid ? Spec.Grid : Grid; }
	float GetMaxSlope(const FEGPlacementSpec& Spec) const { return Spec.bOverrideMaxSlope ? Spec.MaxSlopeDegrees : DefaultMaxSlopeDegrees; }

	/** Collision query parameters shared by the solvers and rules, ignoring the context's actors and components. */
	FCollisionQueryParams MakeQueryParams(const FEGPlacementContext& Context, FName TraceTag, bool bReturnPhysicalMaterial = false) const;

	/** A line trace with the profile's channel or collision profile. */
	bool LineTrace(const FEGPlacementContext& Context, const FVector& Start, const FVector& End, FHitResult& OutHit, FName TraceTag) const;

	/** An overlap test of one oriented box with the profile's channel or collision profile. */
	bool OverlapBox(const FEGPlacementContext& Context, const FEGPlacementOrientedBox& Box, TArray<FOverlapResult>& OutOverlaps, FName TraceTag) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
