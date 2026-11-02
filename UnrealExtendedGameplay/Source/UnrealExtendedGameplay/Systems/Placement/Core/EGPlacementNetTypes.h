// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementTypes.h"

#include "EGPlacementNetTypes.generated.h"

class UPackageMap;

/**
 * A compact placement request a project puts in its own RPC or command. The host never trusts
 * it: it resolves the spec from Source with its own data, turns the proposal into an intent with
 * ToIntent, evaluates it in Authority scope and applies that result's pose. The ClaimMatch and
 * Revision rules catch a world that changed between aiming and committing.
 *
 * The pose is quantized to 0.01 cm, finer than any grid step and the solvers' tolerance, so
 * solving it again on the host stays idempotent.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementProposal
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	FQuat Rotation = FQuat::Identity;

	/** Uniform. */
	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	float Scale = 1.0f;

	/** The mode that produced it, routed by the project's commit handler. */
	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	FGameplayTag ModeTag;

	/** The project's identity for the item. Never a spec. */
	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	FEGPlacementHandle Source;

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	FEGPlacementClaims Claims;

	/** The component or anchor aimed at, if any. Replicated as an object reference. */
	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	TWeakObjectPtr<UObject> Target;

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	bool bFreePosition = false;

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	bool bFreeRotation = false;

	/** A proposal for an evaluation the requester saw, with its claims and revisions. */
	static FEGPlacementProposal FromEvaluation(const FEGPlacementEvaluation& Evaluation, const FEGPlacementIntent& Intent, FGameplayTag ModeTag, const FEGPlacementHandle& Source);

	/** The intent the host evaluates. Its pose is the proposed pose; the requested spin is already in it. */
	FEGPlacementIntent ToIntent() const;

	FTransform GetPose() const { return FTransform(Rotation, Location, FVector(Scale)); }

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};

template <>
struct TStructOpsTypeTraits<FEGPlacementProposal> : public TStructOpsTypeTraitsBase2<FEGPlacementProposal>
{
	enum
	{
		WithNetSerializer = true,
	};
};
