// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Shared/ESteamSubsystem.h"
#include "Shared/ESteamTypes.h"
#include "ESteamUserSubsystem.generated.h"

/** Result of starting a server-side auth session (mirrors Steamworks EBeginAuthSessionResult + unavailable). */
UENUM(BlueprintType)
enum class EESteamBeginAuthResult : uint8
{
	Ok,
	InvalidTicket,
	DuplicateRequest,
	InvalidVersion,
	GameMismatch,
	ExpiredTicket,
	SteamUnavailable
};

/** Result of UserHasLicenseForApp (mirrors Steamworks EUserHasLicenseForAppResult). */
UENUM(BlueprintType)
enum class EESteamUserHasLicenseResult : uint8
{
	/** User has a license for the specified app. */
	HasLicense,
	/** User does not have a license for the specified app. */
	DoesNotHaveLicense,
	/** User has not been authenticated (call BeginAuthSession first), or Steam is unavailable. */
	NoAuth
};

/**
 * Detailed outcome carried by the ValidateAuthTicket callback
 * (mirrors Steamworks EAuthSessionResponse; only k_EAuthSessionResponseOK authorizes the peer).
 */
UENUM(BlueprintType)
enum class EESteamAuthSessionResponse : uint8
{
	/** Ticket is valid and the user is online and owns the game. */
	Ok,
	/** The user is not connected to Steam. */
	UserNotConnectedToSteam,
	/** The license has expired. */
	NoLicenseOrExpired,
	/** The user is VAC banned for this game. */
	VACBanned,
	/** The user account logged in elsewhere and this session was disconnected. */
	LoggedInElseWhere,
	/** VAC was unable to perform anti-cheat checks on this user. */
	VACCheckTimedOut,
	/** The ticket was cancelled by the issuer. */
	AuthTicketCanceled,
	/** The ticket has already been used and is not valid. */
	AuthTicketInvalidAlreadyUsed,
	/** The ticket is not from a user instance currently connected to Steam. */
	AuthTicketInvalid,
	/** The user is banned for this game (web-api issued, not VAC). */
	PublisherIssuedBan,
	/** Any newer/unknown response value, or Steam unavailable. */
	Unknown
};

/** Fired when a web API auth ticket request completes (PlayFab LoginWithSteam, backend auth...). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSteamWebApiAuthTicketReady, bool, bSuccess, int32, TicketHandle, const FString&, HexTicket);

/** Fired when a requested encrypted app ticket is ready. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamEncryptedAppTicketReady, bool, bSuccess, const FString&, HexTicket);

/** Fired when Steam reports the local auth session ticket became valid (or failed). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamAuthTicketResponse, int32, TicketHandle, bool, bSuccess);

/**
 * Fired for peers validated through BeginAuthSession (server side).
 * bAuthorized is true only when Response == Ok. OwnerSteamId is the actual license owner and
 * differs from SteamId when the peer is playing via Family Sharing.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnSteamValidateAuthTicket, FESteamId, SteamId, bool, bAuthorized, EESteamAuthSessionResponse, Response, FESteamId, OwnerSteamId);

/** Fired when an authenticated connection to the Steam back-end is (re)established. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSteamServersConnected);

/** Fired when the client loses its connection to the Steam servers (Result mirrors EResult). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamServersDisconnected, int32, Result);

/** Fired when a connection attempt to the Steam servers fails (Result mirrors EResult). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamServerConnectFailure, int32, Result, bool, bStillRetrying);

/** Fired when a GetMarketEligibility request completes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSteamMarketEligibility, bool, bAllowed);

/** Fired when a RequestStoreAuthURL request completes (Url is empty on failure). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamStoreAuthURL, bool, bSuccess, const FString&, Url);

/**
 * Fired for anti-indulgence / duration control, either in response to GetDurationControl or on Steam's timers.
 * SecondsRemaining reaches 0 when the user hits a regulatory playtime limit.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnSteamDurationControl, bool, bApplicable, int32, SecondsLast5h, int32, SecondsToday, int32, SecondsRemaining);

/**
 * Wraps ISteamUser: local identity, auth session tickets, web API tickets and
 * encrypted app tickets, license/ownership checks, market eligibility and duration control.
 */
UCLASS()
class UNREALEXTENDEDSTEAM_API UESteamUserSubsystem : public UESteamSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** The local user's Steam id (invalid when Steam is unavailable). */
	UFUNCTION(BlueprintPure, Category = "Steam|User")
	FESteamId GetLocalSteamId() const;

	/** True when the local user is logged into Steam. */
	UFUNCTION(BlueprintPure, Category = "Steam|User")
	bool IsLoggedOn() const;

	/** The local user's Steam community level (0 when unavailable). */
	UFUNCTION(BlueprintPure, Category = "Steam|User")
	int32 GetPlayerSteamLevel() const;

	/**
	 * Trading-card badge level for the given card series (regular badge maxes at 5, foil at 1).
	 * Wraps ISteamUser::GetGameBadgeLevel.
	 */
	UFUNCTION(BlueprintPure, Category = "Steam|User")
	int32 GetGameBadgeLevel(int32 Series, bool bFoil) const;

	UFUNCTION(BlueprintPure, Category = "Steam|User")
	bool IsBehindNAT() const;

	UFUNCTION(BlueprintPure, Category = "Steam|User")
	bool IsPhoneVerified() const;

	UFUNCTION(BlueprintPure, Category = "Steam|User")
	bool IsTwoFactorEnabled() const;

	/** True when the user's phone number is being used for identification. Wraps ISteamUser::BIsPhoneIdentifying. */
	UFUNCTION(BlueprintPure, Category = "Steam|User")
	bool IsPhoneIdentifying() const;

	/** True when the user's phone number is awaiting (re)verification. Wraps ISteamUser::BIsPhoneRequiringVerification. */
	UFUNCTION(BlueprintPure, Category = "Steam|User")
	bool IsPhoneRequiringVerification() const;

	/**
	 * Whether a peer owns the given app, once their ticket has been passed to BeginAuthSession.
	 * Wraps ISteamUser::UserHasLicenseForApp (returns NoAuth when Steam is unavailable).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|User")
	EESteamUserHasLicenseResult UserHasLicenseForApp(FESteamId SteamId, int32 AppId) const;

	/** Local storage folder for the current Steam account (empty when unavailable). Wraps ISteamUser::GetUserDataFolder. */
	UFUNCTION(BlueprintCallable, Category = "Steam|User")
	FString GetUserDataFolder() const;

	/**
	 * Publishes the game server the local user is playing on so friends can join.
	 * ServerIp is a dotted IPv4 string ("203.0.113.7"); pass an empty/zero id to clear.
	 * Wraps ISteamUser::AdvertiseGame.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|User")
	void AdvertiseGame(FESteamId GameServerId, const FString& ServerIp, int32 ServerPort);

	/**
	 * Creates an auth session ticket for game-server authentication (BeginAuthSession on the peer).
	 * Returns the ticket handle (0 on failure) and outputs the ticket hex-encoded.
	 * Validity confirmation arrives on OnAuthTicketResponse.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|User|Auth")
	int32 GetAuthSessionTicket(FString& OutHexTicket);

	/**
	 * Requests an auth ticket for a web API backend (e.g. PlayFab LoginWithSteam).
	 * RemoteServiceIdentity optionally pins the ticket to a named service (empty = any).
	 * Returns the ticket handle (0 on failure); the ticket arrives on OnWebApiAuthTicketReady.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|User|Auth")
	int32 RequestWebApiAuthTicket(const FString& RemoteServiceIdentity);

	/** Cancels a ticket returned by GetAuthSessionTicket / RequestWebApiAuthTicket. */
	UFUNCTION(BlueprintCallable, Category = "Steam|User|Auth")
	void CancelAuthTicket(int32 TicketHandle);

	/** Requests an encrypted app ticket; the result arrives on OnEncryptedAppTicketReady. */
	UFUNCTION(BlueprintCallable, Category = "Steam|User|Auth")
	bool RequestEncryptedAppTicket();

	/** Server side: validates a peer's hex-encoded session ticket. Final result arrives on OnValidateAuthTicket. */
	UFUNCTION(BlueprintCallable, Category = "Steam|User|Auth")
	EESteamBeginAuthResult BeginAuthSession(const FString& HexTicket, FESteamId SteamId);

	/** Server side: ends a session started with BeginAuthSession. */
	UFUNCTION(BlueprintCallable, Category = "Steam|User|Auth")
	void EndAuthSession(FESteamId SteamId);

	/**
	 * Requests a URL that authenticates an in-game browser for store checkout, then redirects to RedirectUrl.
	 * The result arrives on OnStoreAuthURL. Wraps ISteamUser::RequestStoreAuthURL.
	 * Returns false when the request could not be dispatched.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|User")
	bool RequestStoreAuthURL(const FString& RedirectUrl);

	/**
	 * Requests the local user's Steam Community Market eligibility.
	 * The result arrives on OnMarketEligibility. Wraps ISteamUser::GetMarketEligibility.
	 * Returns false when the request could not be dispatched.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|User")
	bool GetMarketEligibility();

	/**
	 * Requests the current anti-indulgence / duration control state.
	 * The result arrives on OnDurationControl. Wraps ISteamUser::GetDurationControl.
	 * Returns false when the request could not be dispatched.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|User")
	bool GetDurationControl();

	/**
	 * Advises the Steam China duration-control system about the game's online state so offline play
	 * does not count against playtime limits. NewState mirrors EDurationControlOnlineState
	 * (0 Invalid, 1 Offline, 2 Online, 3 OnlineHighPriority). Wraps ISteamUser::BSetDurationControlOnlineState.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|User")
	bool SetDurationControlOnlineState(int32 NewState);

	UPROPERTY(BlueprintAssignable, Category = "Steam|User|Auth")
	FOnSteamWebApiAuthTicketReady OnWebApiAuthTicketReady;

	UPROPERTY(BlueprintAssignable, Category = "Steam|User|Auth")
	FOnSteamEncryptedAppTicketReady OnEncryptedAppTicketReady;

	UPROPERTY(BlueprintAssignable, Category = "Steam|User|Auth")
	FOnSteamAuthTicketResponse OnAuthTicketResponse;

	UPROPERTY(BlueprintAssignable, Category = "Steam|User|Auth")
	FOnSteamValidateAuthTicket OnValidateAuthTicket;

	UPROPERTY(BlueprintAssignable, Category = "Steam|User")
	FOnSteamServersConnected OnSteamServersConnected;

	UPROPERTY(BlueprintAssignable, Category = "Steam|User")
	FOnSteamServersDisconnected OnSteamServersDisconnected;

	UPROPERTY(BlueprintAssignable, Category = "Steam|User")
	FOnSteamServerConnectFailure OnSteamServerConnectFailure;

	UPROPERTY(BlueprintAssignable, Category = "Steam|User")
	FOnSteamMarketEligibility OnMarketEligibility;

	UPROPERTY(BlueprintAssignable, Category = "Steam|User")
	FOnSteamStoreAuthURL OnStoreAuthURL;

	UPROPERTY(BlueprintAssignable, Category = "Steam|User")
	FOnSteamDurationControl OnDurationControl;

protected:
	virtual void HandleSteamClientInitialized() override;
	virtual void HandleSteamClientShutdown() override;

private:
	friend class FESteamUserCallbacks;
	TSharedPtr<class FESteamUserCallbacks> Callbacks;
};
