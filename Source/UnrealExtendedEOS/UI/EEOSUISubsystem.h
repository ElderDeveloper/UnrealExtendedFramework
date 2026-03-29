// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/EEOSSubsystem.h"
#include "EEOSUISubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSOverlayStateChanged, bool, bIsVisible);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSProfileClosed, bool, bSuccess);

/**
 * Manages the EOS Social Overlay — friends list, player profiles,
 * invite UI, achievements UI, leaderboard UI, store, and web views.
 * Uses IOnlineExternalUI.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSUISubsystem : public UEEOSSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Friends Overlay ──────────────────────────────────────────────────────

	/** Show the platform friends list overlay */
	UFUNCTION(BlueprintCallable, Category = "EOS|UI")
	bool ShowFriendsUI();

	/** Show the invite UI for the current session */
	UFUNCTION(BlueprintCallable, Category = "EOS|UI")
	bool ShowInviteUI(const FString& SessionName = TEXT("GameSession"));

	// ── Player Profile ───────────────────────────────────────────────────────

	/** Show a player's profile card */
	UFUNCTION(BlueprintCallable, Category = "EOS|UI")
	bool ShowProfileUI(const FString& TargetUserId);

	// ── Achievements / Leaderboards ──────────────────────────────────────────

	/** Show the platform achievements overlay */
	UFUNCTION(BlueprintCallable, Category = "EOS|UI")
	bool ShowAchievementsUI();

	/** Show a specific leaderboard */
	UFUNCTION(BlueprintCallable, Category = "EOS|UI")
	bool ShowLeaderboardUI(const FString& LeaderboardName);

	// ── Store ────────────────────────────────────────────────────────────────

	/** Show the platform store overlay */
	UFUNCTION(BlueprintCallable, Category = "EOS|UI")
	bool ShowStoreUI(const FString& ProductId = TEXT(""));

	// ── Web View ─────────────────────────────────────────────────────────────

	/** Open a web URL in the overlay browser */
	UFUNCTION(BlueprintCallable, Category = "EOS|UI")
	bool ShowWebURL(const FString& URL);

	/** Close the currently open web view */
	UFUNCTION(BlueprintCallable, Category = "EOS|UI")
	bool CloseWebURL();

	// ── Login UI ─────────────────────────────────────────────────────────────

	/** Show the platform login UI */
	UFUNCTION(BlueprintCallable, Category = "EOS|UI")
	bool ShowLoginUI();

	// ── Account Upgrade ──────────────────────────────────────────────────────

	/** Show the account upgrade UI (e.g., PS+, Xbox Gold) */
	UFUNCTION(BlueprintCallable, Category = "EOS|UI")
	bool ShowAccountUpgradeUI();

	// ── Overlay State ────────────────────────────────────────────────────────

	/** Check if the overlay is currently visible */
	UFUNCTION(BlueprintPure, Category = "EOS|UI")
	bool IsOverlayVisible() const;

	// ── Delegates ────────────────────────────────────────────────────────────

	UPROPERTY(BlueprintAssignable, Category = "EOS|UI")
	FOnEOSOverlayStateChanged OnOverlayStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "EOS|UI")
	FOnEOSProfileClosed OnProfileClosed;

private:

	bool bOverlayVisible = false;
	FDelegateHandle ExternalUIChangeHandle;
};
