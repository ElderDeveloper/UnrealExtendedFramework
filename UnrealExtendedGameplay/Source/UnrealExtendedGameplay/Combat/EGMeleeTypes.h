// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "GameplayTagContainer.h"

#include "EGMeleeTypes.generated.h"

class AActor;

/** Where melee hits are detected. Damage is only ever applied where the owner has authority. */
UENUM(BlueprintType)
enum class EEGMeleeDetection : uint8
{
	/** Server and standalone only. */
	AuthorityOnly,
	/**
	 * Also on the owning client, for immediate feedback (hit pause, sounds, effects). Its hits
	 * arrive with bAuthoritative false and never apply damage.
	 */
	AuthorityAndLocalOwner,
	/** Every machine that plays the animation. For cosmetic-only setups. */
	Everywhere
};

/** One traced volume: a sphere at a socket, or a row of spheres between two sockets (a blade). */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGMeleeHitShape
{
	GENERATED_BODY()

	/**
	 * Opened by any window whose tag is this tag or a parent of it: a window tagged
	 * ExtendedGameplay.Melee.Window opens every shape, ...Window.Right only the right-hand ones.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee", meta = (Categories = "ExtendedGameplay.Melee.Window"))
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
	FName StartSocket;

	/** None: a single sphere at StartSocket. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
	FName EndSocket;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee", meta = (ClampMin = "0", Units = "cm"))
	float Radius = 10.0f;

	/** Spheres along StartSocket..EndSocket. Long blades need more so a thin target cannot slip between two. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee", meta = (ClampMin = "1", ClampMax = "16"))
	int32 PointCount = 3;

	/**
	 * Component the sockets belong to, by name, on the owner or an actor attached to it (a weapon).
	 * None uses the mesh that opened the window. UEGMeleeTraceComponent::SetShapeSource overrides it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
	FName SourceComponentName;
};

/** One actor struck by one window. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGMeleeHit
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Melee")
	TObjectPtr<AActor> Target = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Melee")
	FHitResult HitResult;

	UPROPERTY(BlueprintReadOnly, Category = "Melee")
	FGameplayTag WindowTag;

	UPROPERTY(BlueprintReadOnly, Category = "Melee")
	int32 WindowId = INDEX_NONE;

	/** Direction the striking point was moving in, or attacker to target when it was still. */
	UPROPERTY(BlueprintReadOnly, Category = "Melee")
	FVector SwingDirection = FVector::ForwardVector;

	/** From the window (the Melee Window notify's DamageMultiplier). */
	UPROPERTY(BlueprintReadOnly, Category = "Melee")
	float DamageMultiplier = 1.0f;

	/** From the window (the Melee Window notify's AttackTags). */
	UPROPERTY(BlueprintReadOnly, Category = "Melee")
	FGameplayTagContainer AttackTags;

	/** Detected where the owner has authority. Only these apply damage. */
	UPROPERTY(BlueprintReadOnly, Category = "Melee")
	bool bAuthoritative = false;
};

namespace EGMelee
{
	/** Count points evenly from A to B, both ends included; one point is A. */
	UNREALEXTENDEDGAMEPLAY_API void SampleSegment(const FVector& A, const FVector& B, int32 Count, TArray<FVector>& OutPoints);
}
