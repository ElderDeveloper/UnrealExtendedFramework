// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedGameplay/SoftCollision/EGSoftCollisionMath.h"
#include "UnrealExtendedGameplay/SoftCollision/EGSoftCollisionTypes.h"

class UCharacterMovementComponent;
class UEGSoftCollisionDriver;

/**
 * Prediction bookkeeping for predicted pushes. The component owns one of each; nothing here
 * touches the network itself.
 *
 *  - FEGSoftPushClientQueue: the owning client's requests until the server acknowledges or rejects
 *    them. Requests repeat inside a short redundancy window, the same idea the CMC uses for its own
 *    important moves, because a reliable resend would arrive after the move it belongs to.
 *  - FEGSoftPushServerState: what the server remembers per pushing player: the last sequence it
 *    processed, its rate limit, and the pushes it must remove at a later client move time.
 *  - FEGSoftCollisionSafetyLease: the server's temporary change of two CMC flags while a push runs.
 */

/** One request the client is waiting on. */
struct FEGSoftPushPendingRequest
{
	FEGSoftPushRequest Request;
	double FirstSendTime = 0.0;
	double LastSendTime = 0.0;
	int32 SendCount = 0;
};

struct UNREALEXTENDEDGAMEPLAY_API FEGSoftPushClientQueue
{
	/** Requests beyond this are dropped oldest first; they are long timed out by then. */
	static constexpr int32 MaxPending = 16;

	void Add(const FEGSoftPushRequest& Request, double Now);

	/** Everything up to and including LastAccepted is answered. */
	void Acknowledge(uint16 LastAccepted);

	/** Returns false when Sequence was not pending. */
	bool Remove(uint16 Sequence);

	/**
	 * Requests to send now: every request never sent, then repeats of requests still inside the
	 * redundancy window that were not sent in the last ResendInterval. At most MaxBatch.
	 */
	void CollectBatch(double Now, float RedundancyWindow, float ResendInterval, int32 MaxBatch, TArray<FEGSoftPushRequest>& OutBatch);

	/** Removes requests unanswered for longer than Timeout and returns their sequences. */
	void CollectTimedOut(double Now, float Timeout, TArray<uint16, TInlineAllocator<8>>& OutSequences);

	bool HasPending() const { return !Pending.IsEmpty(); }
	int32 Num() const { return Pending.Num(); }
	void Reset() { Pending.Reset(); }

	const TArray<FEGSoftPushPendingRequest, TInlineAllocator<MaxPending>>& GetPending() const { return Pending; }

private:
	TArray<FEGSoftPushPendingRequest, TInlineAllocator<MaxPending>> Pending;
};

/** A push the server must remove once it has processed the client move ending at ClientTime. */
struct FEGSoftPushScheduledRemoval
{
	const UEGSoftCollisionDriver* Driver = nullptr;
	FEGSoftPushCommand Command;
	int32 Handle = INDEX_NONE;
	float ClientTime = 0.0f;
};

struct UNREALEXTENDEDGAMEPLAY_API FEGSoftPushServerState
{
	uint16 LastProcessedSequence = 0;
	bool bHasProcessed = false;
	EGSoftCollision::FTokenBucket Bucket;
	double LastBumpTime = -1.0e9;

	/** Last client move time seen, to detect timestamp resets. */
	float LastClientTime = 0.0f;
	bool bHasLastClientTime = false;

	TArray<FEGSoftPushScheduledRemoval, TInlineAllocator<4>> Removals;

	/** True when Sequence was already processed (a repeat) or is older than what was processed. */
	bool IsDuplicate(uint16 Sequence) const
	{
		return bHasProcessed && !EGSoftCollision::IsSequenceNewer(Sequence, LastProcessedSequence);
	}

	void MarkProcessed(uint16 Sequence)
	{
		LastProcessedSequence = Sequence;
		bHasProcessed = true;
	}

	void Reset()
	{
		*this = FEGSoftPushServerState();
	}
};

/**
 * FEGSoftCollisionSafetyLease
 * Holds bServerAcceptClientAuthoritativePosition (Level 1) and, for Level 2, also
 * bIgnoreClientMovementErrorChecksAndCorrection, on the server, until a client move time. The
 * previous values are restored on release. The component holds one lease per pawn and extends it
 * for every accepted push, so overlapping pushes count as one window.
 *
 * Level 2 gives the client full authority, so OnServerMove checks every processed move: with both
 * flags on, the server copies the client's position after simulating, so the jump between where the
 * server's own simulation ended and where the next move starts is the client's extra displacement.
 * More than Tolerance plus the push allowance closes the authoritative part of the window.
 */
struct UNREALEXTENDEDGAMEPLAY_API FEGSoftCollisionSafetyLease
{
	bool IsActive() const { return bActive; }
	EEGSoftCollisionSafetyLevel GetLevel() const { return Level; }

	void Acquire(UCharacterMovementComponent& Movement, EEGSoftCollisionSafetyLevel InLevel, float UntilClientTime);

	void Release(UCharacterMovementComponent& Movement);

	/**
	 * Call after every server move of the pawn. Releases the lease once ClientTime passes its end.
	 * Returns false when the Level 2 check failed and the window fell back to Level 1.
	 */
	bool OnServerMove(UCharacterMovementComponent& Movement, float ClientTime, const FVector& MoveStartLocation,
		const FVector& MoveEndLocation, float DeltaSeconds, float AllowedExtraSpeed, float Tolerance);

	/** A client timestamp reset invalidates the end time. */
	void OnTimeStampReset(UCharacterMovementComponent& Movement) { Release(Movement); }

private:
	bool bActive = false;
	bool bSavedAccept = false;
	bool bSavedIgnore = false;
	bool bHasLastSimulated = false;
	EEGSoftCollisionSafetyLevel Level = EEGSoftCollisionSafetyLevel::KeysOnly;
	float UntilClientTime = 0.0f;
	FVector LastSimulatedLocation = FVector::ZeroVector;
};
