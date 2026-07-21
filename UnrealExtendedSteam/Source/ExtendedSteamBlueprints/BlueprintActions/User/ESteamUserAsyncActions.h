// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/ESteamAsyncActionBase.h"
#include "ESteamUserAsyncActions.generated.h"

class UESteamUserSubsystem;
class UESteamAppsSubsystem;

/** Completion pin for the web API auth ticket node (HexTicket is empty on failure). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamAsyncWebApiAuthTicketPin, const FString&, HexTicket);

/** Completion pin for the encrypted app ticket node (HexTicket is empty on failure). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamAsyncEncryptedAppTicketPin, const FString&, HexTicket);

/** Completion pin for the store auth URL node (Url is empty on failure). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamAsyncStoreAuthURLPin, const FString&, Url);

/** Completion pin for the market eligibility node (bAllowed is false on failure). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamAsyncMarketEligibilityPin, bool, bAllowed);

/** Completion pin for the duration control node (all fields 0/false on failure). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FSteamAsyncDurationControlPin, bool, bApplicable, int32, SecondsLast5h, int32, SecondsToday, int32, SecondsRemaining);

/** Completion pin for the file details node (FileSize 0 / Sha1Hex empty on failure). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FSteamAsyncFileDetailsPin, int64, FileSize, const FString&, Sha1Hex, int32, Flags);

/**
 * Requests an auth ticket for a web API backend (e.g. PlayFab LoginWithSteam) and
 * completes when the matching ticket arrives from UESteamUserSubsystem.
 */
UCLASS()
class USteamAsyncGetWebApiAuthTicket : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Requests an auth ticket for a web API backend (e.g. PlayFab LoginWithSteam).
	 * RemoteServiceIdentity optionally pins the ticket to a named service (empty = any).
	 * @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|User", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Get Web API Auth Ticket"))
	static USteamAsyncGetWebApiAuthTicket* GetWebApiAuthTicket(UObject* WorldContext, const FString& RemoteServiceIdentity, float Timeout = 10.0f);

	//~ UBlueprintAsyncActionBase
	virtual void Activate() override;

	/** The ticket is ready; HexTicket holds it hex-encoded. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncWebApiAuthTicketPin OnSuccess;

	/** Steam is unavailable or the request failed; HexTicket is empty. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncWebApiAuthTicketPin OnFailure;

private:
	UFUNCTION()
	void HandleWebApiAuthTicketReady(bool bSuccess, int32 TicketHandle, const FString& HexTicket);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, const FString& HexTicket);

	TWeakObjectPtr<UESteamUserSubsystem> UserSubsystem;
	FString RemoteServiceIdentity;
	int32 RequestHandle = 0;
};

/**
 * Requests an encrypted app ticket and completes when it arrives from
 * UESteamUserSubsystem.
 */
UCLASS()
class USteamAsyncRequestEncryptedAppTicket : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Requests an encrypted app ticket (one request may be in flight at a time, Steam-side).
	 * @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|User", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Request Encrypted App Ticket"))
	static USteamAsyncRequestEncryptedAppTicket* RequestEncryptedAppTicket(UObject* WorldContext, float Timeout = 10.0f);

	//~ UBlueprintAsyncActionBase
	virtual void Activate() override;

	/** The ticket is ready; HexTicket holds it hex-encoded. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncEncryptedAppTicketPin OnSuccess;

	/** Steam is unavailable or the request failed; HexTicket is empty. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncEncryptedAppTicketPin OnFailure;

private:
	UFUNCTION()
	void HandleEncryptedAppTicketReady(bool bSuccess, const FString& HexTicket);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, const FString& HexTicket);

	TWeakObjectPtr<UESteamUserSubsystem> UserSubsystem;
};

/**
 * Requests a store checkout auth URL and completes when it arrives from UESteamUserSubsystem.
 */
UCLASS()
class USteamAsyncRequestStoreAuthURL : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Requests a URL that authenticates an in-game browser for store checkout, then redirects to RedirectUrl.
	 * @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|User", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Request Store Auth URL"))
	static USteamAsyncRequestStoreAuthURL* RequestStoreAuthURL(UObject* WorldContext, const FString& RedirectUrl, float Timeout = 10.0f);

	//~ UBlueprintAsyncActionBase
	virtual void Activate() override;

	/** The URL is ready. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncStoreAuthURLPin OnSuccess;

	/** Steam is unavailable or the request failed; Url is empty. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncStoreAuthURLPin OnFailure;

private:
	UFUNCTION()
	void HandleStoreAuthURL(bool bSuccess, const FString& Url);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, const FString& Url);

	TWeakObjectPtr<UESteamUserSubsystem> UserSubsystem;
	FString RedirectUrl;
};

/**
 * Requests the local user's Steam Community Market eligibility and completes when it arrives
 * from UESteamUserSubsystem.
 */
UCLASS()
class USteamAsyncGetMarketEligibility : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Requests the local user's Steam Community Market eligibility.
	 * @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|User", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Get Market Eligibility"))
	static USteamAsyncGetMarketEligibility* GetMarketEligibility(UObject* WorldContext, float Timeout = 10.0f);

	//~ UBlueprintAsyncActionBase
	virtual void Activate() override;

	/** The eligibility result is ready; bAllowed reports market access. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncMarketEligibilityPin OnSuccess;

	/** Steam is unavailable or the request failed; bAllowed is false. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncMarketEligibilityPin OnFailure;

private:
	UFUNCTION()
	void HandleMarketEligibility(bool bAllowed);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bResultArrived, bool bAllowed);

	TWeakObjectPtr<UESteamUserSubsystem> UserSubsystem;
};

/**
 * Requests the current anti-indulgence / duration control state and completes when it arrives
 * from UESteamUserSubsystem.
 */
UCLASS()
class USteamAsyncGetDurationControl : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Requests the current anti-indulgence / duration control state.
	 * @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|User", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Get Duration Control"))
	static USteamAsyncGetDurationControl* GetDurationControl(UObject* WorldContext, float Timeout = 10.0f);

	//~ UBlueprintAsyncActionBase
	virtual void Activate() override;

	/** The duration control state is ready. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncDurationControlPin OnSuccess;

	/** Steam is unavailable or the request failed; all fields are 0/false. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncDurationControlPin OnFailure;

private:
	UFUNCTION()
	void HandleDurationControl(bool bApplicable, int32 SecondsLast5h, int32 SecondsToday, int32 SecondsRemaining);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bResultArrived, bool bApplicable, int32 SecondsLast5h, int32 SecondsToday, int32 SecondsRemaining);

	TWeakObjectPtr<UESteamUserSubsystem> UserSubsystem;
};

/**
 * Requests the size, SHA1 and flags of an installed file and completes when the result arrives
 * from UESteamAppsSubsystem.
 */
UCLASS()
class USteamAsyncGetFileDetails : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Requests the details of an installed file, relative to the game install directory.
	 * @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|Apps", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Get File Details"))
	static USteamAsyncGetFileDetails* GetFileDetails(UObject* WorldContext, const FString& FileName, float Timeout = 10.0f);

	//~ UBlueprintAsyncActionBase
	virtual void Activate() override;

	/** The file details are ready. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncFileDetailsPin OnSuccess;

	/** Steam is unavailable or the request failed; FileSize is 0 and Sha1Hex is empty. */
	UPROPERTY(BlueprintAssignable)
	FSteamAsyncFileDetailsPin OnFailure;

private:
	UFUNCTION()
	void HandleFileDetails(bool bSuccess, int64 FileSize, const FString& Sha1Hex, int32 Flags);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, int64 FileSize, const FString& Sha1Hex, int32 Flags);

	TWeakObjectPtr<UESteamAppsSubsystem> AppsSubsystem;
	FString FileName;
};
