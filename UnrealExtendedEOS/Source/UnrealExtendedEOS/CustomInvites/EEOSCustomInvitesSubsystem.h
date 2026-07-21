// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "EEOSCustomInvitesSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSCustomInviteSent, bool, bSuccess, const FString&, TargetUserId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSCustomInviteReceived, const FString&, SenderId, const FString&, Payload);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSCustomInviteAccepted, const FString&, SenderId, const FString&, Payload);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSCustomInviteRejected, const FString&, SenderId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSRequestToJoinSent, bool, bSuccess, const FString&, TargetUserId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSRequestToJoinReceived, const FString&, FromUserId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSRequestToJoinResponded, bool, bAccepted, const FString&, FromUserId);

/**
 * Custom game invites with payload data — send, receive, accept, reject.
 * Also supports the Request-to-Join (RTJ) flow.
 *
 * Implemented via raw EOS SDK calls (EOS_CustomInvites_*). Received invites and join
 * requests are recorded from the SDK receive notifications, so Accept/Reject only work
 * for invites/requests this subsystem has actually been notified about. Without the EOS
 * SDK (or before login) every send entry point completes with a failure broadcast —
 * success is never faked.
 *
 * User id parameters accept both the composite "<EpicAccountId>|<ProductUserId>" form
 * (what every other subsystem hands out) and a bare Product User ID — the PUID half is
 * extracted before any SDK call; ids without a PUID half fail observably.
 *
 * Async entry points return bool: true → the operation started and its completion delegate
 * will broadcast exactly once; false → pre-flight failure (a failure broadcast was already
 * sent where the operation has a send/respond delegate).
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSCustomInvitesSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Invite Management ────────────────────────────────────────────────────

	/** Set the custom invite payload (game data like map, mode, etc.) */
	UFUNCTION(BlueprintCallable, Category = "EOS|CustomInvites")
	void SetCustomInvitePayload(const FString& Payload);

	/** Send the configured custom invite to a target user. Returns true when the send
	 *  started (completion arrives on OnCustomInviteSent). */
	UFUNCTION(BlueprintCallable, Category = "EOS|CustomInvites")
	bool SendCustomInvite(const FString& TargetUserId);

	/** Send the custom invite to multiple users (one OnCustomInviteSent per target).
	 *  Returns true only when every send started. */
	UFUNCTION(BlueprintCallable, Category = "EOS|CustomInvites")
	bool SendCustomInviteBatch(const TArray<FString>& TargetUserIds);

	/** Accept a received custom invite. Finalizes it with the SDK and evicts it from the
	 *  pending set (a second accept for the same invite returns false without re-finalizing).
	 *  Returns true when a pending invite was accepted; false (log only, no broadcast) when
	 *  no pending invite from that sender exists. */
	UFUNCTION(BlueprintCallable, Category = "EOS|CustomInvites")
	bool AcceptCustomInvite(const FString& SenderId);

	/** Reject a received custom invite. Always broadcasts OnCustomInviteRejected (the local
	 *  decision stands even without the SDK); returns true when a recorded pending invite was
	 *  actually finalized-rejected with the SDK. */
	UFUNCTION(BlueprintCallable, Category = "EOS|CustomInvites")
	bool RejectCustomInvite(const FString& SenderId);

	// ── Request to Join ──────────────────────────────────────────────────────

	/** Send a request to join another user's game. Returns true when the send started
	 *  (completion arrives on OnRequestToJoinSent). */
	UFUNCTION(BlueprintCallable, Category = "EOS|CustomInvites")
	bool SendRequestToJoin(const FString& TargetUserId);

	/** Accept a request to join from another user. Returns true when the accept started
	 *  (completion arrives on OnRequestToJoinResponded). */
	UFUNCTION(BlueprintCallable, Category = "EOS|CustomInvites")
	bool AcceptRequestToJoin(const FString& FromUserId);

	/** Reject a request to join from another user. Returns true when the reject started
	 *  (completion arrives on OnRequestToJoinResponded). */
	UFUNCTION(BlueprintCallable, Category = "EOS|CustomInvites")
	bool RejectRequestToJoin(const FString& FromUserId);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get the current invite payload */
	UFUNCTION(BlueprintPure, Category = "EOS|CustomInvites")
	FString GetCurrentPayload() const;

	/** Check if there are pending invites */
	UFUNCTION(BlueprintPure, Category = "EOS|CustomInvites")
	bool HasPendingInvites() const;

	/** Get pending invite count */
	UFUNCTION(BlueprintPure, Category = "EOS|CustomInvites")
	int32 GetPendingInviteCount() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	/** Completion of SendCustomInvite / SendCustomInviteBatch (one broadcast per target). */
	UPROPERTY(BlueprintAssignable, Category = "EOS|CustomInvites")
	FOnEOSCustomInviteSent OnCustomInviteSent;

	UPROPERTY(BlueprintAssignable, Category = "EOS|CustomInvites")
	FOnEOSCustomInviteReceived OnCustomInviteReceived;

	UPROPERTY(BlueprintAssignable, Category = "EOS|CustomInvites")
	FOnEOSCustomInviteAccepted OnCustomInviteAccepted;

	UPROPERTY(BlueprintAssignable, Category = "EOS|CustomInvites")
	FOnEOSCustomInviteRejected OnCustomInviteRejected;

	/** Completion of SendRequestToJoin (separate from invite sends). */
	UPROPERTY(BlueprintAssignable, Category = "EOS|CustomInvites")
	FOnEOSRequestToJoinSent OnRequestToJoinSent;

	UPROPERTY(BlueprintAssignable, Category = "EOS|CustomInvites")
	FOnEOSRequestToJoinReceived OnRequestToJoinReceived;

	UPROPERTY(BlueprintAssignable, Category = "EOS|CustomInvites")
	FOnEOSRequestToJoinResponded OnRequestToJoinResponded;

	// ── Internal (SDK notification thunks — not for game code) ──────────────

	/** Records a received invite as pending (finalizing a superseded older invite from the
	 *  same sender first), then broadcasts OnCustomInviteReceived. Game thread only. */
	void HandleCustomInviteReceived(const FString& SenderId, const FString& CustomInviteId, const FString& Payload);

	/** Overlay accept: finalizes the invite with the SDK (required by eos_custominvites.h),
	 *  evicts the pending entry, then broadcasts OnCustomInviteAccepted. Game thread only. */
	void HandleCustomInviteAcceptedOverlay(const FString& SenderId, const FString& CustomInviteId, const FString& Payload);

	/** Overlay reject: evicts the pending entry (the SDK auto-finalizes overlay rejects),
	 *  then broadcasts OnCustomInviteRejected. Game thread only. */
	void HandleCustomInviteRejectedOverlay(const FString& SenderId);

	/** Records a received join request as pending, then broadcasts OnRequestToJoinReceived. Game thread only. */
	void HandleRequestToJoinReceived(const FString& FromUserId);

private:

	FString CurrentPayload;

	/** A received custom invite pending local accept/reject. */
	struct FEEOSPendingCustomInvite
	{
		FString Payload;
		/** SDK-issued invite id from the receive notification — required by EOS_CustomInvites_FinalizeInvite. */
		FString CustomInviteId;
	};

	/** Pending received invites: bare sender PUID → invite data (populated by the receive
	 *  notification; entry-point sender ids are normalized to the PUID half before lookup). */
	TMap<FString, FEEOSPendingCustomInvite> PendingInvites;

	/** Pending join requests: FromUserId (populated by the RTJ receive notification). */
	TArray<FString> PendingJoinRequests;

#if WITH_EOS_SDK
	void RegisterNotificationCallbacks();
	void UnregisterNotificationCallbacks();

	uint64 NotifyCustomInviteReceivedId = 0;
	uint64 NotifyCustomInviteAcceptedId = 0;
	uint64 NotifyCustomInviteRejectedId = 0;
	uint64 NotifyRequestToJoinReceivedId = 0;
#endif
};
