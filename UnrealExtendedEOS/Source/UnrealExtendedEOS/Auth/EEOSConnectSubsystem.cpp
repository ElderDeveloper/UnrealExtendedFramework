// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSConnectSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Shared/EEOSBlueprintLibrary.h"
#include "UnrealExtendedEOS.h"

#include "eos_connect.h"
#include "eos_connect_types.h"
#include "eos_sdk.h"

/** Heap context passed as EOS ClientData — the EOS platform outlives this subsystem,
 *  so callbacks must resolve a weak pointer instead of touching a raw `this`. */
struct FEEOSConnectCallbackContext
{
	TWeakObjectPtr<UEEOSConnectSubsystem> Self;
};

void UEEOSConnectSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEEOSConnectSubsystem::Deinitialize()
{
	// Clear any pending login delegate so the identity interface doesn't hold a stale binding
	if (LoginCompleteDelegateHandle.IsValid())
	{
		if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
		{
			if (IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface())
			{
				IdentityInterface->OnLoginCompleteDelegates->Remove(LoginCompleteDelegateHandle);
			}
		}
		LoginCompleteDelegateHandle.Reset();
	}

	bIsConnected = false;
	CachedProductUserId.Empty();
	CachedDeviceDisplayName.Empty();
	Super::Deinitialize();
}

bool UEEOSConnectSubsystem::ConnectLogin()
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("ConnectLogin"));
		OnConnectLoginComplete.Broadcast(false, TEXT(""));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem::ConnectLogin — Identity interface not available"));
		OnConnectLoginComplete.Broadcast(false, TEXT(""));
		return false;
	}

	// The EOS OnlineSubsystem handles Connect login through the Identity interface
	// After Auth login, the OSS automatically creates a Product User ID.
	// ToString() is the composite "<EpicAccountId>|<ProductUserId>" — cache only the PUID half.
	FUniqueNetIdPtr UserId = IdentityInterface->GetUniquePlayerId(0);
	const FString ProductUserId = UserId.IsValid() ? UEEOSBlueprintLibrary::ExtractProductUserId(UserId->ToString()) : FString();
	if (!ProductUserId.IsEmpty())
	{
		CachedProductUserId = ProductUserId;
		bIsConnected = true;
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem::ConnectLogin — Connected with Product User ID: %s"), *CachedProductUserId);
		OnConnectLoginComplete.Broadcast(true, CachedProductUserId);
		return true;
	}

	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSConnectSubsystem::ConnectLogin — No Connect session (no Product User ID) found, please login via Auth first"));
	OnConnectLoginComplete.Broadcast(false, TEXT(""));
	return false;
}

bool UEEOSConnectSubsystem::CreateDeviceId()
{
	// In-flight guard FIRST (R1): a device-id identity login is already pending (from
	// CreateDeviceId or LoginWithDeviceId — they share the delegate slot). A rejection
	// must not echo on the shared delegates; no delegate fires for this call.
	if (LoginCompleteDelegateHandle.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSConnectSubsystem::CreateDeviceId — A device-id login/creation is already in progress, rejecting this call (no delegate will fire for it)"));
		return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("CreateDeviceId"));
		OnDeviceIdCreated.Broadcast(false);
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid())
	{
		OnDeviceIdCreated.Broadcast(false);
		return false;
	}

	// Create credentials for Device ID login
	FOnlineAccountCredentials Credentials;
	Credentials.Type = TEXT("deviceid");
	Credentials.Id = FPlatformProcess::ComputerName();
	Credentials.Token = TEXT("");

	LoginCompleteDelegateHandle = IdentityInterface->OnLoginCompleteDelegates->AddWeakLambda(this,
		[this, IdentityInterface](int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& NewUserId, const FString& ErrorStr)
		{
			// Remove ourselves to prevent accumulation on next call
			IdentityInterface->OnLoginCompleteDelegates->Remove(LoginCompleteDelegateHandle);
			LoginCompleteDelegateHandle.Reset();

			// ToString() is the composite "<EpicAccountId>|<ProductUserId>" — cache only the PUID half
			const FString ProductUserId = bWasSuccessful ? UEEOSBlueprintLibrary::ExtractProductUserId(NewUserId.ToString()) : FString();
			const bool bSuccess = bWasSuccessful && !ProductUserId.IsEmpty();
			if (bSuccess)
			{
				CachedProductUserId = ProductUserId;
				bIsConnected = true;
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem: Device ID created and logged in: %s"), *CachedProductUserId);
			}
			else if (bWasSuccessful)
			{
				UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem: Device ID login reported success but the net id '%s' has no Product User ID"), *NewUserId.ToString());
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSConnectSubsystem: Device ID creation failed — %s"), *ErrorStr);
			}
			OnDeviceIdCreated.Broadcast(bSuccess);
		});

	IdentityInterface->Login(0, Credentials);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem::CreateDeviceId — Creating device ID..."));
	return true;
}

bool UEEOSConnectSubsystem::DeleteDeviceId()
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("DeleteDeviceId"));
		OnDeviceIdDeleted.Broadcast(false);
		return false;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem::DeleteDeviceId — Platform handle not available"));
		OnDeviceIdDeleted.Broadcast(false);
		return false;
	}

	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(PlatformHandle);
	if (!ConnectHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem::DeleteDeviceId — Connect interface not available"));
		OnDeviceIdDeleted.Broadcast(false);
		return false;
	}

	EOS_Connect_DeleteDeviceIdOptions Options = {};
	Options.ApiVersion = EOS_CONNECT_DELETEDEVICEID_API_LATEST;

	// Store weak ref for the static callback — the EOS platform outlives this subsystem
	TWeakObjectPtr<UEEOSConnectSubsystem> WeakThis(this);
	EOS_Connect_DeleteDeviceId(ConnectHandle, &Options, new FEEOSConnectCallbackContext{WeakThis},
		[](const EOS_Connect_DeleteDeviceIdCallbackInfo* Data)
		{
			TUniquePtr<FEEOSConnectCallbackContext> Ctx(static_cast<FEEOSConnectCallbackContext*>(Data->ClientData));
			UEEOSConnectSubsystem* Self = Ctx.IsValid() ? Ctx->Self.Get() : nullptr;
			if (!Self) return;

			const bool bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);
			if (bSuccess)
			{
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem::DeleteDeviceId — Device ID deleted successfully"));
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSConnectSubsystem::DeleteDeviceId — Failed: %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			}
			Self->OnDeviceIdDeleted.Broadcast(bSuccess);
		});

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem::DeleteDeviceId — Deleting device ID..."));
	return true;
}

bool UEEOSConnectSubsystem::LoginWithDeviceId(const FString& DisplayName)
{
	// In-flight guard FIRST (R1): a device-id identity login is already pending (from
	// CreateDeviceId or LoginWithDeviceId — they share the delegate slot). A rejection
	// must not echo on the shared delegates; no delegate fires for this call.
	if (LoginCompleteDelegateHandle.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSConnectSubsystem::LoginWithDeviceId — A device-id login/creation is already in progress, rejecting this call (no delegate will fire for it)"));
		return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("LoginWithDeviceId"));
		OnConnectLoginComplete.Broadcast(false, TEXT(""));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid())
	{
		OnConnectLoginComplete.Broadcast(false, TEXT(""));
		return false;
	}

	CachedDeviceDisplayName = DisplayName;

	FOnlineAccountCredentials Credentials;
	Credentials.Type = TEXT("deviceid");
	Credentials.Id = DisplayName;
	Credentials.Token = TEXT("");

	LoginCompleteDelegateHandle = IdentityInterface->OnLoginCompleteDelegates->AddWeakLambda(this,
		[this, IdentityInterface](int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& NewUserId, const FString& ErrorStr)
		{
			// Remove ourselves to prevent accumulation on next call
			IdentityInterface->OnLoginCompleteDelegates->Remove(LoginCompleteDelegateHandle);
			LoginCompleteDelegateHandle.Reset();

			// ToString() is the composite "<EpicAccountId>|<ProductUserId>" — cache only the PUID half
			const FString ProductUserId = bWasSuccessful ? UEEOSBlueprintLibrary::ExtractProductUserId(NewUserId.ToString()) : FString();
			if (bWasSuccessful && !ProductUserId.IsEmpty())
			{
				CachedProductUserId = ProductUserId;
				bIsConnected = true;
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem: Device ID login succeeded: %s"), *CachedProductUserId);
				OnConnectLoginComplete.Broadcast(true, CachedProductUserId);
			}
			else if (bWasSuccessful)
			{
				UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem: Device ID login reported success but the net id '%s' has no Product User ID"), *NewUserId.ToString());
				OnConnectLoginComplete.Broadcast(false, TEXT(""));
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSConnectSubsystem: Device ID login failed — %s"), *ErrorStr);
				OnConnectLoginComplete.Broadcast(false, TEXT(""));
			}
		});

	IdentityInterface->Login(0, Credentials);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem::LoginWithDeviceId — Logging in as '%s'..."), *DisplayName);
	return true;
}

bool UEEOSConnectSubsystem::LinkAccount(EEOSExternalCredentialType CredentialType, const FString& Token)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("LinkAccount"));
		OnAccountLinked.Broadcast(false);
		return false;
	}

	if (!CachedContinuanceToken)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem::LinkAccount — No ContinuanceToken available. "
			"A ContinuanceToken is obtained when a Connect login returns EOS_InvalidUser. "
			"Listen for OnInvalidUserDetected and call LinkAccount within that flow."));
		OnAccountLinked.Broadcast(false);
		return false;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem::LinkAccount — Platform handle not available"));
		OnAccountLinked.Broadcast(false);
		return false;
	}

	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(PlatformHandle);
	if (!ConnectHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem::LinkAccount — Connect interface not available"));
		OnAccountLinked.Broadcast(false);
		return false;
	}

	// Get current user's Product User ID (may be null if linking creates a new user)
	EOS_ProductUserId LocalPUID = nullptr;
	if (!CachedProductUserId.IsEmpty())
	{
		LocalPUID = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*CachedProductUserId));
	}

	// The ContinuanceToken is single-use — consume it NOW (move into a local and clear
	// the cache before the SDK call) so a fresh token stored mid-flight by a new
	// EOS_InvalidUser login can never be clobbered when this call's callback lands (m4).
	const EOS_ContinuanceToken ConsumedToken = CachedContinuanceToken;
	CachedContinuanceToken = nullptr;

	EOS_Connect_LinkAccountOptions Options = {};
	Options.ApiVersion = EOS_CONNECT_LINKACCOUNT_API_LATEST;
	Options.ContinuanceToken = ConsumedToken;
	Options.LocalUserId = LocalPUID;

	EOS_Connect_LinkAccount(ConnectHandle, &Options, new FEEOSConnectCallbackContext{this},
		[](const EOS_Connect_LinkAccountCallbackInfo* Data)
		{
			TUniquePtr<FEEOSConnectCallbackContext> Ctx(static_cast<FEEOSConnectCallbackContext*>(Data->ClientData));
			UEEOSConnectSubsystem* Self = Ctx.IsValid() ? Ctx->Self.Get() : nullptr;
			if (!Self) return;

			// The consumed ContinuanceToken was already cleared from the cache at call
			// time (single-use, m4) — do NOT clear the member here: it may already hold
			// a fresh token stored by a newer EOS_InvalidUser login.

			const bool bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);
			if (bSuccess)
			{
				// Update the cached Product User ID
				char PUIDBuffer[EOS_PRODUCTUSERID_MAX_LENGTH + 1];
				int32_t BufferSize = sizeof(PUIDBuffer);
				if (EOS_ProductUserId_ToString(Data->LocalUserId, PUIDBuffer, &BufferSize) == EOS_EResult::EOS_Success)
				{
					Self->CachedProductUserId = ANSI_TO_TCHAR(PUIDBuffer);
				}
				Self->bIsConnected = true;
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem::LinkAccount — Account linked successfully. PUID: %s"), *Self->CachedProductUserId);
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSConnectSubsystem::LinkAccount — Failed: %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			}
			Self->OnAccountLinked.Broadcast(bSuccess);
		});

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem::LinkAccount — Linking account (type=%d) with ContinuanceToken..."), static_cast<int32>(CredentialType));
	return true;
}

bool UEEOSConnectSubsystem::UnlinkAccount(EEOSExternalCredentialType CredentialType)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("UnlinkAccount"));
		OnAccountUnlinked.Broadcast(false);
		return false;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem::UnlinkAccount — Platform handle not available"));
		OnAccountUnlinked.Broadcast(false);
		return false;
	}

	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(PlatformHandle);
	if (!ConnectHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem::UnlinkAccount — Connect interface not available"));
		OnAccountUnlinked.Broadcast(false);
		return false;
	}

	// Get the logged-in product user ID
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid() || !IdentityInterface->GetUniquePlayerId(0).IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem::UnlinkAccount — No logged in user"));
		OnAccountUnlinked.Broadcast(false);
		return false;
	}

	// Note: EOS_Connect_UnlinkAccount requires the EOS_ProductUserId handle, not the string.
	// ToString() is the composite "<EpicAccountId>|<ProductUserId>" — extract the PUID half
	// before parsing. EOS_ProductUserId_FromString performs NO validation (any non-null string
	// yields a handle EOS_ProductUserId_IsValid accepts), so an empty extracted half is the
	// only reliable failure signal here.
	const FString ProductUserIdStr = UEEOSBlueprintLibrary::ExtractProductUserId(IdentityInterface->GetUniquePlayerId(0)->ToString());
	if (ProductUserIdStr.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem::UnlinkAccount — Logged-in user has no Product User ID (no Connect session)"));
		OnAccountUnlinked.Broadcast(false);
		return false;
	}
	EOS_ProductUserId ProductUserId = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*ProductUserIdStr));

	EOS_Connect_UnlinkAccountOptions Options = {};
	Options.ApiVersion = EOS_CONNECT_UNLINKACCOUNT_API_LATEST;
	Options.LocalUserId = ProductUserId;

	EOS_Connect_UnlinkAccount(ConnectHandle, &Options, new FEEOSConnectCallbackContext{this},
		[](const EOS_Connect_UnlinkAccountCallbackInfo* Data)
		{
			TUniquePtr<FEEOSConnectCallbackContext> Ctx(static_cast<FEEOSConnectCallbackContext*>(Data->ClientData));
			UEEOSConnectSubsystem* Self = Ctx.IsValid() ? Ctx->Self.Get() : nullptr;
			if (!Self) return;

			const bool bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);
			if (bSuccess)
			{
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem::UnlinkAccount — Account unlinked successfully"));
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSConnectSubsystem::UnlinkAccount — Failed: %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			}
			Self->OnAccountUnlinked.Broadcast(bSuccess);
		});

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem::UnlinkAccount — Unlinking account (type=%d)..."), static_cast<int32>(CredentialType));
	return true;
}

bool UEEOSConnectSubsystem::TransferDeviceIdAccount(const FString& DeviceIdProductUserId, const FString& ExternalProductUserId, bool bKeepExternalAccountProgression)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("TransferDeviceIdAccount"));
		OnDeviceIdAccountTransferred.Broadcast(false, TEXT(""));
		return false;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem::TransferDeviceIdAccount — Platform handle not available"));
		OnDeviceIdAccountTransferred.Broadcast(false, TEXT(""));
		return false;
	}

	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(PlatformHandle);
	if (!ConnectHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem::TransferDeviceIdAccount — Connect interface not available"));
		OnDeviceIdAccountTransferred.Broadcast(false, TEXT(""));
		return false;
	}

	// Accept either bare PUIDs or composite net-id strings; extraction is a no-op for bare ids.
	// EOS_ProductUserId_FromString performs NO validation, so guard on the strings instead.
	const FString DevicePUIDStr = UEEOSBlueprintLibrary::ExtractProductUserId(DeviceIdProductUserId);
	if (DevicePUIDStr.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem::TransferDeviceIdAccount — Invalid Device ID Product User ID: %s"), *DeviceIdProductUserId);
		OnDeviceIdAccountTransferred.Broadcast(false, TEXT(""));
		return false;
	}

	const FString ExternalPUIDStr = UEEOSBlueprintLibrary::ExtractProductUserId(ExternalProductUserId);
	if (ExternalPUIDStr.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem::TransferDeviceIdAccount — Invalid external-account Product User ID: %s"), *ExternalProductUserId);
		OnDeviceIdAccountTransferred.Broadcast(false, TEXT(""));
		return false;
	}

	const EOS_ProductUserId DeviceUserId = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*DevicePUIDStr));
	const EOS_ProductUserId ExternalUserId = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*ExternalPUIDStr));

	// Contract per eos_connect_types.h (EOS_Connect_TransferDeviceIdAccountOptions):
	// - PrimaryLocalUserId:      the logged-in user already associated with a REAL external account;
	//                            its keychain is preserved and receives the Device ID credentials.
	// - LocalDeviceUserId:       the logged-in user originally created via the anonymous Device ID login.
	// - ProductUserIdToPreserve: which of those two keeps its game progression; the other is
	//                            discarded forever.
	EOS_Connect_TransferDeviceIdAccountOptions Options = {};
	Options.ApiVersion = EOS_CONNECT_TRANSFERDEVICEIDACCOUNT_API_LATEST;
	Options.PrimaryLocalUserId = ExternalUserId;
	Options.LocalDeviceUserId = DeviceUserId;
	Options.ProductUserIdToPreserve = bKeepExternalAccountProgression ? ExternalUserId : DeviceUserId;

	EOS_Connect_TransferDeviceIdAccount(ConnectHandle, &Options, new FEEOSConnectCallbackContext{this},
		[](const EOS_Connect_TransferDeviceIdAccountCallbackInfo* Data)
		{
			TUniquePtr<FEEOSConnectCallbackContext> Ctx(static_cast<FEEOSConnectCallbackContext*>(Data->ClientData));
			UEEOSConnectSubsystem* Self = Ctx.IsValid() ? Ctx->Self.Get() : nullptr;
			if (!Self) return;

			if (Data->ResultCode == EOS_EResult::EOS_Success)
			{
				// Data->LocalUserId is the ProductUserIdToPreserve — it now owns the only
				// valid session; the other PUID is gone forever.
				FString PreservedPUID;
				char PUIDBuffer[EOS_PRODUCTUSERID_MAX_LENGTH + 1];
				int32_t BufferSize = sizeof(PUIDBuffer);
				if (EOS_ProductUserId_ToString(Data->LocalUserId, PUIDBuffer, &BufferSize) == EOS_EResult::EOS_Success)
				{
					PreservedPUID = ANSI_TO_TCHAR(PUIDBuffer);
				}
				else
				{
					// m5: the transfer DID succeed — broadcast success with an empty
					// preserved id rather than leaving state un-updated (documented edge
					// in the header).
					UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSConnectSubsystem::TransferDeviceIdAccount — Transfer succeeded but the preserved Product User ID could not be stringified; broadcasting success with an empty PreservedProductUserId"));
				}
				// Update cached state consistently with the success broadcast on BOTH
				// branches (m5): the old CachedProductUserId may describe the discarded
				// user, so an empty id is safer than a stale one.
				Self->CachedProductUserId = PreservedPUID;
				Self->bIsConnected = true;
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem::TransferDeviceIdAccount — Transfer successful, preserved PUID: %s"), *PreservedPUID);
				Self->OnDeviceIdAccountTransferred.Broadcast(true, PreservedPUID);
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSConnectSubsystem::TransferDeviceIdAccount — Failed: %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
				Self->OnDeviceIdAccountTransferred.Broadcast(false, TEXT(""));
			}
		});

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem::TransferDeviceIdAccount — Transferring Device ID login '%s' into external account user '%s' (preserving %s)..."),
		*DevicePUIDStr, *ExternalPUIDStr, bKeepExternalAccountProgression ? TEXT("external-account progression") : TEXT("device-id progression"));
	return true;
}

FString UEEOSConnectSubsystem::GetProductUserId() const
{
	return CachedProductUserId;
}

bool UEEOSConnectSubsystem::IsConnected() const
{
	return bIsConnected;
}

FString UEEOSConnectSubsystem::GetDeviceIdDisplayName() const
{
	return CachedDeviceDisplayName;
}

bool UEEOSConnectSubsystem::HasContinuanceToken() const
{
	return CachedContinuanceToken != nullptr;
}

void UEEOSConnectSubsystem::StoreContinuanceToken(EOS_ContinuanceToken Token)
{
	CachedContinuanceToken = Token;
	if (Token)
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem: ContinuanceToken stored — LinkAccount is now available"));
		OnInvalidUserDetected.Broadcast(true);
	}
}
