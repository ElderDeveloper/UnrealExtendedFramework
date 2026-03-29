// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "EEOSCustomInvitesSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSCustomInviteSent, bool, bSuccess, const FString&, TargetUserId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSCustomInviteReceived, const FString&, SenderId, const FString&, Payload);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSCustomInviteAccepted, const FString&, SenderId, const FString&, Payload);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSCustomInviteRejected, const FString&, SenderId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSRequestToJoinReceived, const FString&, FromUserId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSRequestToJoinResponded, bool, bAccepted, const FString&, FromUserId);

/**
 * Custom game invites with payload data — send, receive, accept, reject.
 * Also supports Request-to-Join (RTJ) flow.
 * Note: Full implementation requires direct EOS SDK calls (EOS_CustomInvites_*).
 * This subsystem provides the Blueprint interface and event framework.
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

	/** Send the configured custom invite to a target user */
	UFUNCTION(BlueprintCallable, Category = "EOS|CustomInvites")
	void SendCustomInvite(const FString& TargetUserId);

	/** Send the custom invite to multiple users */
	UFUNCTION(BlueprintCallable, Category = "EOS|CustomInvites")
	void SendCustomInviteBatch(const TArray<FString>& TargetUserIds);

	/** Accept a received custom invite */
	UFUNCTION(BlueprintCallable, Category = "EOS|CustomInvites")
	void AcceptCustomInvite(const FString& SenderId);

	/** Reject a received custom invite */
	UFUNCTION(BlueprintCallable, Category = "EOS|CustomInvites")
	void RejectCustomInvite(const FString& SenderId);

	// ── Request to Join ──────────────────────────────────────────────────────

	/** Send a request to join another user's game */
	UFUNCTION(BlueprintCallable, Category = "EOS|CustomInvites")
	void SendRequestToJoin(const FString& TargetUserId);

	/** Accept a request to join from another user */
	UFUNCTION(BlueprintCallable, Category = "EOS|CustomInvites")
	void AcceptRequestToJoin(const FString& FromUserId);

	/** Reject a request to join from another user */
	UFUNCTION(BlueprintCallable, Category = "EOS|CustomInvites")
	void RejectRequestToJoin(const FString& FromUserId);

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

	UPROPERTY(BlueprintAssignable, Category = "EOS|CustomInvites")
	FOnEOSCustomInviteSent OnCustomInviteSent;

	UPROPERTY(BlueprintAssignable, Category = "EOS|CustomInvites")
	FOnEOSCustomInviteReceived OnCustomInviteReceived;

	UPROPERTY(BlueprintAssignable, Category = "EOS|CustomInvites")
	FOnEOSCustomInviteAccepted OnCustomInviteAccepted;

	UPROPERTY(BlueprintAssignable, Category = "EOS|CustomInvites")
	FOnEOSCustomInviteRejected OnCustomInviteRejected;

	UPROPERTY(BlueprintAssignable, Category = "EOS|CustomInvites")
	FOnEOSRequestToJoinReceived OnRequestToJoinReceived;

	UPROPERTY(BlueprintAssignable, Category = "EOS|CustomInvites")
	FOnEOSRequestToJoinResponded OnRequestToJoinResponded;

private:

	FString CurrentPayload;

	/** Pending received invites: SenderId → Payload */
	TMap<FString, FString> PendingInvites;

	/** Pending join requests: FromUserId */
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
