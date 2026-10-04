// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSConnectSubsystem.h"
#include "Shared/EEOSNativeOperation.h"
#include "Shared/EEOSIdentityUtils.h"
#include "OnlineSubsystemUtils.h"
#include "Shared/EEOSBlueprintLibrary.h"
#include "UnrealExtendedEOS.h"

#include "eos_connect.h"
#include "eos_connect_types.h"
#include "eos_sdk.h"

/** Keeps the originating SDK platform alive until the terminal callback; UObject ownership stays weak. */
struct FEEOSConnectCallbackContext
{
	TWeakObjectPtr<UEEOSConnectSubsystem> Self;
	TSharedPtr<IEOSPlatformHandle, ESPMode::ThreadSafe> Platform;
	FEEOSRequestContext Context;
	FEEOSNativeOperationLease Lease;
	int64 RequestId = 0;
	FName Operation;
};

void UEEOSConnectSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LastIdentityGeneration = CaptureEOSContext().Generation;
	ConnectWatcher = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UEEOSConnectSubsystem::TickConnectIdentity), 0.5f);
}

void UEEOSConnectSubsystem::Deinitialize()
{
	BeginEOSShutdown(); bShuttingDown = true;
	if (ConnectWatcher.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(ConnectWatcher);
	ConnectWatcher.Reset(); CancelSDKMutation();
	CachedContinuanceToken = nullptr;
	EEOSIdentity::FRetired::Hold(OperationIdentity, IdentityLease, GetOwningEOSInstanceName(), false);
	if (OperationIdentity.IsValid()) OperationIdentity->ClearOnLoginCompleteDelegate_Handle(0, LoginCompleteDelegateHandle);
	IdentityLease.Reset(); OperationIdentity.Reset();
	LoginCompleteDelegateHandle.Reset();

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
		FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("ConnectLogin"), TEXT("CapabilityUnavailable"));
		OnConnectLoginComplete.Broadcast(false, TEXT(""));
		return false;
	}

	// The EOS OnlineSubsystem handles Connect login through the Identity interface
	// After Auth login, the OSS automatically creates a Product User ID.
	// ToString() is the composite "<EpicAccountId>|<ProductUserId>" — cache only the PUID half.
	FUniqueNetIdPtr UserId = IdentityInterface->GetUniquePlayerId(0);
	const FString ProductUserId = UserId.IsValid() && UserId->IsValid() && IdentityInterface->GetLoginStatus(0) == ELoginStatus::LoggedIn ? UEEOSBlueprintLibrary::ExtractProductUserId(UserId->ToString()) : FString();
	if (!ProductUserId.IsEmpty() && GetEOSReadiness().bConnectLoggedIn)
	{
		CachedProductUserId = ProductUserId;
		bIsConnected = true;
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem::ConnectLogin — Connected with Product User ID: %s"), *FEEOSNativeOperationLease::SafeField(CachedProductUserId));
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
	if (bShuttingDown || IdentityLease.IsValid() || LoginCompleteDelegateHandle.IsValid())
	{
		RejectOperation(TEXT("CreateDeviceId"), EEOSOperationCode::Busy, TEXT("Another admitted request already owns this operation."));
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

	if (!IdentityLease.TryAcquire(IdentityInterface.Get(), TEXT("Identity0"), this, TEXT("CreateDeviceId")))
	{
		RejectOperation(TEXT("CreateDeviceId"), EEOSOperationCode::Busy, TEXT("Another plugin identity operation is pending.")); return false;
	}
	OperationIdentity = IdentityInterface; NativeLoginContext = CaptureEOSContext();
	const int64 Request = BeginOperation(TEXT("CreateDeviceId"), FString(), IdentityLease.GetRequestId());
	LoginCompleteDelegateHandle = IdentityInterface->OnLoginCompleteDelegates->AddWeakLambda(this,
		[this, IdentityInterface, Request](int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& NewUserId, const FString& ErrorStr)
		{
			if (bShuttingDown || LocalUserNum != 0 || !LoginCompleteDelegateHandle.IsValid() || IdentityLease.GetRequestId() != Request)
			{ LogCallbackDisposition(TEXT("DeviceLogin"), Request, bShuttingDown ? TEXT("ShutdownInternalOnly") : TEXT("DifferentOwner")); return; }
			const bool bCurrent = IsEOSContextCurrent(NativeLoginContext, false);
			LogCallbackDisposition(TEXT("DeviceLogin"), Request, bCurrent ? TEXT("Consumed") : TEXT("StaleGeneration"), NativeLoginContext.Generation);
			// Remove ourselves to prevent accumulation on next call
			IdentityInterface->OnLoginCompleteDelegates->Remove(LoginCompleteDelegateHandle);
			LoginCompleteDelegateHandle.Reset(); IdentityLease.Reset(); OperationIdentity.Reset();

			// ToString() is the composite "<EpicAccountId>|<ProductUserId>" — cache only the PUID half
			const FString ProductUserId = bWasSuccessful ? UEEOSBlueprintLibrary::ExtractProductUserId(NewUserId.ToString()) : FString();
			const auto NativeLocal = IdentityInterface->GetUniquePlayerId(0);
			const bool bReady = bCurrent && NativeLocal.IsValid() && *NativeLocal == NewUserId && bWasSuccessful && NewUserId.IsValid() && IdentityInterface->GetLoginStatus(0) == ELoginStatus::LoggedIn && !ProductUserId.IsEmpty();
			const auto Outcome = CompleteOperation(TEXT("CreateDeviceId"), bReady, !bCurrent ? EEOSOperationCode::Canceled : bReady ? EEOSOperationCode::Succeeded : EEOSOperationCode::NativeFailure, !bCurrent ? TEXT("Original identity/platform retired.") : ErrorStr.IsEmpty() && !bReady ? TEXT("Device login completed without a usable Product User ID.") : ErrorStr, FString(), bWasSuccessful ? TEXT("Success") : TEXT("Failure"), EEOSResultSource::NativeCallback);
			FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
			const bool bSuccess = bReady;
			if (bSuccess)
			{
				LastIdentityGeneration = CaptureEOSContext().Generation;
				CachedProductUserId = ProductUserId;
				bIsConnected = true;
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem: Device ID created and logged in: %s"), *FEEOSNativeOperationLease::SafeField(CachedProductUserId));
			}
			else if (bWasSuccessful)
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSConnectSubsystem: Device ID login reported success but the net id '%s' has no Product User ID"), *FEEOSNativeOperationLease::SafeField(NewUserId.ToString()));
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSConnectSubsystem: Device ID creation failed — %s"), *FEEOSNativeOperationLease::SafeField(ErrorStr));
			}
			OnDeviceIdCreated.Broadcast(bSuccess); OnOperationCompleted.Broadcast(Outcome);
		});

	const bool bStarted = IdentityInterface->Login(0, Credentials);
	if (!bStarted && LoginCompleteDelegateHandle.IsValid() && IdentityLease.GetRequestId() == Request)
	{
		IdentityInterface->ClearOnLoginCompleteDelegate_Handle(0, LoginCompleteDelegateHandle);
		LoginCompleteDelegateHandle.Reset(); IdentityLease.Reset(); OperationIdentity.Reset();
		const auto Outcome = CompleteOperation(TEXT("CreateDeviceId"), false, EEOSOperationCode::NativeStartRejected, TEXT("Native device login refused submission."));
		FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
		OnDeviceIdCreated.Broadcast(false); OnOperationCompleted.Broadcast(Outcome);
	}
	if (!bStarted) return false;
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem::CreateDeviceId — Creating device ID..."));
	return true;
}

bool UEEOSConnectSubsystem::DeleteDeviceId()
{
	if (bShuttingDown) return false;
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("DeleteDeviceId"));
		OnDeviceIdDeleted.Broadcast(false);
		return false;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("DeleteDeviceId"), TEXT("CapabilityUnavailable"));
		OnDeviceIdDeleted.Broadcast(false);
		return false;
	}

	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(PlatformHandle);
	if (!ConnectHandle)
	{
		FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("DeleteDeviceId"), TEXT("CapabilityUnavailable"));
		OnDeviceIdDeleted.Broadcast(false);
		return false;
	}

	EOS_Connect_DeleteDeviceIdOptions Options = {};
	Options.ApiVersion = EOS_CONNECT_DELETEDEVICEID_API_LATEST;

	// Store weak ref for the static callback — the EOS platform outlives this subsystem
	TWeakObjectPtr<UEEOSConnectSubsystem> WeakThis(this);
	auto* Context = BeginSDKMutation(TEXT("DeleteDeviceId"));
	if (!Context) return false;
	EOS_Connect_DeleteDeviceId(ConnectHandle, &Options, Context,
		[](const EOS_Connect_DeleteDeviceIdCallbackInfo* Data)
		{
			if (!Data || !EOS_EResult_IsOperationComplete(Data->ResultCode)) return;
			TUniquePtr<FEEOSConnectCallbackContext> Ctx(static_cast<FEEOSConnectCallbackContext*>(Data->ClientData));
			UEEOSConnectSubsystem* Self = Ctx.IsValid() ? Ctx->Self.Get() : nullptr;
			if (!Self || Self->bShuttingDown || Self->ActiveSDKMutation != Ctx->RequestId) return;
			if (!Self->IsEOSContextCurrent(Ctx->Context))
			{ Self->LogCallbackDisposition(Ctx->Operation, Ctx->RequestId, TEXT("StaleGeneration"), Ctx->Context.Generation); Ctx->Lease.Reset(); Self->CancelSDKMutation(); return; }
			Ctx->Lease.Reset();
			const auto Outcome = Self->FinishSDKMutation(Data->ResultCode == EOS_EResult::EOS_Success, ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			FEEOSOutcomeDispatchScope Dispatch(Self, Outcome);

			const bool bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);
			if (bSuccess)
			{
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem::DeleteDeviceId — Device ID deleted successfully"));
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSConnectSubsystem::DeleteDeviceId — Failed: %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			}
			Self->OnDeviceIdDeleted.Broadcast(bSuccess); Self->OnOperationCompleted.Broadcast(Outcome);
		});

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem::DeleteDeviceId — Deleting device ID..."));
	return true;
}

bool UEEOSConnectSubsystem::LoginWithDeviceId(const FString& DisplayName)
{
	// In-flight guard FIRST (R1): a device-id identity login is already pending (from
	// CreateDeviceId or LoginWithDeviceId — they share the delegate slot). A rejection
	// must not echo on the shared delegates; no delegate fires for this call.
	if (bShuttingDown || IdentityLease.IsValid() || LoginCompleteDelegateHandle.IsValid())
	{
		RejectOperation(TEXT("LoginWithDeviceId"), EEOSOperationCode::Busy, TEXT("Another admitted request already owns this operation."));
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

	if (!IdentityLease.TryAcquire(IdentityInterface.Get(), TEXT("Identity0"), this, TEXT("LoginWithDeviceId")))
	{
		RejectOperation(TEXT("LoginWithDeviceId"), EEOSOperationCode::Busy, TEXT("Another plugin identity operation is pending.")); return false;
	}
	OperationIdentity = IdentityInterface; NativeLoginContext = CaptureEOSContext();
	const int64 Request = BeginOperation(TEXT("LoginWithDeviceId"), FString(), IdentityLease.GetRequestId());
	LoginCompleteDelegateHandle = IdentityInterface->OnLoginCompleteDelegates->AddWeakLambda(this,
		[this, IdentityInterface, Request](int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& NewUserId, const FString& ErrorStr)
		{
			if (bShuttingDown || LocalUserNum != 0 || !LoginCompleteDelegateHandle.IsValid() || IdentityLease.GetRequestId() != Request)
			{ LogCallbackDisposition(TEXT("DeviceLogin"), Request, bShuttingDown ? TEXT("ShutdownInternalOnly") : TEXT("DifferentOwner")); return; }
			const bool bCurrent = IsEOSContextCurrent(NativeLoginContext, false);
			LogCallbackDisposition(TEXT("DeviceLogin"), Request, bCurrent ? TEXT("Consumed") : TEXT("StaleGeneration"), NativeLoginContext.Generation);
			// Remove ourselves to prevent accumulation on next call
			IdentityInterface->OnLoginCompleteDelegates->Remove(LoginCompleteDelegateHandle);
			LoginCompleteDelegateHandle.Reset(); IdentityLease.Reset(); OperationIdentity.Reset();

			// ToString() is the composite "<EpicAccountId>|<ProductUserId>" — cache only the PUID half
			const FString ProductUserId = bWasSuccessful ? UEEOSBlueprintLibrary::ExtractProductUserId(NewUserId.ToString()) : FString();
			const auto NativeLocal = IdentityInterface->GetUniquePlayerId(0);
			const bool bReady = bCurrent && NativeLocal.IsValid() && *NativeLocal == NewUserId && bWasSuccessful && NewUserId.IsValid() && IdentityInterface->GetLoginStatus(0) == ELoginStatus::LoggedIn && !ProductUserId.IsEmpty();
			const auto Outcome = CompleteOperation(TEXT("LoginWithDeviceId"), bReady, !bCurrent ? EEOSOperationCode::Canceled : bReady ? EEOSOperationCode::Succeeded : EEOSOperationCode::NativeFailure, !bCurrent ? TEXT("Original identity/platform retired.") : ErrorStr.IsEmpty() && !bReady ? TEXT("Device login completed without a usable Product User ID.") : ErrorStr, FString(), bWasSuccessful ? TEXT("Success") : TEXT("Failure"), EEOSResultSource::NativeCallback);
			FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
			if (bReady)
			{
				LastIdentityGeneration = CaptureEOSContext().Generation;
				CachedProductUserId = ProductUserId;
				bIsConnected = true;
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem: Device ID login succeeded: %s"), *FEEOSNativeOperationLease::SafeField(CachedProductUserId));
				OnConnectLoginComplete.Broadcast(true, CachedProductUserId); OnOperationCompleted.Broadcast(Outcome);
			}
			else if (bWasSuccessful)
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSConnectSubsystem: Device ID login reported success but the net id '%s' has no Product User ID"), *FEEOSNativeOperationLease::SafeField(NewUserId.ToString()));
				OnConnectLoginComplete.Broadcast(false, TEXT("")); OnOperationCompleted.Broadcast(Outcome);
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSConnectSubsystem: Device ID login failed — %s"), *FEEOSNativeOperationLease::SafeField(ErrorStr));
				OnConnectLoginComplete.Broadcast(false, TEXT("")); OnOperationCompleted.Broadcast(Outcome);
			}
		});

	const bool bStarted = IdentityInterface->Login(0, Credentials);
	if (!bStarted && LoginCompleteDelegateHandle.IsValid() && IdentityLease.GetRequestId() == Request)
	{
		IdentityInterface->ClearOnLoginCompleteDelegate_Handle(0, LoginCompleteDelegateHandle);
		LoginCompleteDelegateHandle.Reset(); IdentityLease.Reset(); OperationIdentity.Reset();
		const auto Outcome = CompleteOperation(TEXT("LoginWithDeviceId"), false, EEOSOperationCode::NativeStartRejected, TEXT("Native device login refused submission."));
		FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
		OnConnectLoginComplete.Broadcast(false, TEXT("")); OnOperationCompleted.Broadcast(Outcome);
	}
	if (!bStarted) return false;
	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem::LoginWithDeviceId — Logging in as '%s'..."), *FEEOSNativeOperationLease::SafeField(DisplayName));
	return true;
}

bool UEEOSConnectSubsystem::LinkAccount(EEOSExternalCredentialType CredentialType, const FString& Token)
{
	if (bShuttingDown) return false;
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("LinkAccount"));
		OnAccountLinked.Broadcast(false);
		return false;
	}

	if (!CachedContinuanceToken)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSConnectSubsystem::LinkAccount — No ContinuanceToken available. "
			"A ContinuanceToken is obtained when a Connect login returns EOS_InvalidUser. "
			"Listen for OnInvalidUserDetected and call LinkAccount within that flow."));
		OnAccountLinked.Broadcast(false);
		return false;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("LinkAccount"), TEXT("CapabilityUnavailable"));
		OnAccountLinked.Broadcast(false);
		return false;
	}

	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(PlatformHandle);
	if (!ConnectHandle)
	{
		FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("LinkAccount"), TEXT("CapabilityUnavailable"));
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


	EOS_Connect_LinkAccountOptions Options = {};
	Options.ApiVersion = EOS_CONNECT_LINKACCOUNT_API_LATEST;
	Options.ContinuanceToken = ConsumedToken;
	Options.LocalUserId = LocalPUID;

	auto* Context = BeginSDKMutation(TEXT("LinkAccount"));
	if (!Context) return false;
	CachedContinuanceToken = nullptr;
	EOS_Connect_LinkAccount(ConnectHandle, &Options, Context,
		[](const EOS_Connect_LinkAccountCallbackInfo* Data)
		{
			if (!Data || !EOS_EResult_IsOperationComplete(Data->ResultCode)) return;
			TUniquePtr<FEEOSConnectCallbackContext> Ctx(static_cast<FEEOSConnectCallbackContext*>(Data->ClientData));
			UEEOSConnectSubsystem* Self = Ctx.IsValid() ? Ctx->Self.Get() : nullptr;
			if (!Self || Self->bShuttingDown || Self->ActiveSDKMutation != Ctx->RequestId) return;
			if (!Self->IsEOSContextCurrent(Ctx->Context))
			{ Self->LogCallbackDisposition(Ctx->Operation, Ctx->RequestId, TEXT("StaleGeneration"), Ctx->Context.Generation); Ctx->Lease.Reset(); Self->CancelSDKMutation(); return; }
			Ctx->Lease.Reset();
			const auto Outcome = Self->FinishSDKMutation(Data->ResultCode == EOS_EResult::EOS_Success, ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			FEEOSOutcomeDispatchScope Dispatch(Self, Outcome);

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
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem::LinkAccount — Account linked successfully. PUID: %s"), *FEEOSNativeOperationLease::SafeField(Self->CachedProductUserId));
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSConnectSubsystem::LinkAccount — Failed: %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			}
			Self->OnAccountLinked.Broadcast(bSuccess); Self->OnOperationCompleted.Broadcast(Outcome);
		});

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem::LinkAccount — Linking account (type=%d) with ContinuanceToken..."), static_cast<int32>(CredentialType));
	return true;
}

bool UEEOSConnectSubsystem::UnlinkAccount(EEOSExternalCredentialType CredentialType)
{
	if (bShuttingDown) return false;
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("UnlinkAccount"));
		OnAccountUnlinked.Broadcast(false);
		return false;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("UnlinkAccount"), TEXT("CapabilityUnavailable"));
		OnAccountUnlinked.Broadcast(false);
		return false;
	}

	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(PlatformHandle);
	if (!ConnectHandle)
	{
		FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("UnlinkAccount"), TEXT("CapabilityUnavailable"));
		OnAccountUnlinked.Broadcast(false);
		return false;
	}

	// Get the logged-in product user ID
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid() || !IdentityInterface->GetUniquePlayerId(0).IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSConnectSubsystem::UnlinkAccount — No logged in user"));
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
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSConnectSubsystem::UnlinkAccount — Logged-in user has no Product User ID (no Connect session)"));
		OnAccountUnlinked.Broadcast(false);
		return false;
	}
	EOS_ProductUserId ProductUserId = EOS_ProductUserId_FromString(TCHAR_TO_ANSI(*ProductUserIdStr));

	EOS_Connect_UnlinkAccountOptions Options = {};
	Options.ApiVersion = EOS_CONNECT_UNLINKACCOUNT_API_LATEST;
	Options.LocalUserId = ProductUserId;

	auto* Context = BeginSDKMutation(TEXT("UnlinkAccount"));
	if (!Context) return false;
	EOS_Connect_UnlinkAccount(ConnectHandle, &Options, Context,
		[](const EOS_Connect_UnlinkAccountCallbackInfo* Data)
		{
			if (!Data || !EOS_EResult_IsOperationComplete(Data->ResultCode)) return;
			TUniquePtr<FEEOSConnectCallbackContext> Ctx(static_cast<FEEOSConnectCallbackContext*>(Data->ClientData));
			UEEOSConnectSubsystem* Self = Ctx.IsValid() ? Ctx->Self.Get() : nullptr;
			if (!Self || Self->bShuttingDown || Self->ActiveSDKMutation != Ctx->RequestId) return;
			if (!Self->IsEOSContextCurrent(Ctx->Context))
			{ Self->LogCallbackDisposition(Ctx->Operation, Ctx->RequestId, TEXT("StaleGeneration"), Ctx->Context.Generation); Ctx->Lease.Reset(); Self->CancelSDKMutation(); return; }
			Ctx->Lease.Reset();
			const auto Outcome = Self->FinishSDKMutation(Data->ResultCode == EOS_EResult::EOS_Success, ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			FEEOSOutcomeDispatchScope Dispatch(Self, Outcome);

			const bool bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);
			if (bSuccess)
			{
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem::UnlinkAccount — Account unlinked successfully"));
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSConnectSubsystem::UnlinkAccount — Failed: %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			}
			Self->OnAccountUnlinked.Broadcast(bSuccess); Self->OnOperationCompleted.Broadcast(Outcome);
		});

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem::UnlinkAccount — Unlinking account (type=%d)..."), static_cast<int32>(CredentialType));
	return true;
}

bool UEEOSConnectSubsystem::TransferDeviceIdAccount(const FString& DeviceIdProductUserId, const FString& ExternalProductUserId, bool bKeepExternalAccountProgression)
{
	if (bShuttingDown) return false;
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("TransferDeviceIdAccount"));
		OnDeviceIdAccountTransferred.Broadcast(false, TEXT(""));
		return false;
	}

	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("TransferDeviceIdAccount"), TEXT("CapabilityUnavailable"));
		OnDeviceIdAccountTransferred.Broadcast(false, TEXT(""));
		return false;
	}

	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(PlatformHandle);
	if (!ConnectHandle)
	{
		FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("TransferDeviceIdAccount"), TEXT("CapabilityUnavailable"));
		OnDeviceIdAccountTransferred.Broadcast(false, TEXT(""));
		return false;
	}

	// Accept either bare PUIDs or composite net-id strings; extraction is a no-op for bare ids.
	// EOS_ProductUserId_FromString performs NO validation, so guard on the strings instead.
	const FString DevicePUIDStr = UEEOSBlueprintLibrary::ExtractProductUserId(DeviceIdProductUserId);
	if (DevicePUIDStr.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSConnectSubsystem::TransferDeviceIdAccount — Invalid Device ID Product User ID: %s"), *FEEOSNativeOperationLease::SafeField(DeviceIdProductUserId));
		OnDeviceIdAccountTransferred.Broadcast(false, TEXT(""));
		return false;
	}

	const FString ExternalPUIDStr = UEEOSBlueprintLibrary::ExtractProductUserId(ExternalProductUserId);
	if (ExternalPUIDStr.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSConnectSubsystem::TransferDeviceIdAccount — Invalid external-account Product User ID: %s"), *FEEOSNativeOperationLease::SafeField(ExternalProductUserId));
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

	auto* Context = BeginSDKMutation(TEXT("TransferDeviceIdAccount"));
	if (!Context) return false;
	EOS_Connect_TransferDeviceIdAccount(ConnectHandle, &Options, Context,
		[](const EOS_Connect_TransferDeviceIdAccountCallbackInfo* Data)
		{
			if (!Data || !EOS_EResult_IsOperationComplete(Data->ResultCode)) return;
			TUniquePtr<FEEOSConnectCallbackContext> Ctx(static_cast<FEEOSConnectCallbackContext*>(Data->ClientData));
			UEEOSConnectSubsystem* Self = Ctx.IsValid() ? Ctx->Self.Get() : nullptr;
			if (!Self || Self->bShuttingDown || Self->ActiveSDKMutation != Ctx->RequestId) return;
			if (!Self->IsEOSContextCurrent(Ctx->Context))
			{ Self->LogCallbackDisposition(Ctx->Operation, Ctx->RequestId, TEXT("StaleGeneration"), Ctx->Context.Generation); Ctx->Lease.Reset(); Self->CancelSDKMutation(); return; }
			Ctx->Lease.Reset();
			const auto Outcome = Self->FinishSDKMutation(Data->ResultCode == EOS_EResult::EOS_Success, ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
			FEEOSOutcomeDispatchScope Dispatch(Self, Outcome);

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
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem::TransferDeviceIdAccount — Transfer successful, preserved PUID: %s"), *FEEOSNativeOperationLease::SafeField(PreservedPUID));
				Self->OnDeviceIdAccountTransferred.Broadcast(true, PreservedPUID); Self->OnOperationCompleted.Broadcast(Outcome);
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSConnectSubsystem::TransferDeviceIdAccount — Failed: %s"), ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)));
				Self->OnDeviceIdAccountTransferred.Broadcast(false, TEXT("")); Self->OnOperationCompleted.Broadcast(Outcome);
			}
		});

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSConnectSubsystem::TransferDeviceIdAccount — Transferring Device ID login '%s' into external account user '%s' (preserving %s)..."),
		*FEEOSNativeOperationLease::SafeField(DevicePUIDStr), *FEEOSNativeOperationLease::SafeField(ExternalPUIDStr), bKeepExternalAccountProgression ? TEXT("external-account progression") : TEXT("device-id progression"));
	return true;
}

FString UEEOSConnectSubsystem::GetProductUserId() const
{
	const auto Context = CaptureEOSContext();
	return GetEOSReadiness().bConnectLoggedIn ? UEEOSBlueprintLibrary::ExtractProductUserId(Context.LocalId) : FString();
}

bool UEEOSConnectSubsystem::IsConnected() const
{
	return GetEOSReadiness().bConnectLoggedIn;
}

FString UEEOSConnectSubsystem::GetDeviceIdDisplayName() const
{
	return CachedDeviceDisplayName;
}

bool UEEOSConnectSubsystem::HasContinuanceToken() const
{
	return CaptureEOSContext().Generation == LastIdentityGeneration && CachedContinuanceToken != nullptr;
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

FEEOSConnectCallbackContext* UEEOSConnectSubsystem::BeginSDKMutation(FName Operation)
{
	const auto Context = CaptureEOSContext();
	if (bShuttingDown || !Context.Platform.IsValid() || !Context.Identity.IsValid())
	{ RejectOperation(Operation, EEOSOperationCode::UnsupportedCapability, TEXT("Owning identity/platform unavailable.")); return nullptr; }
	if (!SDKMutationLease.TryAcquire(Context.Identity.Get(), TEXT("Identity0"), this, Operation))
	{ RejectOperation(Operation, EEOSOperationCode::Busy, TEXT("Another mutation owns the native identity.")); return nullptr; }
	SDKMutationContext = Context; ActiveSDKOperation = Operation;
	ActiveSDKMutation = BeginOperation(Operation, FString(), SDKMutationLease.GetRequestId());
	return new FEEOSConnectCallbackContext{this, Context.Platform, Context, SDKMutationLease, ActiveSDKMutation, Operation};
}
FEEOSOperationOutcome UEEOSConnectSubsystem::FinishSDKMutation(bool bSuccess, const FString& SDKResult, bool bCanceled)
{
	const auto Outcome = CompleteOperation(ActiveSDKOperation, bSuccess, bCanceled ? EEOSOperationCode::Canceled : bSuccess ? EEOSOperationCode::Succeeded : EEOSOperationCode::NativeFailure,
		bCanceled ? TEXT("Original identity/platform retired; SDK result unobserved.") : bSuccess ? TEXT("Connect mutation completed.") : TEXT("Connect mutation failed."),
		FString(), FString(), bCanceled ? EEOSResultSource::Plugin : EEOSResultSource::SDKCallback, SDKResult);
	SDKMutationLease.Reset(); ActiveSDKMutation = 0; ActiveSDKOperation = NAME_None; return Outcome;
}
void UEEOSConnectSubsystem::CancelSDKMutation()
{
	if (!ActiveSDKMutation) return;
	const FName Operation = ActiveSDKOperation;
	const auto Outcome = FinishSDKMutation(false, FString(), true);
	if (bShuttingDown) return;
	FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
	if (Operation == TEXT("DeleteDeviceId")) OnDeviceIdDeleted.Broadcast(false);
	else if (Operation == TEXT("LinkAccount")) OnAccountLinked.Broadcast(false);
	else if (Operation == TEXT("UnlinkAccount")) OnAccountUnlinked.Broadcast(false);
	else if (Operation == TEXT("TransferDeviceIdAccount")) OnDeviceIdAccountTransferred.Broadcast(false, FString());
	OnOperationCompleted.Broadcast(Outcome);
}
bool UEEOSConnectSubsystem::TickConnectIdentity(float)
{
	if (bShuttingDown) return false;
	const auto Context = CaptureEOSContext();
	if (LastIdentityGeneration != Context.Generation)
	{
		LastIdentityGeneration = Context.Generation;
		CachedContinuanceToken = nullptr; CachedProductUserId.Empty(); CachedDeviceDisplayName.Empty(); bIsConnected = false;
	}
	if (ActiveSDKMutation && !IsEOSContextCurrent(SDKMutationContext)) CancelSDKMutation();
	const auto Readiness = GetEOSReadiness();
	bIsConnected = Readiness.bConnectLoggedIn;
	CachedProductUserId = bIsConnected ? UEEOSBlueprintLibrary::ExtractProductUserId(Context.LocalId) : FString();
	return true;
}
