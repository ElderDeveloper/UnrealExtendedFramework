// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESteamSubsystem.h"
#include "Shared/ESteamTypes.h"
#include "ESteamAppsSubsystem.generated.h"

/** Metadata of one piece of DLC. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDSTEAM_API FESteamDlcData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Steam|Apps")
	int32 AppId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Steam|Apps")
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Steam|Apps")
	FString Name;
};

/** Fired when a DLC finished installing. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamDlcInstalled, int32, AppId);

/**
 * Fired when a second steam://run/...?connect arrives while the app is already running.
 * Consumers should re-read GetLaunchCommandLine / GetLaunchQueryParam.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSteamNewUrlLaunchParameters);

/** Fired for games running in timed-trial mode when the allowed/played time changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnSteamTimedTrialStatus, int32, AppId, bool, bIsOffline, int32, SecondsAllowed, int32, SecondsPlayed);

/** Fired when a GetFileDetails request completes (Sha1Hex/FileSize are empty/0 on failure). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnSteamFileDetails, bool, bSuccess, int64, FileSize, const FString&, Sha1Hex, int32, Flags);

/**
 * Wraps ISteamApps: ownership/subscription checks, DLC management, language,
 * install info and launch parameters.
 */
UCLASS()
class UNREALEXTENDEDSTEAM_API UESteamAppsSubsystem : public UESteamSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** True when the active account owns (or family-shares) the running app. */
	UFUNCTION(BlueprintPure, Category = "Steam|Apps")
	bool IsSubscribed() const;

	/** True when the active account has a license for the given app id. */
	UFUNCTION(BlueprintPure, Category = "Steam|Apps")
	bool IsSubscribedApp(int32 AppId) const;

	UFUNCTION(BlueprintPure, Category = "Steam|Apps")
	bool IsSubscribedFromFreeWeekend() const;

	UFUNCTION(BlueprintPure, Category = "Steam|Apps")
	bool IsSubscribedFromFamilySharing() const;

	/** True when the given DLC is installed. */
	UFUNCTION(BlueprintPure, Category = "Steam|Apps|DLC")
	bool IsDlcInstalled(int32 DlcAppId) const;

	UFUNCTION(BlueprintPure, Category = "Steam|Apps|DLC")
	int32 GetDlcCount() const;

	UFUNCTION(BlueprintCallable, Category = "Steam|Apps|DLC")
	bool GetDlcDataByIndex(int32 Index, FESteamDlcData& OutDlc) const;

	UFUNCTION(BlueprintCallable, Category = "Steam|Apps|DLC")
	void InstallDlc(int32 DlcAppId);

	UFUNCTION(BlueprintCallable, Category = "Steam|Apps|DLC")
	void UninstallDlc(int32 DlcAppId);

	/** Download progress of an installing DLC. Returns false when nothing is downloading. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Apps|DLC")
	bool GetDlcDownloadProgress(int32 DlcAppId, int64& OutBytesDownloaded, int64& OutBytesTotal) const;

	/** Current game language for this app (e.g. "english"). */
	UFUNCTION(BlueprintPure, Category = "Steam|Apps")
	FString GetCurrentGameLanguage() const;

	/** Comma-separated list of languages the app supports. */
	UFUNCTION(BlueprintPure, Category = "Steam|Apps")
	FString GetAvailableGameLanguages() const;

	UFUNCTION(BlueprintPure, Category = "Steam|Apps")
	bool IsLowViolence() const;

	UFUNCTION(BlueprintPure, Category = "Steam|Apps")
	bool IsCybercafe() const;

	UFUNCTION(BlueprintPure, Category = "Steam|Apps")
	bool IsVACBanned() const;

	/** Unix time of the account's earliest purchase of the given app. */
	UFUNCTION(BlueprintPure, Category = "Steam|Apps")
	int64 GetEarliestPurchaseTime(int32 AppId) const;

	UFUNCTION(BlueprintPure, Category = "Steam|Apps")
	bool IsAppInstalled(int32 AppId) const;

	UFUNCTION(BlueprintCallable, Category = "Steam|Apps")
	FString GetAppInstallDir(int32 AppId) const;

	/** Owner of the running app (differs from the local user under family sharing). */
	UFUNCTION(BlueprintPure, Category = "Steam|Apps")
	FESteamId GetAppOwner() const;

	/** Value of a launch query parameter (steam://run/<appid>//?param=value), empty when absent. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Apps")
	FString GetLaunchQueryParam(const FString& Key) const;

	/** Command line the app was launched with through Steam. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Apps")
	FString GetLaunchCommandLine() const;

	/** Name of the opted-in beta branch. Returns false on the default branch. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Apps")
	bool GetCurrentBetaName(FString& OutBetaName) const;

	/** Asks Steam to verify the app's content on next launch. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Apps")
	bool MarkContentCorrupt(bool bMissingFilesOnly);

	/** The build id of this app; may change at any time from backend updates. Wraps ISteamApps::GetAppBuildId. */
	UFUNCTION(BlueprintPure, Category = "Steam|Apps")
	int32 GetAppBuildId() const;

	/** Depot ids installed for the given app, in mount order. Wraps ISteamApps::GetInstalledDepots. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Apps")
	void GetInstalledDepots(int32 AppId, TArray<int32>& OutDepotIds) const;

	/** Sets the DLC AppID currently being played (0 for none) so Steam can track major DLC usage. Wraps ISteamApps::SetDlcContext. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Apps|DLC")
	bool SetDlcContext(int32 AppId);

	/** True when this app is a limited-playtime timed trial; outputs the allowed and played seconds. Wraps ISteamApps::BIsTimedTrial. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Apps")
	bool IsTimedTrial(int32& OutSecondsAllowed, int32& OutSecondsPlayed) const;

	/**
	 * Requests the size, SHA1 and flags of an installed file, relative to the install directory.
	 * The result arrives on OnFileDetails. Wraps ISteamApps::GetFileDetails.
	 * Returns false when the request could not be dispatched.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Apps")
	bool GetFileDetails(const FString& FileName);

	/**
	 * Total number of known beta branches (including the default "public" branch); OutAvailable is how many
	 * betas are selectable and OutPrivate how many are private/password-gated. Wraps ISteamApps::GetNumBetas.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Apps|Betas")
	int32 GetNumBetas(int32& OutAvailable, int32& OutPrivate) const;

	/** Details of the beta branch at Index (name, description, flags, build id). Wraps ISteamApps::GetBetaInfo. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Apps|Betas")
	bool GetBetaInfo(int32 Index, int32& OutFlags, int32& OutBuildId, FString& OutName, FString& OutDescription) const;

	/** Selects a beta branch as active (may require Steam to restart the game to update). Wraps ISteamApps::SetActiveBeta. */
	UFUNCTION(BlueprintCallable, Category = "Steam|Apps|Betas")
	bool SetActiveBeta(const FString& BetaName);

	UPROPERTY(BlueprintAssignable, Category = "Steam|Apps|DLC")
	FOnSteamDlcInstalled OnDlcInstalled;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Apps")
	FOnSteamNewUrlLaunchParameters OnNewUrlLaunchParameters;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Apps")
	FOnSteamTimedTrialStatus OnTimedTrialStatus;

	UPROPERTY(BlueprintAssignable, Category = "Steam|Apps")
	FOnSteamFileDetails OnFileDetails;

protected:
	virtual void HandleSteamClientInitialized() override;
	virtual void HandleSteamClientShutdown() override;

private:
	friend class FESteamAppsCallbacks;
	TSharedPtr<class FESteamAppsCallbacks> Callbacks;
};
