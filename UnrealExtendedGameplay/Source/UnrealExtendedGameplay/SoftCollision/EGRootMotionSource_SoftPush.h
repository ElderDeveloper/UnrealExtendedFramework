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

template <>
struct TStructOpsTypeTraits<FEGRootMotionSource_SoftPush> : public TStructOpsTypeTraitsBase2<FEGRootMotionSource_SoftPush>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};
