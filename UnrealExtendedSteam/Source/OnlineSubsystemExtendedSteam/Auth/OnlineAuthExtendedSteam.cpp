// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Auth/OnlineAuthExtendedSteam.h"
#include "Core/OnlineSubsystemExtendedSteam.h"
#include "ExtendedSteamSharedModule.h"
#include "Shared/ESteamSDK.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
#include "steam/steam_gameserver.h"
THIRD_PARTY_INCLUDES_END
#endif

namespace ExtendedSteamAuth
{
	/** True while the shared module has the Steam client API up (the OSS never initializes it itself here). */
	static bool IsSteamClientUp()
	{
#if WITH_EXTENDEDSTEAM_SDK
		return FExtendedSteamSharedModule::IsModuleAvailable()
			&& FExtendedSteamSharedModule::Get().IsSteamClientInitialized();
#else
		return false;
#endif
	}
}

#if WITH_EXTENDEDSTEAM_SDK
/** True while the shared module has the Steam game-server API up (dedicated-server ticket validation). */
static bool IsGameServerUpForAuth()
{
	return FExtendedSteamSharedModule::IsModuleAvailable()
		&& FExtendedSteamSharedModule::Get().IsSteamGameServerInitialized()
		&& SteamGameServer() != nullptr;
}
#endif

#if WITH_EXTENDEDSTEAM_SDK
/**
 * Native Steam callback listener for the Auth interface; alive for the lifetime of the owning
 * FOnlineAuthExtendedSteam (which the subsystem destroys on Shutdown). Only the web-API ticket flow
 * needs a callback — session/web tickets requested via GetAuthTicketForWebApi land in
 * GetTicketForWebApiResponse_t, dispatched by the shared module's pump.
 */
class FOnlineAuthExtendedSteamCallbacks
{
public:
	explicit FOnlineAuthExtendedSteamCallbacks(FOnlineAuthExtendedSteam* InOwner)
		: Owner(InOwner)
#if ESTEAM_SDK_AT_LEAST(157)
		, WebApiTicketCallback(this, &FOnlineAuthExtendedSteamCallbacks::OnWebApiTicketResponse)
#endif
	{
	}

	FOnlineAuthExtendedSteamCallbacks(const FOnlineAuthExtendedSteamCallbacks&) = delete;
	FOnlineAuthExtendedSteamCallbacks& operator=(const FOnlineAuthExtendedSteamCallbacks&) = delete;

private:
	// Declared before the callback member: members are initialized in declaration order, so Owner is
	// set before the callback registers with Steam.
	FOnlineAuthExtendedSteam* Owner = nullptr;

#if ESTEAM_SDK_AT_LEAST(157)
	void OnWebApiTicketResponse(GetTicketForWebApiResponse_t* Data)
	{
		if (Data == nullptr || Owner == nullptr)
		{
			return;
		}

		const bool bSuccess = Data->m_eResult == k_EResultOK && Data->m_cubTicket > 0;
		const FString HexTicket = bSuccess
			? BytesToHex(Data->m_rgubTicket, Data->m_cubTicket)
			: FString();
		Owner->HandleWebApiTicketResponse(static_cast<uint32>(Data->m_hAuthTicket), bSuccess, HexTicket);
	}

	CCallback<FOnlineAuthExtendedSteamCallbacks, GetTicketForWebApiResponse_t> WebApiTicketCallback;
#endif
};
#endif // WITH_EXTENDEDSTEAM_SDK

FOnlineAuthExtendedSteam::FOnlineAuthExtendedSteam(FOnlineSubsystemExtendedSteam* InSubsystem)
	: Subsystem(InSubsystem)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (ExtendedSteamAuth::IsSteamClientUp())
	{
		Callbacks = MakeShared<FOnlineAuthExtendedSteamCallbacks>(this);
	}
#endif
}

FOnlineAuthExtendedSteam::~FOnlineAuthExtendedSteam()
{
	if (DeferredWebApiTimeoutHandle.IsValid())
	{
		FTSTicker::RemoveTicker(DeferredWebApiTimeoutHandle);
		DeferredWebApiTimeoutHandle.Reset();
	}

	// Fail any web-API ticket requests still in flight or held so callers are never left hanging.
	TArray<FOnExtendedSteamWebApiTicket> Orphaned;
	{
		FScopeLock Lock(&AuthLock);
		PendingWebApiTickets.GenerateValueArray(Orphaned);
		PendingWebApiTickets.Empty();
		for (FDeferredWebApiRequest& Request : DeferredWebApiRequests)
		{
			Orphaned.Add(MoveTemp(Request.Delegate));
		}
		DeferredWebApiRequests.Empty();
	}
	for (const FOnExtendedSteamWebApiTicket& Delegate : Orphaned)
	{
		Delegate.ExecuteIfBound(false, FString());
	}
}

bool FOnlineAuthExtendedSteam::IsSteamAvailableForAuth() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return ExtendedSteamAuth::IsSteamClientUp() && SteamUser() != nullptr;
#else
	return false;
#endif
}

uint32 FOnlineAuthExtendedSteam::GetAuthSessionTicket(TArray<uint8>& OutTicket, FString& OutHexTicket)
{
	OutTicket.Reset();
	OutHexTicket.Reset();

#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailableForAuth() && SteamUser()->BLoggedOn())
	{
		uint8 TicketBuffer[8192];
		uint32 TicketSize = 0;

#if ESTEAM_SDK_AT_LEAST(157)
		// 1.57+ signature (unchanged through 1.64): a null SteamNetworkingIdentity leaves the ticket
		// unbound ("not tied to a specific remote identity"), which is what web backends and simple
		// listen servers validating via BeginAuthSession expect.
		const HAuthTicket TicketHandle = SteamUser()->GetAuthSessionTicket(TicketBuffer, sizeof(TicketBuffer), &TicketSize, nullptr);
#else
		const HAuthTicket TicketHandle = SteamUser()->GetAuthSessionTicket(TicketBuffer, sizeof(TicketBuffer), &TicketSize);
#endif

		if (TicketHandle != k_HAuthTicketInvalid && TicketSize > 0)
		{
			OutTicket.Append(TicketBuffer, static_cast<int32>(TicketSize));
			OutHexTicket = BytesToHex(TicketBuffer, static_cast<int32>(TicketSize));
			return static_cast<uint32>(TicketHandle);
		}

		UE_LOG(LogExtendedSteam, Warning, TEXT("GetAuthSessionTicket: Steam returned no ticket"));
	}
#endif
	return 0;
}

uint32 FOnlineAuthExtendedSteam::GetAuthTicketForWebApi(const FString& ServiceIdentity, const FOnExtendedSteamWebApiTicket& Delegate)
{
#if ESTEAM_SDK_AT_LEAST(157)
	if (!IsSteamAvailableForAuth())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("GetAuthTicketForWebApi: the Steam client API is not up"));
	}
	else if (!SteamUser()->BLoggedOn())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("GetAuthTicketForWebApi: the Steam user is not logged on"));
	}
	else
	{
		uint32 TicketHandle = 0;
		if (TryStartWebApiTicket(ServiceIdentity, Delegate, TicketHandle))
		{
			return TicketHandle;
		}

		// Steam refuses while another web-API ticket is pending, and the GetTicketForWebApiResponse_t
		// that ends that one reaches this object too, so the request is sent again from there.
		UE_LOG(LogExtendedSteam, Log, TEXT("GetAuthTicketForWebApi: Steam refused the request while another ticket is pending; retrying when it completes"));
		{
			FScopeLock Lock(&AuthLock);
			DeferredWebApiRequests.Add({ ServiceIdentity, Delegate });
		}
		RefreshDeferredWebApiTimeout();
		return 0;
	}
#else
	UE_LOG(LogExtendedSteam, Verbose, TEXT("GetAuthTicketForWebApi: requires Steamworks SDK >= 1.57"));
#endif

	// Unavailable: honour the delegate contract with an immediate failure and an invalid handle.
	Delegate.ExecuteIfBound(false, FString());
	return 0;
}

bool FOnlineAuthExtendedSteam::TryStartWebApiTicket(const FString& ServiceIdentity, const FOnExtendedSteamWebApiTicket& Delegate, uint32& OutHandle)
{
	OutHandle = 0;
#if ESTEAM_SDK_AT_LEAST(157)
	if (!IsSteamAvailableForAuth())
	{
		return false;
	}

	// A null identity requests a generic web ticket; a service string binds it to that identity.
	// TCHAR_TO_UTF8's temporary dies at the semicolon. Keep the conversion alive through the SDK call.
	const FTCHARToUTF8 IdentityUtf8(*ServiceIdentity);
	const char* IdentityArg = ServiceIdentity.IsEmpty() ? nullptr : IdentityUtf8.Get();
	const HAuthTicket TicketHandle = SteamUser()->GetAuthTicketForWebApi(IdentityArg);
	if (TicketHandle == k_HAuthTicketInvalid)
	{
		return false;
	}

	{
		FScopeLock Lock(&AuthLock);
		PendingWebApiTickets.Add(static_cast<uint32>(TicketHandle), Delegate);
	}
	OutHandle = static_cast<uint32>(TicketHandle);
	return true;
#else
	return false;
#endif
}

void FOnlineAuthExtendedSteam::RetryDeferredWebApiRequests()
{
	TArray<FDeferredWebApiRequest> Waiting;
	{
		FScopeLock Lock(&AuthLock);
		Waiting = MoveTemp(DeferredWebApiRequests);
		DeferredWebApiRequests.Reset();
	}
	if (Waiting.IsEmpty())
	{
		return;
	}

	// Oldest first. Once one gets through it is the pending ticket, so the rest wait for its response.
	bool bStartedOne = false;
	TArray<FDeferredWebApiRequest> StillWaiting;
	for (FDeferredWebApiRequest& Request : Waiting)
	{
		uint32 TicketHandle = 0;
		if (!bStartedOne && TryStartWebApiTicket(Request.ServiceIdentity, Request.Delegate, TicketHandle))
		{
			bStartedOne = true;
			UE_LOG(LogExtendedSteam, Log, TEXT("GetAuthTicketForWebApi: held request sent again (handle %u)"), TicketHandle);
			continue;
		}
		StillWaiting.Add(MoveTemp(Request));
	}

	{
		FScopeLock Lock(&AuthLock);
		// Requests held while this ran go after the older ones.
		StillWaiting.Append(MoveTemp(DeferredWebApiRequests));
		DeferredWebApiRequests = MoveTemp(StillWaiting);
	}

	// The deadline measures time without progress, so it restarts when a request got through.
	if (bStartedOne)
	{
		RefreshDeferredWebApiTimeout();
	}
}

void FOnlineAuthExtendedSteam::RefreshDeferredWebApiTimeout()
{
	if (DeferredWebApiTimeoutHandle.IsValid())
	{
		FTSTicker::RemoveTicker(DeferredWebApiTimeoutHandle);
		DeferredWebApiTimeoutHandle.Reset();
	}

	bool bAnyHeld = false;
	{
		FScopeLock Lock(&AuthLock);
		bAnyHeld = !DeferredWebApiRequests.IsEmpty();
	}

	// A one-shot deadline, not a retry loop: retries come from ticket responses. It exists because a
	// refusal with no pending ticket behind it would otherwise never be answered.
	if (bAnyHeld)
	{
		DeferredWebApiTimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateRaw(this, &FOnlineAuthExtendedSteam::HandleDeferredWebApiTimeout),
			DeferredWebApiTimeoutSeconds);
	}
}

bool FOnlineAuthExtendedSteam::HandleDeferredWebApiTimeout(float DeltaTime)
{
	DeferredWebApiTimeoutHandle.Reset();

	TArray<FDeferredWebApiRequest> Expired;
	{
		FScopeLock Lock(&AuthLock);
		Expired = MoveTemp(DeferredWebApiRequests);
		DeferredWebApiRequests.Reset();
	}

	if (!Expired.IsEmpty())
	{
		UE_LOG(LogExtendedSteam, Warning, TEXT("GetAuthTicketForWebApi: Steam kept refusing for %.0f s with no ticket completing; failing %d held request(s)"),
			DeferredWebApiTimeoutSeconds, Expired.Num());
	}
	for (const FDeferredWebApiRequest& Request : Expired)
	{
		Request.Delegate.ExecuteIfBound(false, FString());
	}

	// One-shot.
	return false;
}

void FOnlineAuthExtendedSteam::CancelAuthTicket(uint32 AuthTicketHandle)
{
	{
		// Drop any pending web-API delegate for this handle without firing it (caller cancelled).
		FScopeLock Lock(&AuthLock);
		PendingWebApiTickets.Remove(AuthTicketHandle);
	}

#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailableForAuth() && AuthTicketHandle != 0)
	{
		SteamUser()->CancelAuthTicket(static_cast<HAuthTicket>(AuthTicketHandle));
	}
#endif
}

int32 FOnlineAuthExtendedSteam::BeginAuthSession(const TArray<uint8>& AuthTicket, uint64 SteamId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (AuthTicket.Num() > 0)
	{
		// On a dedicated server only the Steam game-server API is up (SteamUser() is null), so ticket
		// validation must go through SteamGameServer(). Listen-server hosts are Steam clients and
		// validate via SteamUser() as before.
		if (Subsystem != nullptr && Subsystem->IsDedicated())
		{
			if (IsGameServerUpForAuth())
			{
				return static_cast<int32>(SteamGameServer()->BeginAuthSession(
					AuthTicket.GetData(), AuthTicket.Num(), CSteamID(SteamId)));
			}
		}
		else if (IsSteamAvailableForAuth())
		{
			return static_cast<int32>(SteamUser()->BeginAuthSession(
				AuthTicket.GetData(), AuthTicket.Num(), CSteamID(SteamId)));
		}
	}
#endif
	// Steam unavailable / empty ticket: negative sentinel distinct from any EBeginAuthSessionResult.
	return -1;
}

void FOnlineAuthExtendedSteam::EndAuthSession(uint64 SteamId)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (Subsystem != nullptr && Subsystem->IsDedicated())
	{
		if (IsGameServerUpForAuth())
		{
			SteamGameServer()->EndAuthSession(CSteamID(SteamId));
		}
	}
	else if (IsSteamAvailableForAuth())
	{
		SteamUser()->EndAuthSession(CSteamID(SteamId));
	}
#endif
}

void FOnlineAuthExtendedSteam::HandleWebApiTicketResponse(uint32 AuthTicketHandle, bool bSuccess, const FString& HexTicket)
{
	FOnExtendedSteamWebApiTicket Delegate;
	bool bFound = false;
	{
		FScopeLock Lock(&AuthLock);
		bFound = PendingWebApiTickets.RemoveAndCopyValue(AuthTicketHandle, Delegate);
	}

	if (bFound)
	{
		Delegate.ExecuteIfBound(bSuccess, HexTicket);
	}

	// Whoever's ticket this was, it is no longer pending, so a held request can go now.
	RetryDeferredWebApiRequests();
}
