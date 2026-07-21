// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/OnlineSubsystemExtendedSteamPackage.h"

class FOnlineSubsystemExtendedSteam;
class FOnlineEncryptedAppTicketExtendedSteamCallbacks;

/**
 * Delegate for the async RequestEncryptedAppTicket result.
 *  - bSuccess: true when Steam produced a ticket (k_EResultOK) and it was read back.
 *  - Ticket  : the encrypted app ticket bytes on success (empty on failure). The bytes are opaque;
 *              a trusted backend decrypts them with the app's Encrypted App Ticket key.
 */
DECLARE_DELEGATE_TwoParams(FOnExtendedSteamEncryptedAppTicket, bool /*bSuccess*/, const TArray<uint8>& /*Ticket*/);

/**
 * Plugin-specific Encrypted App Ticket interface for the "EXTENDEDSTEAM" online subsystem.
 *
 * Not an engine IOnlineSubsystem interface (the framework declares no such getter): it is owned by
 * FOnlineSubsystemExtendedSteam and reached through GetEncryptedAppTicketInterfaceExtended(). It
 * wraps ISteamUser::RequestEncryptedAppTicket / GetEncryptedAppTicket.
 *
 * Encrypted app tickets are the offline-verifiable alternative to session/web tickets: the client
 * asks Steam for a ticket (optionally embedding a small payload), and a trusted backend decrypts and
 * validates it with the app's private key — no live call back to Steam required. RequestEncryptedApp-
 * Ticket is asynchronous (SteamAPICall_t -> EncryptedAppTicketResponse_t); the actual bytes are then
 * fetched synchronously with GetEncryptedAppTicket.
 *
 * One request in flight at a time (Steam keeps a single most-recent ticket per process); starting a
 * second while one is pending fails immediately. Every method is offline-safe: with the SDK compiled
 * out or the Steam client down, RequestEncryptedAppTicket fires its delegate with failure and
 * GetEncryptedAppTicket returns false.
 *
 * Callback ownership: this object NEVER pumps SteamAPI_RunCallbacks — FExtendedSteamSharedModule owns
 * the pump. The EncryptedAppTicketResponse_t call-result lives in the cpp-local callback holder.
 */
class FOnlineEncryptedAppTicketExtendedSteam
{
public:
	explicit FOnlineEncryptedAppTicketExtendedSteam(FOnlineSubsystemExtendedSteam* InSubsystem);
	virtual ~FOnlineEncryptedAppTicketExtendedSteam();

	FOnlineEncryptedAppTicketExtendedSteam(const FOnlineEncryptedAppTicketExtendedSteam&) = delete;
	FOnlineEncryptedAppTicketExtendedSteam& operator=(const FOnlineEncryptedAppTicketExtendedSteam&) = delete;

	/**
	 * Asks Steam to issue a fresh encrypted app ticket, optionally embedding DataToInclude.
	 * The ticket is delivered via Delegate once EncryptedAppTicketResponse_t arrives.
	 *
	 * @param DataToInclude optional payload embedded in the ticket (may be empty).
	 * @param Delegate      receives (bSuccess, Ticket). Always fired: immediately with failure when
	 *                      unavailable or a request is already pending, otherwise from the callback.
	 * @return true when the request was queued; false when it could not start.
	 */
	bool RequestEncryptedAppTicket(const TArray<uint8>& DataToInclude, const FOnExtendedSteamEncryptedAppTicket& Delegate);

	/**
	 * Reads back the most recently issued encrypted app ticket synchronously.
	 * @param OutTicket filled with the ticket bytes on success (emptied on failure).
	 * @return true when a current ticket was available and read.
	 */
	bool GetEncryptedAppTicket(TArray<uint8>& OutTicket) const;

	/** True while the Steam client API is up (mirrors the sibling interfaces' offline gate). */
	bool IsSteamAvailableForTickets() const;

private:
	friend class FOnlineEncryptedAppTicketExtendedSteamCallbacks;

	/** EncryptedAppTicketResponse_t arrived (forwarded by the callback holder); reads bytes + fires the pending delegate. */
	void HandleEncryptedAppTicketResponse(bool bSuccess);

	/** Owning subsystem (owns this object; outlives it). */
	FOnlineSubsystemExtendedSteam* Subsystem = nullptr;

	/** Native Steam call-result listener (SDK-gated, defined in the cpp). */
	TSharedPtr<FOnlineEncryptedAppTicketExtendedSteamCallbacks> Callbacks;

	/** Guards the single-in-flight request state below (the call-result dispatches on the pump thread). */
	mutable FCriticalSection TicketLock;

	/** One in-flight request: kept until EncryptedAppTicketResponse_t confirms it. */
	bool bRequestInFlight = false;
	FOnExtendedSteamEncryptedAppTicket PendingRequestDelegate;
};

typedef TSharedPtr<FOnlineEncryptedAppTicketExtendedSteam, ESPMode::ThreadSafe> FOnlineEncryptedAppTicketExtendedSteamPtr;
