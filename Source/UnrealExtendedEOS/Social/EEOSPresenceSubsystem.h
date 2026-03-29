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

/**
 * Manages online presence status, rich presence text, and join info.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSPresenceSubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Actions ──────────────────────────────────────────────────────────────

	/** Set the local user's presence status and rich text */
	UFUNCTION(BlueprintCallable, Category = "EOS|Presence")
	void SetPresence(const FString& StatusString, const FString& RichText);

	/** Set presence with a specific online status enum */
	UFUNCTION(BlueprintCallable, Category = "EOS|Presence")
	void SetPresenceWithStatus(EEOSOnlineStatus OnlineStatus, const FString& RichText);

	/** Set a custom presence key-value pair (rich presence data) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Presence")
	void SetPresenceKey(const FString& Key, const FString& Value);

	/** Set the joinable session info for presence (allows friends to join) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Presence")
	void SetJoinInfo(const FString& JoinInfoString);

	/** Clear all presence information (set to offline) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Presence")
	void ClearPresence();

	/** Query another user's presence */
	UFUNCTION(BlueprintCallable, Category = "EOS|Presence")
	void QueryPresence(const FString& UserId);

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

private:

	FEEOSPresenceInfo CachedLocalPresence;
	FString CachedRichText;
};
