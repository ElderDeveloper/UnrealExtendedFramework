// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EncryptedAppTicket/OnlineEncryptedAppTicketExtendedSteam.h"
#include "Core/OnlineSubsystemExtendedSteam.h"
#include "ExtendedSteamSharedModule.h"
#include "Shared/ESteamSDK.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END
#endif

namespace ExtendedSteamEncryptedAppTicket
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
/**
 * Native Steam call-result listener for the encrypted app ticket interface; alive for the lifetime
 * of the owning FOnlineEncryptedAppTicketExtendedSteam. Tracks the single in-flight
 * RequestEncryptedAppTicket SteamAPICall_t and forwards its EncryptedAppTicketResponse_t.
 */
class FOnlineEncryptedAppTicketExtendedSteamCallbacks
{
public:
	explicit FOnlineEncryptedAppTicketExtendedSteamCallbacks(FOnlineEncryptedAppTicketExtendedSteam* InOwner)
		: Owner(InOwner)
	{
	}

	FOnlineEncryptedAppTicketExtendedSteamCallbacks(const FOnlineEncryptedAppTicketExtendedSteamCallbacks&) = delete;
	FOnlineEncryptedAppTicketExtendedSteamCallbacks& operator=(const FOnlineEncryptedAppTicketExtendedSteamCallbacks&) = delete;

	void TrackRequest(SteamAPICall_t Call)
	{
		RequestResult.Set(Call, this, &FOnlineEncryptedAppTicketExtendedSteamCallbacks::OnEncryptedAppTicketResponse);
	}

private:
	void OnEncryptedAppTicketResponse(EncryptedAppTicketResponse_t* Data, bool bIOFailure)
	{
		if (Owner == nullptr)
		{
			return;
		}
		Owner->HandleEncryptedAppTicketResponse(!bIOFailure && Data != nullptr && Data->m_eResult == k_EResultOK);
	}

	CCallResult<FOnlineEncryptedAppTicketExtendedSteamCallbacks, EncryptedAppTicketResponse_t> RequestResult;

	FOnlineEncryptedAppTicketExtendedSteam* Owner = nullptr;
};
#endif // WITH_EXTENDEDSTEAM_SDK

FOnlineEncryptedAppTicketExtendedSteam::FOnlineEncryptedAppTicketExtendedSteam(FOnlineSubsystemExtendedSteam* InSubsystem)
	: Subsystem(InSubsystem)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (ExtendedSteamEncryptedAppTicket::IsSteamClientUp())
	{
		Callbacks = MakeShared<FOnlineEncryptedAppTicketExtendedSteamCallbacks>(this);
	}
#endif
}

FOnlineEncryptedAppTicketExtendedSteam::~FOnlineEncryptedAppTicketExtendedSteam()
{
	// Fail a request still in flight so the caller is never left hanging.
	FOnExtendedSteamEncryptedAppTicket Orphaned;
	{
		FScopeLock Lock(&TicketLock);
		if (bRequestInFlight)
		{
			Orphaned = PendingRequestDelegate;
			PendingRequestDelegate.Unbind();
			bRequestInFlight = false;
		}
	}
	Orphaned.ExecuteIfBound(false, TArray<uint8>());
}

bool FOnlineEncryptedAppTicketExtendedSteam::IsSteamAvailableForTickets() const
{
#if WITH_EXTENDEDSTEAM_SDK
	return ExtendedSteamEncryptedAppTicket::IsSteamClientUp() && SteamUser() != nullptr;
#else
	return false;
#endif
}

bool FOnlineEncryptedAppTicketExtendedSteam::RequestEncryptedAppTicket(const TArray<uint8>& DataToInclude, const FOnExtendedSteamEncryptedAppTicket& Delegate)
{
#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailableForTickets() && Callbacks.IsValid())
	{
		{
			FScopeLock Lock(&TicketLock);
			if (bRequestInFlight)
			{
				UE_LOG(LogExtendedSteam, Warning, TEXT("RequestEncryptedAppTicket: a request is already in flight"));
				Delegate.ExecuteIfBound(false, TArray<uint8>());
				return false;
			}
			bRequestInFlight = true;
			PendingRequestDelegate = Delegate;
		}

		// Steam copies the payload; a const_cast is safe because RequestEncryptedAppTicket only reads it.
		void* PayloadPtr = DataToInclude.Num() > 0 ? const_cast<uint8*>(DataToInclude.GetData()) : nullptr;
		const SteamAPICall_t Call = SteamUser()->RequestEncryptedAppTicket(PayloadPtr, DataToInclude.Num());
		if (Call == k_uAPICallInvalid)
		{
			{
				FScopeLock Lock(&TicketLock);
				bRequestInFlight = false;
				PendingRequestDelegate.Unbind();
			}
			UE_LOG(LogExtendedSteam, Warning, TEXT("RequestEncryptedAppTicket: Steam refused the request"));
			Delegate.ExecuteIfBound(false, TArray<uint8>());
			return false;
		}

		Callbacks->TrackRequest(Call);
		return true;
	}
#endif

	UE_LOG(LogExtendedSteam, Verbose, TEXT("RequestEncryptedAppTicket: Steam unavailable"));
	Delegate.ExecuteIfBound(false, TArray<uint8>());
	return false;
}

bool FOnlineEncryptedAppTicketExtendedSteam::GetEncryptedAppTicket(TArray<uint8>& OutTicket) const
{
	OutTicket.Reset();

#if WITH_EXTENDEDSTEAM_SDK
	if (IsSteamAvailableForTickets())
	{
		// Steam encrypted app tickets are small; a fixed buffer comfortably exceeds the maximum.
		uint8 TicketBuffer[2048];
		uint32 TicketSize = 0;
		if (SteamUser()->GetEncryptedAppTicket(TicketBuffer, sizeof(TicketBuffer), &TicketSize) && TicketSize > 0)
		{
			OutTicket.Append(TicketBuffer, static_cast<int32>(TicketSize));
			return true;
		}
	}
#endif
	return false;
}

void FOnlineEncryptedAppTicketExtendedSteam::HandleEncryptedAppTicketResponse(bool bSuccess)
{
	FOnExtendedSteamEncryptedAppTicket Delegate;
	{
		FScopeLock Lock(&TicketLock);
		if (!bRequestInFlight)
		{
			return;
		}
		Delegate = PendingRequestDelegate;
		PendingRequestDelegate.Unbind();
		bRequestInFlight = false;
	}

	TArray<uint8> Ticket;
	const bool bTicketRead = bSuccess && GetEncryptedAppTicket(Ticket);
	Delegate.ExecuteIfBound(bTicketRead, Ticket);
}
