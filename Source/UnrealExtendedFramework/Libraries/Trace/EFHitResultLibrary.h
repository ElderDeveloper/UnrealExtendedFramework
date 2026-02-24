// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EFHitResultLibrary.generated.h"

/**
 * Blueprint function library for extracting data from FHitResult structs.
 * Provides compact Blueprint nodes for common hit result queries.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFHitResultLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Returns the world-space location of the hit. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Hit Location", CompactNodeTitle = "Location"), Category="UEExtended|Hit Result")
	static FVector GetHitLocationFromHitResult(const FHitResult& HitResult);

	/** Returns the trace end point (the furthest point in the attempted sweep). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Trace End", CompactNodeTitle = "TraceEnd"), Category="UEExtended|Hit Result")
	static FVector GetTraceEndFromHitResult(const FHitResult& HitResult);

	/** Returns the trace start point. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Trace Start", CompactNodeTitle = "TraceStart"), Category="UEExtended|Hit Result")
	static FVector GetTraceStartFromHitResult(const FHitResult& HitResult);

	/** Returns the actor that was hit, or nullptr. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Hit Actor", CompactNodeTitle = "HitActor"), Category="UEExtended|Hit Result")
	static AActor* GetHitActorFromHitResult(const FHitResult& HitResult);

	/** Returns the component that was hit, or nullptr. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Hit Component", CompactNodeTitle = "HitComponent"), Category="UEExtended|Hit Result")
	static USceneComponent* GetHitComponentFromHitResult(const FHitResult& HitResult);

	/** Returns the distance from the trace start to the hit location. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Hit Distance", CompactNodeTitle = "Distance"), Category="UEExtended|Hit Result")
	static float GetHitDistance(const FHitResult& HitResult);

	/** Returns the surface normal at the impact point (outward from the hit object). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Impact Normal", CompactNodeTitle = "ImpactNormal"), Category="UEExtended|Hit Result")
	static FVector GetHitImpactNormal(const FHitResult& HitResult);

	/** Returns the exact impact point in world space. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Impact Point", CompactNodeTitle = "ImpactPoint"), Category="UEExtended|Hit Result")
	static FVector GetHitImpactPoint(const FHitResult& HitResult);

	/** Returns the swept shape's normal at the hit (equal to ImpactNormal for line traces). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Hit Normal", CompactNodeTitle = "Normal"), Category="UEExtended|Hit Result")
	static FVector GetHitNormal(const FHitResult& HitResult);

	/** Returns true if the hit was a blocking hit. */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Is Blocking Hit", CompactNodeTitle = "Blocking?"), Category="UEExtended|Hit Result")
	static bool IsBlockingHit(const FHitResult& HitResult);

	/** Returns the name of the bone hit (for skeletal meshes). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Hit Bone Name", CompactNodeTitle = "BoneName"), Category="UEExtended|Hit Result")
	static FName GetHitBoneName(const FHitResult& HitResult);

	/** Returns the physical material at the hit point (requires bReturnPhysicalMaterial). */
	UFUNCTION(BlueprintPure, meta=(DisplayName = "Get Hit Phys Material", CompactNodeTitle = "PhysMat"), Category="UEExtended|Hit Result")
	static UPhysicalMaterial* GetHitPhysMaterial(const FHitResult& HitResult);
};
