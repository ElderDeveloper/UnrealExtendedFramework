// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGSoftCollisionPrediction.h"

#include "GameFramework/CharacterMovementComponent.h"

// =====================================================================
// Client queue
// =====================================================================

void FEGSoftPushClientQueue::Add(const FEGSoftPushRequest& Request, double Now)
{
	if (Pending.Num() >= MaxPending)
	{
		Pending.RemoveAt(0, 1, EAllowShrinking::No);
	}

	FEGSoftPushPendingRequest& Entry = Pending.AddDefaulted_GetRef();
	Entry.Request = Request;
	Entry.FirstSendTime = Now;
	Entry.LastSendTime = Now;
	Entry.SendCount = 0;
}

void FEGSoftPushClientQueue::Acknowledge(uint16 LastAccepted)
{
	Pending.RemoveAll([LastAccepted](const FEGSoftPushPendingRequest& Entry)
	{
		return !EGSoftCollision::IsSequenceNewer(Entry.Request.Sequence, LastAccepted);
	});
}

bool FEGSoftPushClientQueue::Remove(uint16 Sequence)
{
	return Pending.RemoveAll([Sequence](const FEGSoftPushPendingRequest& Entry)
	{
		return Entry.Request.Sequence == Sequence;
	}) > 0;
}

void FEGSoftPushClientQueue::CollectBatch(double Now, float RedundancyWindow, float ResendInterval, int32 MaxBatch, TArray<FEGSoftPushRequest>& OutBatch)
{
	OutBatch.Reset();
	const int32 Limit = FMath::Max(MaxBatch, 1);

	// New requests first, newest first: the newest push is the one the next move needs.
	for (int32 Index = Pending.Num() - 1; Index >= 0 && OutBatch.Num() < Limit; --Index)
	{
		FEGSoftPushPendingRequest& Entry = Pending[Index];
		if (Entry.SendCount == 0)
		{
			OutBatch.Add(Entry.Request);
			Entry.LastSendTime = Now;
			++Entry.SendCount;
		}
	}

	// Then repeats of unanswered requests still worth repeating.
	for (int32 Index = Pending.Num() - 1; Index >= 0 && OutBatch.Num() < Limit; --Index)
	{
		FEGSoftPushPendingRequest& Entry = Pending[Index];
		if (Entry.SendCount > 0 && Entry.LastSendTime < Now
			&& Now - Entry.FirstSendTime <= RedundancyWindow
			&& Now - Entry.LastSendTime >= ResendInterval)
		{
			OutBatch.Add(Entry.Request);
			Entry.LastSendTime = Now;
			++Entry.SendCount;
		}
	}
}

void FEGSoftPushClientQueue::CollectTimedOut(double Now, float Timeout, TArray<uint16, TInlineAllocator<8>>& OutSequences)
{
	OutSequences.Reset();
	for (int32 Index = Pending.Num() - 1; Index >= 0; --Index)
	{
		if (Now - Pending[Index].FirstSendTime > Timeout)
		{
			OutSequences.Add(Pending[Index].Request.Sequence);
			Pending.RemoveAt(Index, 1, EAllowShrinking::No);
		}
	}
}

// =====================================================================
// Safety lease
// =====================================================================

void FEGSoftCollisionSafetyLease::Acquire(UCharacterMovementComponent& Movement, EEGSoftCollisionSafetyLevel InLevel, float InUntilClientTime)
{
	if (InLevel == EEGSoftCollisionSafetyLevel::KeysOnly)
	{
		return;
	}

	if (!bActive)
	{
		bSavedAccept = !!Movement.bServerAcceptClientAuthoritativePosition;
		bSavedIgnore = !!Movement.bIgnoreClientMovementErrorChecksAndCorrection;
		bActive = true;
		bHasLastSimulated = false;
		Level = InLevel;
		UntilClientTime = InUntilClientTime;
	}
	else
	{
		// Never raise a window that fell back from Level 2 until it closes.
		Level = static_cast<uint8>(InLevel) < static_cast<uint8>(Level) ? InLevel : Level;
		UntilClientTime = FMath::Max(UntilClientTime, InUntilClientTime);
	}

	Movement.bServerAcceptClientAuthoritativePosition = true;
	if (Level == EEGSoftCollisionSafetyLevel::ClientAuthoritativeWindow)
	{
		Movement.bIgnoreClientMovementErrorChecksAndCorrection = true;
	}
}

void FEGSoftCollisionSafetyLease::Release(UCharacterMovementComponent& Movement)
{
	if (!bActive)
	{
		return;
	}

	Movement.bServerAcceptClientAuthoritativePosition = bSavedAccept;
	Movement.bIgnoreClientMovementErrorChecksAndCorrection = bSavedIgnore;
	bActive = false;
	bHasLastSimulated = false;
	Level = EEGSoftCollisionSafetyLevel::KeysOnly;
}

bool FEGSoftCollisionSafetyLease::OnServerMove(UCharacterMovementComponent& Movement, float ClientTime, const FVector& MoveStartLocation,
	const FVector& MoveEndLocation, float DeltaSeconds, float AllowedExtraSpeed, float Tolerance)
{
	if (!bActive)
	{
		return true;
	}

	bool bPassed = true;
	if (Level == EEGSoftCollisionSafetyLevel::ClientAuthoritativeWindow && bHasLastSimulated)
	{
		// Horizontal only: gravity and steps are simulated identically on both machines.
		const FVector Jump = MoveStartLocation - LastSimulatedLocation;
		const double Drift = FVector2D(Jump.X, Jump.Y).Size();
		const double Allowed = static_cast<double>(Tolerance) + static_cast<double>(AllowedExtraSpeed) * FMath::Max(DeltaSeconds, 0.0f);
		if (Drift > Allowed)
		{
			Level = EEGSoftCollisionSafetyLevel::AcceptClientPosition;
			Movement.bIgnoreClientMovementErrorChecksAndCorrection = bSavedIgnore;
			bPassed = false;
		}
	}

	LastSimulatedLocation = MoveEndLocation;
	bHasLastSimulated = true;

	if (ClientTime > UntilClientTime)
	{
		Release(Movement);
	}
	return bPassed;
}
