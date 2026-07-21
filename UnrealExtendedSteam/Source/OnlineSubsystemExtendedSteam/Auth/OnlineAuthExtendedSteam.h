// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/OnlineSubsystemExtendedSteamPackage.h"

class FOnlineSubsystemExtendedSteam;
class FOnlineAuthExtendedSteamCallbacks;

/**
 * Delegate for the async GetAuthTicketForWebApi result.
 *  - bSuccess : true when Steam returned k_EResultOK with a non-empty ticket.
 *  - HexTicket: hex-encoded ticket bytes, ready to hand to a web backend's
 *               ISteamUserAuth/AuthenticateUserTicket call. Empty on failure.
 */
DECLARE_DELEGATE_TwoParams(FOnExtendedSteamWebApiTicket, bool /*bSuccess*/, FString /*HexTicket*/);

/**
 * Plugin-specific Auth interface for the "EXTENDEDSTEAM" online subsystem.
 *
 * This is NOT an engine IOnlineSubsystem interface (the framework declares no Auth getter). It is
 * owned by FOnlineSubsystemExtendedSteam and reached through GetAuthInterfaceExtended(), mirroring
 * how the vendor exposes plugin-specific Steam interfaces. It wraps ISteamUser's authentication
 * surface for two clients:
 *
 *  - Session tickets (GetAuthSessionTicket / Begin/EndAuthSession / CancelAuthTicket): the classic
 *    peer-to-peer / game-server authentication flow. GetAuthSessionTicket mints a ticket bound to a
 *    SteamNetworkingIdentity (null here == unbound, which is what web backends and simple listen
 *    servers need); server code validates it with BeginAuthSession and tears it down with
 *    EndAuthSession.
 *  - Web-API tickets (GetAuthTicketForWebApi): the modern, preferred ticket for third-party web
 *    backends (PlayFab LoginWithSteam, EOS Steam auth, custom services). It is inherently async:
 *    the request returns a handle and the ticket bytes arrive later via GetTicketForWebApiResponse_t,
 *    delivered through FOnExtendedSteamWebApiTicket. Requires Steamworks SDK >= 1.57.
 *
 * Handles are HAuthTicket (a uint32); this header keeps SDK types out and uses uint32 directly,
 * with 0 standing in for k_HAuthTicketInvalid. Every method is offline-safe: with the SDK compiled
 * out (WITH_EXTENDEDSTEAM_SDK=0) or the Steam client API down, minting returns an invalid handle,
 * async requests fire their delegate with failure, and the teardown helpers are no-ops.
 *
 * Callback ownership: like the sibling interfaces, this object NEVER pumps SteamAPI_RunCallbacks —
 * FExtendedSteamSharedModule owns the callback pump. The GetTicketForWebApiResponse_t listener lives
 * in FOnlineAuthExtendedSteamCallbacks (cpp-local) for the lifetime of this interface.
 */
class FOnlineAuthExtendedSteam
{
public:
	explicit FOnlineAuthExtendedSteam(FOnlineSubsystemExtendedSteam* InSubsystem);
	virtual ~FOnlineAuthExtendedSteam();

	FOnlineAuthExtendedSteam(const FOnlineAuthExtendedSteam&) = delete;
	FOnlineAuthExtendedSteam& operator=(const FOnlineAuthExtendedSteam&) = delete;

	/**
	 * Mints a classic session ticket for the local user, synchronously.
	 * Uses the modern SteamNetworkingIdentity-binding overload where the SDK provides it
	 * (>= 1.57); the ticket is left unbound (null identity), suitable for web backends and
	 * listen servers that validate with BeginAuthSession.
	 *
	 * @param OutTicket    filled with the raw ticket bytes on success (emptied on failure).
	 * @param OutHexTicket hex-encoded form of OutTicket on success (emptied on failure).
	 * @return the HAuthTicket handle (pass to CancelAuthTicket when done); 0 on failure.
	 */
	uint32 GetAuthSessionTicket(TArray<uint8>& OutTicket, FString& OutHexTicket);

	/**
	 * Requests a web-API auth ticket for a named third-party service, asynchronously.
	 * The ticket is delivered via Delegate once GetTicketForWebApiResponse_t arrives.
	 *
	 * @param ServiceIdentity optional service identity string the backend expects (may be empty).
	 * @param Delegate        receives (bSuccess, HexTicket). Always fired: immediately with failure
	 *                        when unavailable, otherwise from the Steam callback.
	 * @return the HAuthTicket handle correlating the pending request; 0 when it could not start
	 *         (SDK < 1.57, SDK compiled out, or Steam client down).
	 */
	uint32 GetAuthTicketForWebApi(const FString& ServiceIdentity, const FOnExtendedSteamWebApiTicket& Delegate);

	/** Cancels/invalidates a ticket previously returned by GetAuthSessionTicket or GetAuthTicketForWebApi. */
	void CancelAuthTicket(uint32 AuthTicketHandle);

	/**
	 * Server-side: begins validating a remote user's ticket (ISteamUser::BeginAuthSession).
	 * @return the EBeginAuthSessionResult as an int32 (0 == k_EBeginAuthSessionResultOK); a negative
	 *         sentinel (-1) when Steam is unavailable.
	 */
	int32 BeginAuthSession(const TArray<uint8>& AuthTicket, uint64 SteamId);

	/** Server-side: stops tracking a remote user previously passed to BeginAuthSession. No-op offline. */
	void EndAuthSession(uint64 SteamId);

	/** True while the Steam client API is up (mirrors the sibling interfaces' offline gate). */
	bool IsSteamAvailableForAuth() const;

private:
	friend class FOnlineAuthExtendedSteamCallbacks;

	/** GetTicketForWebApiResponse_t arrived (forwarded by the callback holder); fires + clears the pending delegate. */
	void HandleWebApiTicketResponse(uint32 AuthTicketHandle, bool bSuccess, const FString& HexTicket);

	/** Owning subsystem (owns this object; outlives it). */
	FOnlineSubsystemExtendedSteam* Subsystem = nullptr;

	/** Native Steam callback listener (SDK-gated, defined in the cpp). */
	TSharedPtr<FOnlineAuthExtendedSteamCallbacks> Callbacks;

	/** Guards PendingWebApiTickets (callbacks dispatch on the pump thread). */
	mutable FCriticalSection AuthLock;

	/** Web-API ticket requests awaiting GetTicketForWebApiResponse_t, keyed by HAuthTicket. */
	TMap<uint32, FOnExtendedSteamWebApiTicket> PendingWebApiTickets;
};

typedef TSharedPtr<FOnlineAuthExtendedSteam, ESPMode::ThreadSafe> FOnlineAuthExtendedSteamPtr;
