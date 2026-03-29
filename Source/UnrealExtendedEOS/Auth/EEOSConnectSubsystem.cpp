// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSConnectSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "UnrealExtendedEOS.h"

#include "eos_connect.h"
#include "eos_connect_types.h"
#include "eos_sdk.h"

void UEEOSConnectSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UEEOSConnectSubsystem::Deinitialize()
{
	bIsConnected = false;
	CachedProductUserId.Empty();
	CachedDeviceDisplayName.Empty();
	Super::Deinitialize();
}

void UEEOSConnectSubsystem::ConnectLogin()
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("ConnectLogin"));
		OnConnectLoginComplete.Broadcast(false, TEXT(""));
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem::ConnectLogin — Identity interface not available"));
		OnConnectLoginComplete.Broadcast(false, TEXT(""));
		return;
	}

	// The EOS OnlineSubsystem handles Connect login through the Identity interface
	// After Auth login, the OSS automatically creates a Product User ID
	FUniqueNetIdPtr UserId = IdentityInterface->GetUniquePlayerId(0);
	if (UserId.IsValid())
	{
		CachedProductUserId = UserId->ToString();
		bIsConnected = true;
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem::ConnectLogin — Connected with Product User ID: %s"), *CachedProductUserId);
		OnConnectLoginComplete.Broadcast(true, CachedProductUserId);
	}
	else
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSConnectSubsystem::ConnectLogin — No active login found, please login via Auth first"));
		OnConnectLoginComplete.Broadcast(false, TEXT(""));
	}
}

void UEEOSConnectSubsystem::CreateDeviceId()
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("CreateDeviceId"));
		OnDeviceIdCreated.Broadcast(false);
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid())
	{
		OnDeviceIdCreated.Broadcast(false);
		return;
	}

	// Create credentials for Device ID login
	FOnlineAccountCredentials Credentials;
	Credentials.Type = TEXT("deviceid");
	Credentials.Id = FPlatformProcess::ComputerName();
	Credentials.Token = TEXT("");

	// Remove any previous login delegate to prevent accumulation
	if (LoginCompleteDelegateHandle.IsValid())
	{
		IdentityInterface->OnLoginCompleteDelegates->Remove(LoginCompleteDelegateHandle);
	}

	LoginCompleteDelegateHandle = IdentityInterface->OnLoginCompleteDelegates->AddLambda(
		[this, IdentityInterface](int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& NewUserId, const FString& ErrorStr)
		{
			// Remove ourselves to prevent accumulation on next call
			IdentityInterface->OnLoginCompleteDelegates->Remove(LoginCompleteDelegateHandle);
			LoginCompleteDelegateHandle.Reset();

			if (bWasSuccessful)
			{
				CachedProductUserId = NewUserId.ToString();
				bIsConnected = true;
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem: Device ID created and logged in: %s"), *CachedProductUserId);
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSConnectSubsystem: Device ID creation failed — %s"), *ErrorStr);
			}
			OnDeviceIdCreated.Broadcast(bWasSuccessful);
		});

	IdentityInterface->Login(0, Credentials);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem::CreateDeviceId — Creating device ID..."));
}

void UEEOSConnectSubsystem::DeleteDeviceId()
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("DeleteDeviceId"));
		OnDeviceIdDeleted.Broadcast(false);
		return;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem::DeleteDeviceId — Platform handle not available"));
		OnDeviceIdDeleted.Broadcast(false);
		return;
	}

	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(PlatformHandle);
	if (!ConnectHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem::DeleteDeviceId — Connect interface not available"));
		OnDeviceIdDeleted.Broadcast(false);
		return;
	}

	EOS_Connect_DeleteDeviceIdOptions Options = {};
	Options.ApiVersion = EOS_CONNECT_DELETEDEVICEID_API_LATEST;

	// Store weak ref for the static callback
	TWeakObjectPtr<UEEOSConnectSubsystem> WeakThis(this);
	EOS_Connect_DeleteDeviceId(ConnectHandle, &Options, this,
		[](const EOS_Connect_DeleteDeviceIdCallbackInfo* Data)
		{
			UEEOSConnectSubsystem* Self = static_cast<UEEOSConnectSubsystem*>(Data->ClientData);
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
}

void UEEOSConnectSubsystem::LoginWithDeviceId(const FString& DisplayName)
{
	CachedDeviceDisplayName = DisplayName;

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("LoginWithDeviceId"));
		OnConnectLoginComplete.Broadcast(false, TEXT(""));
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid())
	{
		OnConnectLoginComplete.Broadcast(false, TEXT(""));
		return;
	}

	FOnlineAccountCredentials Credentials;
	Credentials.Type = TEXT("deviceid");
	Credentials.Id = DisplayName;
	Credentials.Token = TEXT("");

	// Remove any previous login delegate to prevent accumulation
	if (LoginCompleteDelegateHandle.IsValid())
	{
		IdentityInterface->OnLoginCompleteDelegates->Remove(LoginCompleteDelegateHandle);
	}

	LoginCompleteDelegateHandle = IdentityInterface->OnLoginCompleteDelegates->AddLambda(
		[this, IdentityInterface](int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& NewUserId, const FString& ErrorStr)
		{
			// Remove ourselves to prevent accumulation on next call
			IdentityInterface->OnLoginCompleteDelegates->Remove(LoginCompleteDelegateHandle);
			LoginCompleteDelegateHandle.Reset();

			if (bWasSuccessful)
			{
				CachedProductUserId = NewUserId.ToString();
				bIsConnected = true;
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem: Device ID login succeeded: %s"), *CachedProductUserId);
				OnConnectLoginComplete.Broadcast(true, CachedProductUserId);
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSConnectSubsystem: Device ID login failed — %s"), *ErrorStr);
				OnConnectLoginComplete.Broadcast(false, TEXT(""));
			}
		});

	IdentityInterface->Login(0, Credentials);
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem::LoginWithDeviceId — Logging in as '%s'..."), *DisplayName);
}

void UEEOSConnectSubsystem::LinkAccount(EEOSExternalCredentialType CredentialType, const FString& Token)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("LinkAccount"));
		OnAccountLinked.Broadcast(false);
		return;
	}

	if (!CachedContinuanceToken)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem::LinkAccount — No ContinuanceToken available. "
			"A ContinuanceToken is obtained when a Connect login returns EOS_InvalidUser. "
			"Listen for OnInvalidUserDetected and call LinkAccount within that flow."));
		OnAccountLinked.Broadcast(false);
		return;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem::LinkAccount — Platform handle not available"));
		OnAccountLinked.Broadcast(false);
		return;
	}

	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(PlatformHandle);
	if (!ConnectHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem::LinkAccount — Connect interface not available"));
		OnAccountLinked.Broadcast(false);
		return;
	}

	// Get current user's Product User ID (may be null if linking creates a new user)
	EOS_ProductUserId LocalPUID = nullptr;
	if (!CachedProductUserId.IsEmpty())
	{
		LocalPUID = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*CachedProductUserId));
	}

	EOS_Connect_LinkAccountOptions Options = {};
	Options.ApiVersion = EOS_CONNECT_LINKACCOUNT_API_LATEST;
	Options.ContinuanceToken = CachedContinuanceToken;
	Options.LocalUserId = LocalPUID;

	EOS_Connect_LinkAccount(ConnectHandle, &Options, this,
		[](const EOS_Connect_LinkAccountCallbackInfo* Data)
		{
			UEEOSConnectSubsystem* Self = static_cast<UEEOSConnectSubsystem*>(Data->ClientData);
			if (!Self) return;

			// ContinuanceToken is consumed — clear it regardless of result
			Self->CachedContinuanceToken = nullptr;

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
}

void UEEOSConnectSubsystem::UnlinkAccount(EEOSExternalCredentialType CredentialType)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("UnlinkAccount"));
		OnAccountUnlinked.Broadcast(false);
		return;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem::UnlinkAccount — Platform handle not available"));
		OnAccountUnlinked.Broadcast(false);
		return;
	}

	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(PlatformHandle);
	if (!ConnectHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem::UnlinkAccount — Connect interface not available"));
		OnAccountUnlinked.Broadcast(false);
		return;
	}

	// Get the logged-in product user ID
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid() || !IdentityInterface->GetUniquePlayerId(0).IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem::UnlinkAccount — No logged in user"));
		OnAccountUnlinked.Broadcast(false);
		return;
	}

	// Note: EOS_Connect_UnlinkAccount requires the EOS_ProductUserId handle, not the string.
	// The OSS abstraction doesn't expose the raw handle, so we need to get it from the Connect interface.
	FString UserIdStr = IdentityInterface->GetUniquePlayerId(0)->ToString();
	EOS_ProductUserId ProductUserId = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*UserIdStr));
	if (!EOS_ProductUserId_IsValid(ProductUserId))
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem::UnlinkAccount — Invalid Product User ID"));
		OnAccountUnlinked.Broadcast(false);
		return;
	}

	EOS_Connect_UnlinkAccountOptions Options = {};
	Options.ApiVersion = EOS_CONNECT_UNLINKACCOUNT_API_LATEST;
	Options.LocalUserId = ProductUserId;

	EOS_Connect_UnlinkAccount(ConnectHandle, &Options, this,
		[](const EOS_Connect_UnlinkAccountCallbackInfo* Data)
		{
			UEEOSConnectSubsystem* Self = static_cast<UEEOSConnectSubsystem*>(Data->ClientData);
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
}

void UEEOSConnectSubsystem::TransferDeviceIdAccount(const FString& RealProductUserId)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("TransferDeviceIdAccount"));
		return;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem::TransferDeviceIdAccount — Platform handle not available"));
		return;
	}

	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(PlatformHandle);
	if (!ConnectHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem::TransferDeviceIdAccount — Connect interface not available"));
		return;
	}

	EOS_ProductUserId TargetUserId = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*RealProductUserId));
	if (!EOS_ProductUserId_IsValid(TargetUserId))
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSConnectSubsystem::TransferDeviceIdAccount — Invalid target Product User ID: %s"), *RealProductUserId);
		return;
	}

	EOS_Connect_TransferDeviceIdAccountOptions Options = {};
	Options.ApiVersion = EOS_CONNECT_TRANSFERDEVICEIDACCOUNT_API_LATEST;
	// The primary user is the one currently logged in with device ID
	// The target user is the "real" account to transfer data to
	EOS_ProductUserId LocalUserId = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*CachedProductUserId));
	Options.PrimaryLocalUserId = LocalUserId;
	Options.ProductUserIdToPreserve = TargetUserId;

	EOS_Connect_TransferDeviceIdAccount(ConnectHandle, &Options, this,
		[](const EOS_Connect_TransferDeviceIdAccountCallbackInfo* Data)
		{
			UEEOSConnectSubsystem* Self = static_cast<UEEOSConnectSubsystem*>(Data->ClientData);
			if (!Self) return;

			if (Data->ResultCode == EOS_EResult::EOS_Success)
			{
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem::TransferDeviceIdAccount — Transfer successful"));
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSConnectSubsystem::TransferDeviceIdAccount — Failed: %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			}
		});

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem::TransferDeviceIdAccount — Transferring device account to '%s'..."), *RealProductUserId);
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
