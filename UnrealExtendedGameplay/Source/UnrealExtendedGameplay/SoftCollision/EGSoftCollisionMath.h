// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGSoftCollisionTypes.h"

/**
 * The math of Soft Collision. Plain C++ with no UObject in sight, so worker threads can run it on
 * the frame snapshot and the tests can build a scene in a line.
 *
 * Directions always point from the other pawn towards the pawn being pushed: a pawn is pushed
 * along the direction, away from what it overlaps.
 */
namespace EGSoftCollision
{
	/** Overlap between two bodies, seen from the pawn being pushed. */
	struct FContact
	{
		/** Unit direction, from the other pawn towards this one. */
		FVector2D Direction = FVector2D::ZeroVector;
		/** How deep the shapes overlap, cm. */
		float Overlap = 0.0f;
		/** Overlap divided by both shapes' extent along Direction: 0 touching, 1 centres meet. */
		float Penetration = 0.0f;
	};

	/** Whether Self and Other overlap, and how. Heights must overlap too. */
	UNREALEXTENDEDGAMEPLAY_API bool ComputeContact(const FEGSoftBody& Self, const FEGSoftBody& Other, FContact& OutContact);

	/** Response curve value for a penetration in 0..1, before the deep-overlap boost. */
	UNREALEXTENDEDGAMEPLAY_API float EvaluateResponse(EEGSoftPushResponse Response, float Penetration);

	/** Response with the deep-overlap ramp applied. */
	UNREALEXTENDEDGAMEPLAY_API float EvaluatePenetrationFactor(const FEGSoftCollisionTuning& Tuning, float Penetration);

	/**
	 * Fraction of the separation this pawn takes, 0..1.
	 * M = weight * max(speed towards the other pawn, idle floor); Other's M is also scaled by its push
	 * power. share = M_other / (M_self + M_other). Two idle equal pawns split it evenly; an idle pawn
	 * hit by a runner takes almost all of it.
	 */
	UNREALEXTENDEDGAMEPLAY_API float ComputeMomentumShare(float SelfWeight, float SelfApproachSpeed,
		float OtherWeight, float OtherPushPower, float OtherApproachSpeed, float IdleFloorSpeed);

	/** Strength multiplier of a push at Alpha (0..1) of its duration. */
	UNREALEXTENDEDGAMEPLAY_API float EvaluateDecay(EEGSoftPushDecay Decay, float Alpha);

	/** Average decay multiplier between two alphas, so partial ticks integrate the same on every machine. */
	UNREALEXTENDEDGAMEPLAY_API float EvaluateDecayAverage(EEGSoftPushDecay Decay, float AlphaStart, float AlphaEnd);

	/**
	 * Sums every neighbour's contribution to one pawn's push. Add is called once per overlapping
	 * neighbour, Finish once at the end.
	 */
	struct UNREALEXTENDEDGAMEPLAY_API FPushAccumulator
	{
		/** Returns false when the pair does not interact (filter or no overlap). */
		bool AddNeighbour(const FEGSoftCollisionTuning& Tuning, const FEGSoftBody& Self, const FEGSoftBody& Other);

		/** Adds an already computed contact. */
		void AddContact(const FEGSoftCollisionTuning& Tuning, const FEGSoftBody& Self, const FEGSoftBody& Other, const FContact& Contact);

		FEGSoftPushResult Finish(const FEGSoftCollisionTuning& Tuning) const;

		FVector2D SeparationSum = FVector2D::ZeroVector;
		FVector2D BumpDirection = FVector2D::ZeroVector;
		float BumpRelativeSpeed = 0.0f;
		float BumpShare = 0.0f;
		float BumpScore = 0.0f;
		float MaxPenetration = 0.0f;
		int32 NeighbourCount = 0;
		int32 NeighbourTests = 0;
	};

	/** Whether Other may push Self under both filters. */
	inline bool CanPush(const FEGSoftBody& Other, const FEGSoftBody& Self)
	{
		return Other.bEnabled && Self.bEnabled && Other.bCanPush && Self.bCanBePushed
			&& (Self.PushedByMask & (1u << (Other.Layer & 31u))) != 0;
	}

	/** Strength step of the quantized push, cm/s. */
	inline constexpr float QuantizedStrengthStep = 0.1f;

	/** Largest push speed a request can carry, cm/s. */
	inline constexpr float MaxQuantizedStrength = 65535.0f * QuantizedStrengthStep;

	UNREALEXTENDEDGAMEPLAY_API FEGSoftPushQuantized Quantize(const FVector2D& Velocity);
	UNREALEXTENDEDGAMEPLAY_API FVector2D Dequantize(const FEGSoftPushQuantized& Quantized);
	UNREALEXTENDEDGAMEPLAY_API float DequantizeStrength(uint16 Strength);

	/** Angle between two push velocities, degrees. 180 when either is zero. */
	UNREALEXTENDEDGAMEPLAY_API float AngleBetweenDegrees(const FVector2D& A, const FVector2D& B);

	/** Wrap-aware: true when A was issued after B. */
	inline bool IsSequenceNewer(uint16 A, uint16 B)
	{
		return static_cast<int16>(static_cast<uint16>(A - B)) > 0;
	}

	/** Rate limiter. Times are seconds on any monotonic clock. */
	struct UNREALEXTENDEDGAMEPLAY_API FTokenBucket
	{
		float Tokens = -1.0f;
		double LastTime = 0.0;

		bool TryConsume(double Now, float RatePerSecond, float Burst);
	};
}
