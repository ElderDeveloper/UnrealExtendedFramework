// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/RootMotionSource.h"
#include "EGSoftCollisionTypes.h"

#include "EGRootMotionSource_SoftPush.generated.h"

/**
 * FEGRootMotionSource_SoftPush
 * The predicted push: an additive horizontal velocity for a short duration, applied to a
 * CharacterMovementComponent on the owning client and, from the request, on the server.
 *
 * Built only from quantized integers (direction, strength, sequence) plus the profile's duration
 * and decay, so both machines build the same source. Matches compares the sequence, as the engine
 * suggests for sources that need an exact pairing during corrections.
 *
 * Every replicated field is a UPROPERTY. The custom NetSerialize keeps the classic path small;
 * Iris replicates structs with a custom NetSerialize from their reflected properties, so this
 * replicates there too.
 *
 * The decay is integrated over each tick (average multiplier between the tick's start and end),
 * so a tick split differently on two machines moves the pawn the same distance.
 */
USTRUCT()
struct UNREALEXTENDEDGAMEPLAY_API FEGRootMotionSource_SoftPush : public FRootMotionSource
{
	GENERATED_BODY()

	FEGRootMotionSource_SoftPush();
	virtual ~FEGRootMotionSource_SoftPush() override = default;

	UPROPERTY()
	uint16 Sequence = 0;

	UPROPERTY()
	FEGSoftPushQuantized Push;

	UPROPERTY()
	EEGSoftPushDecay Decay = EEGSoftPushDecay::Constant;

	/**
	 * Contributes nothing while the Character is moved by gameplay root motion (a root-motion montage
	 * or another root motion source). Decided inside the move's simulation, so the owning client and
	 * the server agree on exactly which moves it yields on.
	 */
	UPROPERTY()
	bool bYieldToGameplayRootMotion = false;

	/** Horizontal push velocity, cm/s, from the quantized values. */
	FVector GetPushVelocity() const;

	virtual FRootMotionSource* Clone() const override;
	virtual bool Matches(const FRootMotionSource* Other) const override;
	virtual bool MatchesAndHasSameState(const FRootMotionSource* Other) const override;
	virtual bool UpdateStateFrom(const FRootMotionSource* SourceToTakeStateFrom, bool bMarkForSimulatedCatchup = false) override;
	virtual void PrepareRootMotion(float SimulationTime, float MovementTickTime, const ACharacter& Character, const UCharacterMovementComponent& MoveComponent) override;
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
};

namespace EGSoftCollision
{
	/** Whether a root motion source is a soft push. */
	UNREALEXTENDEDGAMEPLAY_API bool IsSoftPushSource(const FRootMotionSource& Source);

	/**
	 * Active or pending root motion sources other than soft pushes. Use it instead of
	 * CurrentRootMotion.HasActiveRootMotionSources() wherever root motion means "gameplay is moving
	 * this character" (dashes, knockbacks): a pawn being jostled in a crowd is not.
	 */
	UNREALEXTENDEDGAMEPLAY_API bool HasGameplayRootMotionSources(const UCharacterMovementComponent& Movement);

	/** Root motion from a montage or a gameplay source; soft pushes excluded. Use it instead of ACharacter::HasAnyRootMotion(). */
	UNREALEXTENDEDGAMEPLAY_API bool HasGameplayRootMotion(const ACharacter& Character);

	/**
	 * A networked root-motion montage or a gameplay root motion source; soft pushes excluded. Unlike
	 * HasGameplayRootMotion, root-motion locomotion (Root Motion From Everything) does not count, so
	 * characters that walk with root motion can still be pushed.
	 */
	UNREALEXTENDEDGAMEPLAY_API bool IsDrivenByGameplayRootMotion(const ACharacter& Character);
}

template <>
struct TStructOpsTypeTraits<FEGRootMotionSource_SoftPush> : public TStructOpsTypeTraitsBase2<FEGRootMotionSource_SoftPush>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};
