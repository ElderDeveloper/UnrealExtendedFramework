// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Shared/EEOSNativeOperation.h"
#include "EEOSSearchCoordinator.generated.h"

/**
 * Serializes session/lobby searches across the Sessions, Lobby and Matchmaking subsystems.
 *
 * The 5.8 EOS OSS cannot run concurrent searches: IOnlineSession::FindSessions guards only on
 * the INCOMING search object (OnlineSessionEOS.cpp:2275) and overwrites CurrentSessionSearch/
 * CurrentSearchHandle, so a second search issued while one is in flight hijacks the first —
 * the completing EOS callback then marks the OTHER search's object Done (:4773 writes the
 * member, not its own captured object) and the hijacked search never completes.
 *
 * This subsystem is the single gate: every subsystem must TryAcquire the slot before calling
 * IOnlineSession::FindSessions and Release it on every terminal path (completion handler,
 * synchronous FindSessions failure, cancellation, Deinitialize). Plugin leases serialize participating callers; native completion delegates still cannot
 * identify an external caller. Consume the held native search completion, including the engine's
 * zero-result path, which fires the delegate without ever setting SearchState
 * (OnlineSessionEOS.cpp:2675-2679).
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSSearchCoordinator : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	/** Try to acquire the single search slot for OwnerTag. Fails (with a warning) while any
	 *  owner — including OwnerTag itself — holds the slot: each search must be released
	 *  before the next may start. */
	bool TryAcquire(FName OwnerTag);
	bool TryAcquire(FName OwnerTag, const IOnlineSessionPtr& Sessions, UObject* OperationOwner = nullptr, FName Operation = NAME_None);
	/** Detach a canceled caller while retaining native ownership until its callback retires. */
	void Retire(FName OwnerTag);
	virtual void Deinitialize() override;

	/** Release the slot. Only the current owner's tag releases it; a mismatched tag is logged
	 *  and ignored. Releasing an already-free slot is a silent no-op, so terminal paths may
	 *  release unconditionally. */
	void Release(FName OwnerTag);

	/** The tag of the owner currently holding the search slot, or NAME_None when free. */
	FName GetCurrentOwner() const { return CurrentOwner; }
	int64 GetRequestId() const { return SearchLease.GetRequestId(); }

private:

	FName CurrentOwner = NAME_None;
	IOnlineSessionPtr HeldSessions;
	FEEOSNativeOperationLease SearchLease;
};
