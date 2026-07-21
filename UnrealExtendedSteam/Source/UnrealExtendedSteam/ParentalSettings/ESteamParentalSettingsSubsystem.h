// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESteamSubsystem.h"
#include "ESteamParentalSettingsSubsystem.generated.h"

/** Steam features that Family View can block (maps EParentalFeature). */
UENUM(BlueprintType)
enum class EESteamParentalFeature : uint8
{
	Store,
	Community,
	Profile,
	Friends,
	News,
	Trading,
	Settings,
	Console,
	Browser,
	ParentalSetup,
	Library,
	Test,
	/** Restricts store/community for site-license (e.g. internet cafe) accounts. */
	SiteLicense,
	/** Steam kiosk mode (deprecated by Valve, kept for completeness). */
	KioskMode,
	/** Feature is always blocked regardless of other settings. */
	BlockAlways,
	/** Desktop / non-Steam access. */
	Desktop
};

/** Fired when the parental settings change (e.g. Family View gets locked or unlocked). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSteamParentalSettingsChanged);

/**
 * Wraps ISteamParentalSettings: Family View state queries.
 * All queries return false when Steam is unavailable (treat as "not blocked").
 */
UCLASS()
class UNREALEXTENDEDSTEAM_API UESteamParentalSettingsSubsystem : public UESteamSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** True when Family View is set up on this account. */
	UFUNCTION(BlueprintPure, Category = "Steam|ParentalSettings")
	bool IsParentalLockEnabled() const;

	/** True when Family View is currently locked (PIN not entered). */
	UFUNCTION(BlueprintPure, Category = "Steam|ParentalSettings")
	bool IsParentalLockLocked() const;

	/** True when the given app is blocked right now (in the block list and the lock is engaged). */
	UFUNCTION(BlueprintPure, Category = "Steam|ParentalSettings")
	bool IsAppBlocked(int32 AppId) const;

	/** True when the given app is in the Family View block list (regardless of lock state). */
	UFUNCTION(BlueprintPure, Category = "Steam|ParentalSettings")
	bool IsAppInBlockList(int32 AppId) const;

	/** True when the given feature is blocked right now. */
	UFUNCTION(BlueprintPure, Category = "Steam|ParentalSettings")
	bool IsFeatureBlocked(EESteamParentalFeature Feature) const;

	/** True when the given feature is in the Family View block list (regardless of lock state). */
	UFUNCTION(BlueprintPure, Category = "Steam|ParentalSettings")
	bool IsFeatureInBlockList(EESteamParentalFeature Feature) const;

	UPROPERTY(BlueprintAssignable, Category = "Steam|ParentalSettings")
	FOnSteamParentalSettingsChanged OnParentalSettingsChanged;

protected:
	virtual void HandleSteamClientInitialized() override;
	virtual void HandleSteamClientShutdown() override;

private:
	friend class FESteamParentalSettingsCallbacks;
	TSharedPtr<class FESteamParentalSettingsCallbacks> Callbacks;
};
