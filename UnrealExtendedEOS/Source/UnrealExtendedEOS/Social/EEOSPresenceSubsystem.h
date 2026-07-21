// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "EEOSPresenceSubsystem.generated.h"

UENUM(BlueprintType)
enum class EEOSOnlineStatus : uint8
{
	Online,
	Away,
	DoNotDisturb,
	ExtendedAway,
	Offline
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSPresenceUpdated, const FEEOSPresenceInfo&, PresenceInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSPresenceSet, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSPresenceQueryFailed, const FString&, UserId);

/**
 * Manages online presence status, rich presence text, and join info.
 *
 * Every setter stages a full presence state and submits it; the local caches are committed
 * ONLY when the backend confirms success, so a failed submit never misreports through
 * GetLocalPresence/GetRichPresenceText.
 *
 * Setters return bool: true → the submit started and OnPresenceSet will broadcast exactly
 * once with the backend result; false → pre-flight failure, OnPresenceSet(false) was already
 * broadcast. QueryPresence returns the same, with OnPresenceQueryFailed as its failure signal.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSPresenceSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Set the local user's presence status and rich text. Preserves the last state set via
	 *  SetPresenceWithStatus (does not force Online) and previously-set custom keys. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Presence")
	bool SetPresence(const FString& StatusString, const FString& RichText);

	/** Set presence with a specific online status enum */
	UFUNCTION(BlueprintCallable, Category = "EOS|Presence")
	bool SetPresenceWithStatus(EEOSOnlineStatus OnlineStatus, const FString& RichText);

	/** Set a custom presence key-value pair (rich presence data). Previously-set status text,
	 *  state, and other keys are preserved. Completion arrives on OnPresenceSet. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Presence")
	bool SetPresenceKey(const FString& Key, const FString& Value);

	/** Set the joinable session info for presence (allows friends to join).
	 *  Previously-set status text, state, and other keys are preserved. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Presence")
	bool SetJoinInfo(const FString& JoinInfoString);

	/** Clear all presence information (submit Offline with empty text/keys). On success the
	 *  remembered state resets to Online, so a later SetPresence/SetPresenceKey/SetJoinInfo
	 *  brings the user back online instead of silently re-submitting Offline forever. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Presence")
	bool ClearPresence();

	/** Query another user's presence. Success arrives on OnPresenceUpdated; every failure
	 *  path (pre-flight or backend) broadcasts OnPresenceQueryFailed with the queried id. */
	UFUNCTION(BlueprintCallable, Category = "EOS|Presence")
	bool QueryPresence(const FString& UserId);

	// ── Queries ──────────────────────────────────────────────────────────────

	/** Get the local user's current presence info */
	UFUNCTION(BlueprintPure, Category = "EOS|Presence")
	FEEOSPresenceInfo GetLocalPresence() const;

	/** Check if the local user's presence is set to online */
	UFUNCTION(BlueprintPure, Category = "EOS|Presence")
	bool IsOnline() const;

	/** Get the current rich presence text */
	UFUNCTION(BlueprintPure, Category = "EOS|Presence")
	FString GetRichPresenceText() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "EOS|Presence")
	FOnEOSPresenceUpdated OnPresenceUpdated;

	UPROPERTY(BlueprintAssignable, Category = "EOS|Presence")
	FOnEOSPresenceSet OnPresenceSet;

	/** Terminal failure signal for QueryPresence. Carries the queried user id (the raw input
	 *  for pre-flight failures; the engine-echoed composite for backend failures). */
	UPROPERTY(BlueprintAssignable, Category = "EOS|Presence")
	FOnEOSPresenceQueryFailed OnPresenceQueryFailed;

private:

	/** A full presence state staged for one submit. Committed to the caches only when the
	 *  backend confirms success — failed submits must not poison the local mirror. */
	struct FEEOSPendingPresence
	{
		EEOSOnlineStatus State = EEOSOnlineStatus::Online;
		FString StatusText;
		FString RichText;
		TMap<FString, FString> Properties;
		/** ClearPresence sets this: on success CachedPresenceState resets to Online so the
		 *  next setter does not silently keep the user offline. */
		bool bResetStateToOnlineOnSuccess = false;
	};

	FEEOSPresenceInfo CachedLocalPresence;
	FString CachedRichText;

	/** Last state CONFIRMED by the backend through SetPresenceWithStatus — the other setters
	 *  reuse it instead of stomping Away/DND back to Online. */
	EEOSOnlineStatus CachedPresenceState = EEOSOnlineStatus::Online;

	/** Local mirror of the full CONFIRMED presence status. Every IOnlinePresence::SetPresence
	 *  call replaces the WHOLE backend status (state + rich text + data records), so each
	 *  setter stages this cache plus its change and submits the full merged status — status
	 *  text, JoinInfo, and custom keys coexist instead of clobbering each other. */
	FString CachedStatusText;
	TMap<FString, FString> CachedPresenceProperties;

	/** Stage a copy of the confirmed caches as the starting point for one setter's change. */
	FEEOSPendingPresence StagePendingFromCache() const;

	/** Submit a staged status to the presence interface. Broadcasts OnPresenceSet exactly
	 *  once: immediately on failure to start (returns false), otherwise from the async
	 *  completion (returns true), which also commits the staged values into the caches. */
	bool SubmitPresence(FEEOSPendingPresence&& Pending, const FString& CallerName);
};
