// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSAuthSubsystem.h"
#include "Shared/EEOSNativeOperation.h"
#include "Shared/EEOSIdentityUtils.h"
#include "Auth/EEOSConnectSubsystem.h"
#include "Shared/EEOSSettings.h"
#include "Shared/EEOSBlueprintLibrary.h"
#include "EOSSettings.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "UnrealExtendedEOS.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

#if WITH_EOS_SDK
#include "IEOSSDKManager.h"
#include "OnlineSubsystemEOSTypesPublic.h"
#include "eos_connect.h"
#include "eos_auth.h"
#include "eos_sdk.h"
#endif

// ── Initialize / Deinitialize ────────────────────────────────────────────────

void UEEOSAuthSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	TickNativeIdentity(0);
	IdentityTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UEEOSAuthSubsystem::TickNativeIdentity), 0.5f);

	const UEEOSSettings* Settings = GetEOSSettings();
	if (Settings && (Settings->bAutoLoginOnStart || Settings->bAutoConnectLoginOnStart))
	{
		// Defer one tick so Blueprints (GameInstance, early widgets) can bind to the login
		// delegates before any auto-login result broadcasts. The GameInstance timer manager
		// is safe to use here: it is created in the UGameInstance constructor, while
		// subsystems initialize later inside UGameInstance::Init.
		AutoLoginTimer = GetGameInstance()->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				KickOffAutoLogin();
			}));
	}
}

void UEEOSAuthSubsystem::KickOffAutoLogin()
{
	if (bShuttingDown) return;
	const UEEOSSettings* Settings = GetEOSSettings();
	if (!Settings)
	{
		return;
	}

	if (Settings->bAutoLoginOnStart)
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem: Auto-login enabled, attempting login..."));
		LoginWithDefaults();
	}
	else if (Settings->bAutoConnectLoginOnStart)
	{
		// No Auth login, go straight to Connect login (e.g. Steam, DeviceId)
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem: Auto Connect login enabled (no Auth login)"));
		ConnectLoginWithDefaults();
	}
}

void UEEOSAuthSubsystem::Deinitialize()
{
	BeginEOSShutdown(); bShuttingDown = true;
	if (GetGameInstance()) GetGameInstance()->GetTimerManager().ClearTimer(AutoLoginTimer);
	if (IdentityTicker.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(IdentityTicker);
	IdentityTicker.Reset();
	if (bNativeIdentitySubmitted && (LoginDelegateHandle.IsValid() || LogoutDelegateHandle.IsValid()))
		EEOSIdentity::FRetired::Hold(OperationIdentity, IdentityLease, GetOwningEOSInstanceName(), LogoutDelegateHandle.IsValid());
	if (OperationIdentity.IsValid())
	{
		OperationIdentity->ClearOnLoginCompleteDelegate_Handle(0, LoginDelegateHandle);
		OperationIdentity->ClearOnLogoutCompleteDelegate_Handle(0, LogoutDelegateHandle);
	}
	LoginDelegateHandle.Reset(); LogoutDelegateHandle.Reset(); IdentityLease.Reset(); OperationIdentity.Reset(); bNativeIdentitySubmitted = false; ConnectLease.Reset(); ConnectOperationIdentity.Reset(); ActiveSDKConnectRequest = 0; ActivePersistentAuthRequest = 0;
	CachedProductUserId.Empty(); bConnectedToGameServices = false;
	Super::Deinitialize();
}

// ── Epic Auth Login ──────────────────────────────────────────────────────────

bool UEEOSAuthSubsystem::Login(EEOSLoginType LoginType, const FString& Id, const FString& Token)
{
	FOnlineAccountCredentials Credentials;

	switch (LoginType)
	{
	case EEOSLoginType::Password:
		Credentials.Type = TEXT("password");
		Credentials.Id = Id;
		Credentials.Token = Token;
		break;
	case EEOSLoginType::ExchangeCode:
		Credentials.Type = TEXT("exchangecode");
		Credentials.Token = Token;
		break;
	case EEOSLoginType::PersistentAuth:
		Credentials.Type = TEXT("persistentauth");
		break;
	case EEOSLoginType::DeviceCode:
		// EOS_LCT_DeviceCode is "Not supported. Superseded by EOS_LCT_ExternalAuth."
		// (eos_auth_types.h) and "devicecode" does not exist in the engine's credential
		// parser (FUserManagerEOS ToEOS_ELoginCredentialType) — fail fast and clearly.
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem::Login — DeviceCode login is not supported by this EOS SDK (superseded by ExternalAuth). Use LoginWithExternalAuth instead."));
		BroadcastLoginPreflightFailure(TEXT("DeviceCode login is not supported by this EOS SDK"));
		return false;
	case EEOSLoginType::Developer:
		{
			const UEEOSSettings* Settings = GetEOSSettings();
			Credentials.Type = TEXT("developer");
			Credentials.Id = Settings ? Settings->DevAuthToolAddress : TEXT("localhost:6547");
			Credentials.Token = Settings ? Settings->DevAuthCredentialName : TEXT("");
		}
		break;
	case EEOSLoginType::AccountPortal:
		Credentials.Type = TEXT("accountportal");
		break;
	case EEOSLoginType::ExternalAuth:
		// A bare "externalauth" (no ":<TokenType>" suffix) is rejected by the engine
		// (FUserManagerEOS::CallEOSAuthLogin: "External Auth Token Type not specified").
		// The token type cannot be derived from this signature — use the dedicated API.
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem::Login — ExternalAuth requires the external token type. Call LoginWithExternalAuth(CredentialType, Token) instead."));
		BroadcastLoginPreflightFailure(TEXT("ExternalAuth requires a credential type — use LoginWithExternalAuth"));
		return false;
	}

	return PerformAuthLogin(Credentials, LoginType);
}

bool UEEOSAuthSubsystem::LoginWithExternalAuth(EEOSExternalCredentialType CredentialType, const FString& Token, const FString& ExternalAccountId)
{
	const FString TokenType = ExternalCredentialTypeToTokenTypeString(CredentialType);
	if (TokenType.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem::LoginWithExternalAuth — Invalid external credential type (%d)"), static_cast<int32>(CredentialType));
		BroadcastLoginPreflightFailure(TEXT("Invalid external credential type"));
		return false;
	}

	if (Token.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem::LoginWithExternalAuth — Token is empty (type=%s)"), *FEEOSNativeOperationLease::SafeField(TokenType));
		BroadcastLoginPreflightFailure(TEXT("External auth token is empty"));
		return false;
	}

	// The engine splits Credentials.Type on ':' and maps the suffix through
	// LexFromString(EOS_EExternalCredentialType&) — see FUserManagerEOS::CallEOSAuthLogin.
	FOnlineAccountCredentials Credentials;
	Credentials.Type = FString::Printf(TEXT("externalauth:%s"), *TokenType);
	Credentials.Id = ExternalAccountId; // Optional; if set, must match the token's account
	Credentials.Token = Token;

	return PerformAuthLogin(Credentials, EEOSLoginType::ExternalAuth);
}

void UEEOSAuthSubsystem::BroadcastLoginPreflightFailure(const FString& Error)
{
	// R1: never echo a failure on the shared OnLoginComplete while a legitimate login is
	// in flight — waiters could not tell the echo from the real completion.
	if (bShuttingDown || IdentityLease.IsValid() || LoginDelegateHandle.IsValid() || LogoutDelegateHandle.IsValid() || bConnectLoginInFlight)
	{
		RejectOperation(TEXT("Login"), EEOSOperationCode::Busy, TEXT("Preflight failed while an admitted identity mutation owns the shared completion route."));
		return;
	}
	OnLoginComplete.Broadcast(false, Error);
}

bool UEEOSAuthSubsystem::PerformAuthLogin(const FOnlineAccountCredentials& Credentials, EEOSLoginType LoginType)
{
	// In-progress guard FIRST (R1): a second Login while one is pending must not stack
	// another delegate registration, and it must NOT broadcast — a failure echo on the
	// shared OnLoginComplete would poison the in-flight login's waiters. Log + reject;
	// no delegate fires for this call.
	if (bShuttingDown || IdentityLease.IsValid() || LoginDelegateHandle.IsValid() || LogoutDelegateHandle.IsValid() || bConnectLoginInFlight)
	{
		RejectOperation(TEXT("Login"), EEOSOperationCode::Busy, TEXT("An identity operation is already pending."));
		return false;
	}

	// The engine rejects Login while logged in ("Already logged in") — guard here so the
	// rejection can't flip CurrentLoginStatus from LoggedIn to Failed. Log-only (R1).
	if (IsLoggedIn())
	{
		RejectOperation(TEXT("Login"), EEOSOperationCode::AlreadyLoggedIn, TEXT("Native identity is already logged in; healthy state preserved and no native login submitted."));
		return false;
	}

	// Pre-flight failures below broadcast: no login is in flight (guard above), so the
	// failure signal is unambiguous.
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("Login"));
		OnLoginComplete.Broadcast(false, TEXT("EOS is not available"));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid())
	{
		FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("Login"), TEXT("CapabilityUnavailable"));
		OnLoginComplete.Broadcast(false, TEXT("Identity interface not available"));
		return false;
	}

	if (!IdentityLease.TryAcquire(IdentityInterface.Get(), TEXT("Identity0"), this, TEXT("Login")))
	{
		RejectOperation(TEXT("Login"), EEOSOperationCode::Busy, TEXT("Another plugin identity operation is pending.")); return false;
	}
	OperationIdentity = IdentityInterface; NativeIdentityContext = CaptureEOSContext();
	const int64 Request = BeginOperation(TEXT("Login"), FString(), IdentityLease.GetRequestId());
	bIdentitySubmissionRejected = false; bNativeIdentitySubmitted = false;
	CurrentLoginStatus = EEOSLoginStatus::LoggingIn;
	UsedLoginType = LoginType;

	LoginDelegateHandle = IdentityInterface->AddOnLoginCompleteDelegate_Handle(0, FOnLoginCompleteDelegate::CreateUObject(this, &UEEOSAuthSubsystem::HandleLoginComplete));
	OnLoginStatusChanged.Broadcast(CurrentLoginStatus);
	if (bShuttingDown || IdentityLease.GetRequestId() != Request) return false;
	bNativeIdentitySubmitted = true;
	const bool bStarted = IdentityInterface->Login(0, Credentials);
	if (!bStarted && LoginDelegateHandle.IsValid() && IdentityLease.GetRequestId() == Request)
	{
		bIdentitySubmissionRejected = true;
		HandleLoginComplete(0, false, *FUniqueNetIdString::Create(FString(), NAME_None), TEXT("Native Login refused submission."));
	}
	if (!bStarted) return false;

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem::Login — Attempting login with type: %s"), *FEEOSNativeOperationLease::SafeField(Credentials.Type));
	return true;
}

bool UEEOSAuthSubsystem::PerformEngineAutoLogin()
{
	// Same guard order/contract as PerformAuthLogin (R1).
	if (LoginDelegateHandle.IsValid())
	{
		RejectOperation(TEXT("Login"), EEOSOperationCode::Busy, TEXT("Another admitted request already owns this operation."));
		return false;
	}
	if (IsLoggedIn())
	{
		RejectOperation(TEXT("Login"), EEOSOperationCode::AlreadyLoggedIn, TEXT("Native identity is already logged in; healthy state preserved."));
		return false;
	}
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("LoginWithDefaults"));
		OnLoginComplete.Broadcast(false, TEXT("EOS is not available"));
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid())
	{
		FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("LoginWithDefaults"), TEXT("CapabilityUnavailable"));
		OnLoginComplete.Broadcast(false, TEXT("Identity interface not available"));
		return false;
	}

	// Without bPreferPersistentAuth the engine's AutoLogin skips the persistent-auth branch
	// entirely — and in an EAS + EOS-platform configuration that leaves it with no valid branch
	// at all, so it refuses without ever firing a completion. Warn precisely instead of leaving
	// the caller to decode "No valid configuration for AutoLogin".
	if (!UEOSSettings::GetSettings().bPreferPersistentAuth)
	{
		UE_LOG(LogExtendedEOS, Warning,
			TEXT("EEOSAuthSubsystem: bPreferPersistentAuth is false in [/Script/OnlineSubsystemEOS.EOSSettings] — ")
			TEXT("silent re-login is disabled and AutoLogin may refuse outright. Set bPreferPersistentAuth=true in DefaultEngine.ini."));
	}

	if (!IdentityLease.TryAcquire(IdentityInterface.Get(), TEXT("Identity0"), this, TEXT("Login")))
	{
		RejectOperation(TEXT("Login"), EEOSOperationCode::Busy, TEXT("Another plugin identity operation is pending.")); return false;
	}
	OperationIdentity = IdentityInterface; NativeIdentityContext = CaptureEOSContext();
	const int64 Request = BeginOperation(TEXT("Login"), FString(), IdentityLease.GetRequestId());
	bIdentitySubmissionRejected = false; bNativeIdentitySubmitted = false;
	CurrentLoginStatus = EEOSLoginStatus::LoggingIn;
	// First attempt of the engine chain; it may fall back to Account Portal internally.
	UsedLoginType = EEOSLoginType::PersistentAuth;

	LoginDelegateHandle = IdentityInterface->AddOnLoginCompleteDelegate_Handle(0, FOnLoginCompleteDelegate::CreateUObject(this, &UEEOSAuthSubsystem::HandleLoginComplete));
	OnLoginStatusChanged.Broadcast(CurrentLoginStatus);
	if (bShuttingDown || IdentityLease.GetRequestId() != Request) return false;

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem::LoginWithDefaults — Auto-login (persistent auth first, Account Portal fallback)"));

	bNativeIdentitySubmitted = true;
	const bool bStarted = IdentityInterface->AutoLogin(0);
	if (!bStarted && LoginDelegateHandle.IsValid() && IdentityLease.GetRequestId() == Request)
	{
		bIdentitySubmissionRejected = true;
		HandleLoginComplete(0, false, *FUniqueNetIdString::Create(FString(), NAME_None), TEXT("AutoLogin refused; inspect native EOS configuration."));
	}
	return bStarted;
}

FString UEEOSAuthSubsystem::ExternalCredentialTypeToTokenTypeString(EEOSExternalCredentialType CredentialType)
{
	// These strings must match the EOSShared LexFromString(EOS_EExternalCredentialType&)
	// names (EOSShared.cpp) — anything else fails the engine's suffix parse.
	switch (CredentialType)
	{
	case EEOSExternalCredentialType::Steam:    return TEXT("SteamSessionTicket");
	case EEOSExternalCredentialType::PSN:      return TEXT("PSNIdToken");
	case EEOSExternalCredentialType::XboxLive: return TEXT("XBLXSTSToken");
	case EEOSExternalCredentialType::Nintendo: return TEXT("NintendoIdToken");
	case EEOSExternalCredentialType::Discord:  return TEXT("DiscordAccessToken");
	case EEOSExternalCredentialType::OpenID:   return TEXT("OpenIdAccessToken");
	case EEOSExternalCredentialType::Apple:    return TEXT("AppleIdToken");
	case EEOSExternalCredentialType::Google:   return TEXT("GoogleIdToken");
	case EEOSExternalCredentialType::None:
	default:
		return FString();
	}
}

bool UEEOSAuthSubsystem::LoginWithDefaults()
{
	const UEEOSSettings* Settings = GetEOSSettings();
	if (Settings)
	{
		// AccountPortal and PersistentAuth are the two halves of the SDK's standalone auto-login
		// sequence (eos_auth_types.h: persistent token first, manual prompt as fallback), so both
		// route through the engine's AutoLogin — that is what actually consumes the keychain
		// token. Calling Login(AccountPortal) directly would prompt the browser on every launch.
		// Other types (Developer, ExternalAuth, Password, ExchangeCode) stay explicit.
		if (Settings->DefaultLoginType == EEOSLoginType::AccountPortal ||
			Settings->DefaultLoginType == EEOSLoginType::PersistentAuth)
		{
			return PerformEngineAutoLogin();
		}
		return Login(Settings->DefaultLoginType);
	}

	FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("LoginWithDefaults"), TEXT("CapabilityUnavailable"));
	BroadcastLoginPreflightFailure(TEXT("EOS Settings not available"));
	return false;
}

bool UEEOSAuthSubsystem::Logout()
{
	// In-progress guard FIRST (R1): don't stack a second delegate registration and don't
	// broadcast for the rejected duplicate. The pending logout's completion is the single
	// OnLogoutComplete broadcast for both calls (the delegate has no failure payload to
	// report a per-call rejection with).
	if (bShuttingDown || IdentityLease.IsValid() || LoginDelegateHandle.IsValid() || LogoutDelegateHandle.IsValid() || bConnectLoginInFlight)
	{
		RejectOperation(TEXT("Logout"), EEOSOperationCode::Busy, TEXT("Another admitted request already owns this operation."));
		return false;
	}

	// Pre-flight failures below broadcast: no logout is in flight (guard above).
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("Logout"));
		OnLogoutComplete.Broadcast();
		return false;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid())
	{
		FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("Logout"), TEXT("CapabilityUnavailable"));
		OnLogoutComplete.Broadcast();
		return false;
	}

	if (!IdentityLease.TryAcquire(IdentityInterface.Get(), TEXT("Identity0"), this, TEXT("Logout")))
	{
		RejectOperation(TEXT("Logout"), EEOSOperationCode::Busy, TEXT("Another plugin identity operation is pending.")); return false;
	}
	OperationIdentity = IdentityInterface; NativeIdentityContext = CaptureEOSContext();
	const int64 Request = BeginOperation(TEXT("Logout"), FString(), IdentityLease.GetRequestId());
	bIdentitySubmissionRejected = false;
	LogoutDelegateHandle = IdentityInterface->AddOnLogoutCompleteDelegate_Handle(0, FOnLogoutCompleteDelegate::CreateUObject(this, &UEEOSAuthSubsystem::HandleLogoutComplete));
	bNativeIdentitySubmitted = true;
	const bool bStarted = IdentityInterface->Logout(0);
	if (!bStarted && LogoutDelegateHandle.IsValid() && IdentityLease.GetRequestId() == Request)
	{
		bIdentitySubmissionRejected = true;
		HandleLogoutComplete(0, false);
	}
	if (!bStarted) return false;

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem::Logout — Logging out..."));
	return true;
}

bool UEEOSAuthSubsystem::RefreshAuthToken()
{
	// There is no usable in-process refresh primitive:
	// - The EOS SDK auto-refreshes the Epic auth token internally while logged in.
	// - EOS_LCT_RefreshToken (eos_auth_types.h) only exists for handing a token to another
	//   local process (custom launcher flows), and the engine's credential parser
	//   (FUserManagerEOS ToEOS_ELoginCredentialType) does not accept it anyway.
	// - Login-while-logged-in is rejected by the engine ("Already logged in").
	// So report the CURRENT cached token and never touch login state.
	if (!IsLoggedIn())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem::RefreshAuthToken — Not logged in"));
		OnAuthTokenRefreshed.Broadcast(false, TEXT(""));
		return false;
	}

	const FString Token = GetAuthToken();
	if (Token.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem::RefreshAuthToken — Logged in but no auth token is available"));
		OnAuthTokenRefreshed.Broadcast(false, TEXT(""));
		return false;
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem::RefreshAuthToken — Returning current auth token (the SDK refreshes it internally)"));
	OnAuthTokenRefreshed.Broadcast(true, Token);
	return true;
}

bool UEEOSAuthSubsystem::DeletePersistentAuth()
{
	// In-progress guard FIRST (R1): the logged-in path routes through Logout, so a
	// pending logout means this call cannot start. Log + reject; no delegate fires for
	// this call.
	if (bShuttingDown || IdentityLease.IsValid() || LoginDelegateHandle.IsValid() || LogoutDelegateHandle.IsValid() || bConnectLoginInFlight)
	{
		RejectOperation(TEXT("DeletePersistentAuth"), EEOSOperationCode::Busy, TEXT("Another admitted request already owns this operation."));
		return false;
	}

	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("DeletePersistentAuth"));
		OnPersistentAuthDeleted.Broadcast(false);
		return false;
	}

	// Logged-in path: route through Logout as before. The engine's logout path deletes
	// the persistent credentials itself (FUserManagerEOS::CallEOSAuthLogout →
	// EOS_Auth_DeletePersistentAuth), so a normal logout both clears 'remember me' and —
	// via HandleLogoutComplete — updates CurrentLoginStatus/bConnectedToGameServices and
	// broadcasts OnLogoutComplete/OnLoginStatusChanged exactly like Logout(), then
	// follows up with OnPersistentAuthDeleted.
	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr IdentityInterface = EOSSub ? EOSSub->GetIdentityInterface() : nullptr;
	if (IdentityInterface.IsValid() && IdentityInterface->GetUniquePlayerId(0).IsValid())
	{
		bPendingPersistentAuthDeleteViaLogout = true;
		const bool bStarted = Logout();
		if (!bStarted && bPendingPersistentAuthDeleteViaLogout) bPendingPersistentAuthDeleteViaLogout = false;
		return bStarted;
	}

	// Sessionless path (the primary use case — clearing 'remember me' from a login
	// screen): the engine's logout route needs a live session and would delete nothing.
	// Do what the engine itself does in sessionless contexts: call
	// EOS_Auth_DeletePersistentAuth directly on the platform handle.
#if WITH_EOS_SDK
	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("DeletePersistentAuth"), TEXT("CapabilityUnavailable"));
		OnPersistentAuthDeleted.Broadcast(false);
		return false;
	}

	EOS_HAuth AuthHandle = EOS_Platform_GetAuthInterface(PlatformHandle);
	if (!AuthHandle)
	{
		FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("DeletePersistentAuth"), TEXT("CapabilityUnavailable"));
		OnPersistentAuthDeleted.Broadcast(false);
		return false;
	}

	EOS_Auth_DeletePersistentAuthOptions Options = {};
	Options.ApiVersion = EOS_AUTH_DELETEPERSISTENTAUTH_API_LATEST;
	// RefreshToken is Console-only (the caller manages token storage there); on Desktop
	// and Mobile it must be NULL (eos_auth_types.h, EOS_Auth_DeletePersistentAuthOptions).
	Options.RefreshToken = nullptr;

	const auto PlatformOwner = GetOwningEOSPlatform();
	if (!PlatformOwner.IsValid() || !IdentityInterface.IsValid())
	{
		RejectOperation(TEXT("DeletePersistentAuth"), EEOSOperationCode::UnsupportedCapability, TEXT("An owning platform and identity interface are required."));
		OnPersistentAuthDeleted.Broadcast(false); return false;
	}
	if (!IdentityLease.TryAcquire(IdentityInterface.Get(), TEXT("Identity0"), this, TEXT("DeletePersistentAuth")))
	{
		RejectOperation(TEXT("DeletePersistentAuth"), EEOSOperationCode::Busy, TEXT("Another plugin identity operation is pending.")); return false;
	}
	OperationIdentity = IdentityInterface; NativeIdentityContext = CaptureEOSContext();
	ActivePersistentAuthRequest = BeginOperation(TEXT("DeletePersistentAuth"), FString(), IdentityLease.GetRequestId());
	struct FDeletePersistentAuthContext
	{
		TWeakObjectPtr<UEEOSAuthSubsystem> Self;
		TSharedPtr<IEOSPlatformHandle, ESPMode::ThreadSafe> Platform;
		FEEOSNativeOperationLease Lease;
		IOnlineIdentityPtr Identity;
		int64 RequestId;
		FEEOSRequestContext Context;
	};
	EOS_Auth_DeletePersistentAuth(AuthHandle, &Options,
		new FDeletePersistentAuthContext{this, PlatformOwner, IdentityLease, IdentityInterface, ActivePersistentAuthRequest, CaptureEOSContext()},
		[](const EOS_Auth_DeletePersistentAuthCallbackInfo* Data)
		{
			if (!Data || !Data->ClientData || !EOS_EResult_IsOperationComplete(Data->ResultCode)) return;
			TUniquePtr<FDeletePersistentAuthContext> Ctx(static_cast<FDeletePersistentAuthContext*>(Data->ClientData));
			const bool bSuccess = Data->ResultCode == EOS_EResult::EOS_Success;
			AsyncTask(ENamedThreads::GameThread, [Weak = Ctx->Self, Platform = MoveTemp(Ctx->Platform), NativeLease = MoveTemp(Ctx->Lease),
				Identity = MoveTemp(Ctx->Identity), Request = Ctx->RequestId, Context = Ctx->Context, bSuccess, SDKResult = FString(ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode)))]() mutable
			{
				UEEOSAuthSubsystem* Self = Weak.Get();
				if (!Self || Self->bShuttingDown || Self->ActivePersistentAuthRequest != Request) return;
				const bool bCurrent = Self->IsEOSContextCurrent(Context);
				Self->ActivePersistentAuthRequest = 0; Self->IdentityLease.Reset(); Self->OperationIdentity.Reset();
				NativeLease.Reset(); Identity.Reset();
				const auto Outcome = Self->CompleteOperation(TEXT("DeletePersistentAuth"), bSuccess && bCurrent, !bCurrent ? EEOSOperationCode::Canceled : bSuccess ? EEOSOperationCode::Succeeded : EEOSOperationCode::NativeFailure,
					!bCurrent ? TEXT("Original identity/platform retired.") : bSuccess ? TEXT("Persistent auth credentials deleted.") : TEXT("Persistent auth deletion failed."), FString(), FString(), EEOSResultSource::SDKCallback, SDKResult);
				FEEOSOutcomeDispatchScope Dispatch(Self, Outcome);
				Self->OnPersistentAuthDeleted.Broadcast(bSuccess && bCurrent); Self->OnOperationCompleted.Broadcast(Outcome);
			});
		});

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem::DeletePersistentAuth — No logged-in user, deleting persistent auth credentials directly via EOS_Auth_DeletePersistentAuth"));
	return true;
#else
	FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("DeletePersistentAuth"), TEXT("CapabilityUnavailable"));
	OnPersistentAuthDeleted.Broadcast(false);
	return false;
#endif
}

// ── EOS Connect Login (Game Services) ────────────────────────────────────────

bool UEEOSAuthSubsystem::ConnectLogin(EEOSConnectLoginType LoginType, const FString& Token, const FString& DisplayName)
{
#if WITH_EOS_SDK
	return PerformConnectLogin(LoginType, Token, DisplayName);
#else
	FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("ConnectLogin"), TEXT("CapabilityUnavailable"));
	OnConnectLoginComplete.Broadcast(false, TEXT(""), TEXT("EOS SDK not available"));
	return false;
#endif
}

bool UEEOSAuthSubsystem::ConnectLoginWithDeviceId(const FString& DisplayName)
{
#if WITH_EOS_SDK
	// In-flight guard FIRST (R1/m8): the create→login chain counts as a Connect login in
	// flight. Log + reject; no delegate fires for this call.
	if (bShuttingDown || bConnectLoginInFlight || IdentityLease.IsValid())
	{
		RejectOperation(TEXT("ConnectLogin"), EEOSOperationCode::Busy, TEXT("Another admitted request already owns this operation."));
		return false;
	}

	const IEOSPlatformHandlePtr PlatformOwner = GetOwningEOSPlatform();
	if (!PlatformOwner.IsValid())
	{
		OnConnectLoginComplete.Broadcast(false, TEXT(""), TEXT("Owning EOS platform is unavailable.")); return false;
	}
	const EOS_HPlatform PlatformHandle = *PlatformOwner;
	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(PlatformHandle);
	if (!ConnectHandle)
	{
		OnConnectLoginComplete.Broadcast(false, TEXT(""), TEXT("Connect interface not available"));
		return false;
	}

	// First, create the device ID (idempotent — succeeds or returns DuplicateNotAllowed)
	EOS_Connect_CreateDeviceIdOptions CreateOpts = {};
	CreateOpts.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
	
	FString DeviceModel = FString::Printf(TEXT("PC Windows - %s"), FPlatformProcess::ComputerName());
	FTCHARToUTF8 DeviceModelUtf8(*DeviceModel);
	CreateOpts.DeviceModel = DeviceModelUtf8.Get();

	if (!ConnectLease.IsValid())
	{
		IOnlineSubsystem* OSS = GetExistingEOSOnlineSubsystem();
		const auto Identity = OSS ? OSS->GetIdentityInterface() : IOnlineIdentityPtr();
		if (!Identity.IsValid() || !ConnectLease.TryAcquire(Identity.Get(), TEXT("Identity0"), this, TEXT("ConnectLogin")))
		{
			RejectOperation(TEXT("ConnectLogin"), EEOSOperationCode::Busy, TEXT("Another plugin identity operation is pending.")); return false;
		}
		ConnectOperationIdentity = Identity;
		ActiveSDKConnectRequest = BeginOperation(TEXT("ConnectLogin"), FString(), ConnectLease.GetRequestId());
		SDKConnectContext = CaptureEOSContext();
	}
	struct FDeviceIdContext
	{
		TWeakObjectPtr<UEEOSAuthSubsystem> Self;
		FString DisplayName;
		IEOSPlatformHandlePtr Platform;
		FEEOSNativeOperationLease Lease;
		int64 RequestId;
		IOnlineIdentityPtr Identity;
	};
	FDeviceIdContext* Ctx = new FDeviceIdContext{this, DisplayName, PlatformOwner, ConnectLease, ActiveSDKConnectRequest, ConnectOperationIdentity};

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem: Creating DeviceId..."));


	// Use a static EOS_CALL wrapper function for the callback
	struct FDeviceIdCallbackWrapper
	{
		static void EOS_CALL Callback(const EOS_Connect_CreateDeviceIdCallbackInfo* Data)
		{
			if (!Data || !EOS_EResult_IsOperationComplete(Data->ResultCode)) return;
			TUniquePtr<FDeviceIdContext> C(static_cast<FDeviceIdContext*>(Data->ClientData));
			if (!C.IsValid()) return;
			if (!C->Self.IsValid() || !C->Self->IsConnectRequestCurrent(C->RequestId)) return; // Subsystem destroyed while in flight — context freed by TUniquePtr

			if (Data->ResultCode == EOS_EResult::EOS_Success ||
				Data->ResultCode == EOS_EResult::EOS_DuplicateNotAllowed)
			{
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem: DeviceId ready, performing Connect login..."));

				AsyncTask(ENamedThreads::GameThread, [WeakSelf = C->Self, DisplayName = C->DisplayName, Request = C->RequestId, Lease = C->Lease, Platform = C->Platform, Identity = C->Identity]()
				{
					if (UEEOSAuthSubsystem* Self = WeakSelf.Get(); Self && Self->IsConnectRequestCurrent(Request))
					{
						// Hand the in-flight slot from the device-id phase to the login
						// phase: clear the flag so PerformConnectLogin's own guard doesn't
						// reject our chain (it re-arms the flag when it issues the login).
						Self->bConnectLoginInFlight = false;
						Self->PerformConnectLogin(EEOSConnectLoginType::DeviceId, TEXT(""), DisplayName);
					}
				});
			}
			else
			{
				FString ErrorMsg = FString::Printf(TEXT("CreateDeviceId failed: %hs"), EOS_EResult_ToString(Data->ResultCode));
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem: %s"), *FEEOSNativeOperationLease::SafeField(ErrorMsg));

				AsyncTask(ENamedThreads::GameThread, [WeakSelf = C->Self, ErrorMsg, Request = C->RequestId, Lease = C->Lease, Platform = C->Platform, Identity = C->Identity]() mutable
				{
					if (UEEOSAuthSubsystem* Self = WeakSelf.Get(); Self && Self->IsConnectRequestCurrent(Request))
					{
						Lease.Reset();
						Self->SetConnectLoginResult(false, TEXT(""), ErrorMsg, Request);
					}
				});
			}
		}
	};

	// Arm the in-flight guard for the whole chain: it is released either by
	// SetConnectLoginResult (all failure/completion paths funnel there) or handed to
	// PerformConnectLogin on the success continuation above.
	bConnectLoginInFlight = true;

	EOS_Connect_CreateDeviceId(ConnectHandle, &CreateOpts, Ctx, &FDeviceIdCallbackWrapper::Callback);
	return true;
#else
	OnConnectLoginComplete.Broadcast(false, TEXT(""), TEXT("EOS SDK not available"));
	return false;
#endif
}

bool UEEOSAuthSubsystem::ConnectLoginWithDefaults()
{
	const UEEOSSettings* Settings = GetEOSSettings();
	if (!Settings)
	{
		// R1: only broadcast the pre-flight failure when no Connect login is in flight.
		if (bConnectLoginInFlight)
		{
			FEEOSNativeOperationLease::ReportRepeated(GetOwningEOSInstanceName(), TEXT("ConnectLogin"), TEXT("CapabilityUnavailable"));
			return false;
		}
		OnConnectLoginComplete.Broadcast(false, TEXT(""), TEXT("Settings not available"));
		return false;
	}

	if (Settings->DefaultConnectLoginType == EEOSConnectLoginType::DeviceId)
	{
		return ConnectLoginWithDeviceId(TEXT("Player"));
	}

	// Epic can piggyback on an active Auth login — use the Epic auth token if we have one.
	if (Settings->DefaultConnectLoginType == EEOSConnectLoginType::Epic && IsLoggedIn())
	{
		const FString EpicToken = GetAuthToken();
		if (!EpicToken.IsEmpty())
		{
			return ConnectLogin(EEOSConnectLoginType::Epic, EpicToken);
		}
	}

	// Every other type needs a platform token we cannot fetch here (Steam, PSN, etc.).
	// Issuing a token-less EOS_Connect_Login would just fail at the SDK — fail fast with
	// a clear config error instead.
	const FString ErrorMsg = FString::Printf(
		TEXT("ConnectLoginWithDefaults — DefaultConnectLoginType (%d) requires a platform token that cannot be fetched automatically. Call ConnectLogin(Type, Token) with a token from the platform SDK, or set DefaultConnectLoginType to DeviceId."),
		static_cast<int32>(Settings->DefaultConnectLoginType));
	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem: %s"), *FEEOSNativeOperationLease::SafeField(ErrorMsg));
	// R1: only broadcast the pre-flight failure when no Connect login is in flight.
	if (!bConnectLoginInFlight)
	{
		OnConnectLoginComplete.Broadcast(false, TEXT(""), ErrorMsg);
	}
	return false;
}

#if WITH_EOS_SDK

// ── Static EOS_CALL callbacks for Connect ────────────────────────────────────

struct FConnectLoginContext
{
	TWeakObjectPtr<UEEOSAuthSubsystem> Self;
	bool bAutoCreateUser;
	IEOSPlatformHandlePtr Platform;
	FEEOSNativeOperationLease Lease;
	int64 RequestId;
	IOnlineIdentityPtr Identity;
	FString SDKResult;
};

static void EOS_CALL OnConnectCreateUserComplete(const EOS_Connect_CreateUserCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;
	if (!Data || !EOS_EResult_IsOperationComplete(Data->ResultCode)) return;
	TUniquePtr<FConnectLoginContext> Ctx(static_cast<FConnectLoginContext*>(Data->ClientData));
	if (!Ctx->Self.IsValid() || !Ctx->Self->IsConnectRequestCurrent(Ctx->RequestId)) return; // Subsystem destroyed while in flight — context freed by TUniquePtr

	Ctx->SDKResult = ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode));
	if (Data->ResultCode == EOS_EResult::EOS_Success)
	{
		char PuidBuf[EOS_PRODUCTUSERID_MAX_LENGTH + 1] = {};
		int32_t PuidLen = sizeof(PuidBuf);
		EOS_ProductUserId_ToString(Data->LocalUserId, PuidBuf, &PuidLen);
		FString ProductUserId(UTF8_TO_TCHAR(PuidBuf));

		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem: CreateUser succeeded — ProductUserId: %s"), *FEEOSNativeOperationLease::SafeField(ProductUserId));

		AsyncTask(ENamedThreads::GameThread, [WeakSelf = Ctx->Self, ProductUserId, Request = Ctx->RequestId, SDKResult = Ctx->SDKResult]()
		{
			if (UEEOSAuthSubsystem* Self = WeakSelf.Get(); Self && Self->IsConnectRequestCurrent(Request))
			{
				Self->SetConnectLoginResult(true, ProductUserId, TEXT(""), Request, SDKResult);
			}
		});
	}
	else
	{
		FString ErrorMsg = FString::Printf(TEXT("CreateUser failed: %hs"), EOS_EResult_ToString(Data->ResultCode));
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem: %s"), *FEEOSNativeOperationLease::SafeField(ErrorMsg));

		AsyncTask(ENamedThreads::GameThread, [WeakSelf = Ctx->Self, ErrorMsg, Request = Ctx->RequestId, SDKResult = Ctx->SDKResult]()
		{
			if (UEEOSAuthSubsystem* Self = WeakSelf.Get(); Self && Self->IsConnectRequestCurrent(Request))
			{
				Self->SetConnectLoginResult(false, TEXT(""), ErrorMsg, Request, SDKResult);
			}
		});
	}
}

static void EOS_CALL OnConnectLoginCallbackStatic(const EOS_Connect_LoginCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;
	if (!Data || !EOS_EResult_IsOperationComplete(Data->ResultCode)) return;
	TUniquePtr<FConnectLoginContext> Ctx(static_cast<FConnectLoginContext*>(Data->ClientData));
	if (!Ctx->Self.IsValid() || !Ctx->Self->IsConnectRequestCurrent(Ctx->RequestId)) return; // Subsystem destroyed while in flight — context freed by TUniquePtr

	Ctx->SDKResult = ANSI_TO_TCHAR(EOS_EResult_ToString(Data->ResultCode));
	if (Data->ResultCode == EOS_EResult::EOS_Success)
	{
		// Login succeeded — extract ProductUserId
		char PuidBuf[EOS_PRODUCTUSERID_MAX_LENGTH + 1] = {};
		int32_t PuidLen = sizeof(PuidBuf);
		EOS_ProductUserId_ToString(Data->LocalUserId, PuidBuf, &PuidLen);
		FString ProductUserId(UTF8_TO_TCHAR(PuidBuf));

		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem: Connect login succeeded — ProductUserId: %s"), *FEEOSNativeOperationLease::SafeField(ProductUserId));

		AsyncTask(ENamedThreads::GameThread, [WeakSelf = Ctx->Self, ProductUserId, Request = Ctx->RequestId, SDKResult = Ctx->SDKResult]()
		{
			if (UEEOSAuthSubsystem* Self = WeakSelf.Get(); Self && Self->IsConnectRequestCurrent(Request))
			{
				Self->SetConnectLoginResult(true, ProductUserId, TEXT(""), Request, SDKResult);
			}
		});
	}
	else if (Data->ResultCode == EOS_EResult::EOS_InvalidUser && Ctx->bAutoCreateUser && Data->ContinuanceToken)
	{
		// First-time user — create a new ProductUser
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem: Connect login returned InvalidUser, creating new ProductUser..."));

		const EOS_HConnect Connect = Ctx->Platform.IsValid() ? EOS_Platform_GetConnectInterface(*Ctx->Platform) : nullptr;
		if (Connect)
		{
			EOS_Connect_CreateUserOptions Options = {};
			Options.ApiVersion = EOS_CONNECT_CREATEUSER_API_LATEST; Options.ContinuanceToken = Data->ContinuanceToken;
			EOS_Connect_CreateUser(Connect, &Options, Ctx.Release(), &OnConnectCreateUserComplete);
			return;
		}
		// Fallback if we can't get the handle
		AsyncTask(ENamedThreads::GameThread, [WeakSelf = Ctx->Self, Request = Ctx->RequestId, SDKResult = Ctx->SDKResult]()
		{
			if (UEEOSAuthSubsystem* Self = WeakSelf.Get(); Self && Self->IsConnectRequestCurrent(Request))
			{
				Self->SetConnectLoginResult(false, TEXT(""), TEXT("Failed to create user — Connect interface unavailable"), Request, SDKResult);
			}
		});
	}
	else if (Data->ResultCode == EOS_EResult::EOS_InvalidUser && !Ctx->bAutoCreateUser && Data->ContinuanceToken)
	{
		// Auto-create is disabled: hand the ContinuanceToken to the Connect subsystem so the
		// game can decide to LinkAccount (StoreContinuanceToken broadcasts OnInvalidUserDetected),
		// and report this login attempt as failed. The token is single-use and short-lived —
		// consume it promptly, never cache it across sessions.
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem: Connect login returned InvalidUser and auto-create is disabled — storing ContinuanceToken for LinkAccount"));

		EOS_ContinuanceToken ContinuanceToken = Data->ContinuanceToken;
		AsyncTask(ENamedThreads::GameThread, [WeakSelf = Ctx->Self, ContinuanceToken, Request = Ctx->RequestId, SDKResult = Ctx->SDKResult]()
		{
			if (UEEOSAuthSubsystem* Self = WeakSelf.Get(); Self && Self->IsConnectRequestCurrent(Request))
			{
				if (UGameInstance* GameInstance = Self->GetGameInstance())
				{
					if (UEEOSConnectSubsystem* ConnectSubsystem = GameInstance->GetSubsystem<UEEOSConnectSubsystem>())
					{
						ConnectSubsystem->StoreContinuanceToken(ContinuanceToken);
					}
				}
				Self->SetConnectLoginResult(false, TEXT(""),
					TEXT("Connect login returned EOS_InvalidUser — no product user exists for these credentials. A ContinuanceToken was stored: call LinkAccount to link an existing user, or enable bAutoCreateProductUser."), Request, SDKResult);
			}
		});
	}
	else
	{
		FString ErrorMsg = FString::Printf(TEXT("Connect login failed: %hs"), EOS_EResult_ToString(Data->ResultCode));
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem: %s"), *FEEOSNativeOperationLease::SafeField(ErrorMsg));

		AsyncTask(ENamedThreads::GameThread, [WeakSelf = Ctx->Self, ErrorMsg, Request = Ctx->RequestId, SDKResult = Ctx->SDKResult]()
		{
			if (UEEOSAuthSubsystem* Self = WeakSelf.Get(); Self && Self->IsConnectRequestCurrent(Request))
			{
				Self->SetConnectLoginResult(false, TEXT(""), ErrorMsg, Request, SDKResult);
			}
		});
	}
}

bool UEEOSAuthSubsystem::PerformConnectLogin(EEOSConnectLoginType LoginType, const FString& Token, const FString& DisplayName)
{
	// In-flight guard FIRST (R1/m8): a second raw Connect login while one is pending
	// would double-connect and last-writer-win CachedProductUserId. Log + reject; no
	// delegate fires for this call.
	if (bShuttingDown || bConnectLoginInFlight || IdentityLease.IsValid())
	{
		RejectOperation(TEXT("ConnectLogin"), EEOSOperationCode::Busy, TEXT("Another admitted request already owns this operation."));
		return false;
	}

	// Pre-flight failures below broadcast: no Connect login is in flight (guard above).
	const IEOSPlatformHandlePtr PlatformOwner = GetOwningEOSPlatform();
	if (!PlatformOwner.IsValid())
	{
		OnConnectLoginComplete.Broadcast(false, TEXT(""), TEXT("Owning EOS platform is unavailable.")); return false;
	}
	const EOS_HPlatform PlatformHandle = *PlatformOwner;
	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(PlatformHandle);
	if (!ConnectHandle)
	{
		OnConnectLoginComplete.Broadcast(false, TEXT(""), TEXT("Connect interface not available"));
		return false;
	}

	// Map our enum to EOS SDK credential type
	EOS_EExternalCredentialType CredType;
	switch (LoginType)
	{
	case EEOSConnectLoginType::Epic:
		CredType = EOS_EExternalCredentialType::EOS_ECT_EPIC;
		break;
	case EEOSConnectLoginType::Steam:
		CredType = EOS_EExternalCredentialType::EOS_ECT_STEAM_SESSION_TICKET;
		break;
	case EEOSConnectLoginType::PSN:
		CredType = EOS_EExternalCredentialType::EOS_ECT_PSN_ID_TOKEN;
		break;
	case EEOSConnectLoginType::XboxLive:
		CredType = EOS_EExternalCredentialType::EOS_ECT_XBL_XSTS_TOKEN;
		break;
	case EEOSConnectLoginType::Nintendo:
		CredType = EOS_EExternalCredentialType::EOS_ECT_NINTENDO_ID_TOKEN;
		break;
	case EEOSConnectLoginType::Discord:
		CredType = EOS_EExternalCredentialType::EOS_ECT_DISCORD_ACCESS_TOKEN;
		break;
	case EEOSConnectLoginType::DeviceId:
		CredType = EOS_EExternalCredentialType::EOS_ECT_DEVICEID_ACCESS_TOKEN;
		break;
	case EEOSConnectLoginType::OpenID:
		CredType = EOS_EExternalCredentialType::EOS_ECT_OPENID_ACCESS_TOKEN;
		break;
	case EEOSConnectLoginType::Apple:
		CredType = EOS_EExternalCredentialType::EOS_ECT_APPLE_ID_TOKEN;
		break;
	case EEOSConnectLoginType::Google:
		CredType = EOS_EExternalCredentialType::EOS_ECT_GOOGLE_ID_TOKEN;
		break;
	default:
		CredType = EOS_EExternalCredentialType::EOS_ECT_EPIC;
		break;
	}

	// Build credentials
	EOS_Connect_Credentials Credentials = {};
	Credentials.ApiVersion = EOS_CONNECT_CREDENTIALS_API_LATEST;
	Credentials.Type = CredType;

	FTCHARToUTF8 TokenUtf8(*Token);
	Credentials.Token = Token.IsEmpty() ? nullptr : TokenUtf8.Get();

	// User login info (required for DeviceId, Apple, Google, Nintendo)
	EOS_Connect_UserLoginInfo UserLoginInfo = {};
	UserLoginInfo.ApiVersion = EOS_CONNECT_USERLOGININFO_API_LATEST;
	FTCHARToUTF8 DisplayNameUtf8(*DisplayName);
	UserLoginInfo.DisplayName = DisplayName.IsEmpty() ? nullptr : DisplayNameUtf8.Get();
	UserLoginInfo.NsaIdToken = nullptr;

	// Login options
	EOS_Connect_LoginOptions LoginOptions = {};
	LoginOptions.ApiVersion = EOS_CONNECT_LOGIN_API_LATEST;
	LoginOptions.Credentials = &Credentials;

	bool bNeedsUserInfo = (LoginType == EEOSConnectLoginType::DeviceId ||
	                       LoginType == EEOSConnectLoginType::Apple ||
	                       LoginType == EEOSConnectLoginType::Google ||
	                       LoginType == EEOSConnectLoginType::Nintendo);
	LoginOptions.UserLoginInfo = bNeedsUserInfo ? &UserLoginInfo : nullptr;

	// Check if auto-create is enabled
	const UEEOSSettings* Settings = GetEOSSettings();
	bool bAutoCreate = Settings ? Settings->bAutoCreateProductUser : true;

	if (!ConnectLease.IsValid())
	{
		IOnlineSubsystem* OSS = GetExistingEOSOnlineSubsystem();
		const auto Identity = OSS ? OSS->GetIdentityInterface() : IOnlineIdentityPtr();
		if (!Identity.IsValid() || !ConnectLease.TryAcquire(Identity.Get(), TEXT("Identity0"), this, TEXT("ConnectLogin")))
		{
			RejectOperation(TEXT("ConnectLogin"), EEOSOperationCode::Busy, TEXT("Another plugin identity operation is pending.")); return false;
		}
		ConnectOperationIdentity = Identity;
		ActiveSDKConnectRequest = BeginOperation(TEXT("ConnectLogin"), FString(), ConnectLease.GetRequestId());
		SDKConnectContext = CaptureEOSContext();
	}
	FConnectLoginContext* Ctx = new FConnectLoginContext{this, bAutoCreate, PlatformOwner, ConnectLease, ActiveSDKConnectRequest, ConnectOperationIdentity, FString()};

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem: Performing EOS Connect login (type=%d)..."), (int32)LoginType);

	// Arm the in-flight guard — every completion path (login success/failure, the
	// CreateUser chain, the ContinuanceToken hand-off) funnels through
	// SetConnectLoginResult, which releases it before broadcasting.
	bConnectLoginInFlight = true;

	EOS_Connect_Login(ConnectHandle, &LoginOptions, Ctx, &OnConnectLoginCallbackStatic);
	return true;
}

#endif // WITH_EOS_SDK

// ── SetConnectLoginResult ────────────────────────────────────────────────────

void UEEOSAuthSubsystem::SetConnectLoginResult(bool bSuccess, const FString& ProductUserId, const FString& Error, int64 RequestId, const FString& SDKResult)
{
	if (bShuttingDown || (RequestId && RequestId != ActiveSDKConnectRequest)) return;
	bConnectLoginInFlight = false; ActiveSDKConnectRequest = 0; ConnectLease.Reset(); ConnectOperationIdentity.Reset();
	bConnectedToGameServices = bSuccess && !ProductUserId.IsEmpty();
	CachedProductUserId = bConnectedToGameServices ? ProductUserId : FString();
	auto Outcome = CompleteOperation(TEXT("ConnectLogin"), bConnectedToGameServices,
		bConnectedToGameServices ? EEOSOperationCode::Succeeded : EEOSOperationCode::NativeFailure,
		Error.IsEmpty() ? TEXT("Connect login completed.") : Error, FString(), FString(), SDKResult.IsEmpty() ? EEOSResultSource::Plugin : EEOSResultSource::SDKCallback, SDKResult);
	FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
	OnConnectLoginComplete.Broadcast(bConnectedToGameServices, CachedProductUserId, Error);
	OnOperationCompleted.Broadcast(Outcome);
}

// ── Queries ──────────────────────────────────────────────────────────────────

EEOSLoginStatus UEEOSAuthSubsystem::GetLoginStatus() const
{
	return CurrentLoginStatus;
}

FString UEEOSAuthSubsystem::GetLoggedInUserId() const
{
	if (!IsEOSAvailable()) return FString();

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid()) return FString();

	FUniqueNetIdPtr UserId = IdentityInterface->GetUniquePlayerId(0);
	return UserId.IsValid() ? UserId->ToString() : FString();
}

FString UEEOSAuthSubsystem::GetDisplayName() const
{
	IOnlineSubsystem* OSS = GetExistingEOSOnlineSubsystem();
	const auto Identity = OSS ? OSS->GetIdentityInterface() : IOnlineIdentityPtr();
	const FString EpicName = EEOSIdentity::SafeLocalNickname(Identity);
	if (!EpicName.IsEmpty()) return EpicName;
	IOnlineSubsystem* Platform = IOnlineSubsystem::GetByPlatform();
	const auto PlatformIdentity = Platform && Platform != OSS ? Platform->GetIdentityInterface() : IOnlineIdentityPtr();
	return PlatformIdentity.IsValid() && PlatformIdentity->GetLoginStatus(0) == ELoginStatus::LoggedIn ? PlatformIdentity->GetPlayerNickname(0) : FString();
}

bool UEEOSAuthSubsystem::IsLoggedIn() const
{
	IOnlineSubsystem* OSS = GetExistingEOSOnlineSubsystem();
	const auto Identity = OSS ? OSS->GetIdentityInterface() : IOnlineIdentityPtr();
	const auto Local = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : FUniqueNetIdPtr();
	return Local.IsValid() && Local->IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn;
}

FString UEEOSAuthSubsystem::GetAuthToken() const
{
	if (!IsEOSAvailable()) return FString();

	IOnlineSubsystem* EOSSub = GetExistingEOSOnlineSubsystem();
	IOnlineIdentityPtr IdentityInterface = EOSSub ? EOSSub->GetIdentityInterface() : IOnlineIdentityPtr();
	if (!EEOSIdentity::HasLocalEpicAccount(IdentityInterface)) return FString();

	return IdentityInterface->GetAuthToken(0);
}

EEOSLoginType UEEOSAuthSubsystem::GetCurrentLoginType() const
{
	return UsedLoginType;
}

FString UEEOSAuthSubsystem::GetProductUserId() const
{
	IOnlineSubsystem* OSS = GetExistingEOSOnlineSubsystem();
	const auto Identity = OSS ? OSS->GetIdentityInterface() : IOnlineIdentityPtr();
	const auto Local = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : FUniqueNetIdPtr();
	if (Local.IsValid() && Local->IsValid() && Identity->GetLoginStatus(0) == ELoginStatus::LoggedIn)
	{
		const FString NativePuid = UEEOSBlueprintLibrary::ExtractProductUserId(Local->ToString());
		if (!NativePuid.IsEmpty()) return NativePuid;
	}
	return bConnectedToGameServices ? CachedProductUserId : FString();
}

bool UEEOSAuthSubsystem::IsConnectedToGameServices() const
{
	return bConnectedToGameServices;
}

// ── Handlers ─────────────────────────────────────────────────────────────────

void UEEOSAuthSubsystem::HandleLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
	if (LocalUserNum != 0 || !LoginDelegateHandle.IsValid())
	{ LogCallbackDisposition(TEXT("Login"), IdentityLease.GetRequestId(), LocalUserNum != 0 ? TEXT("DifferentOwner") : TEXT("Duplicate")); return; }
	const bool bCurrent = bShuttingDown || IsEOSContextCurrent(NativeIdentityContext, false);
	LogCallbackDisposition(TEXT("Login"), IdentityLease.GetRequestId(), bShuttingDown ? TEXT("ShutdownInternalOnly") : bCurrent ? TEXT("Consumed") : TEXT("StaleGeneration"), NativeIdentityContext.Generation);
	if (OperationIdentity.IsValid()) OperationIdentity->ClearOnLoginCompleteDelegate_Handle(0, LoginDelegateHandle);
	LoginDelegateHandle.Reset(); IdentityLease.Reset(); OperationIdentity.Reset(); bNativeIdentitySubmitted = false;
	const bool bSubmissionRejected = bIdentitySubmissionRejected; bIdentitySubmissionRejected = false;
	const auto Local = NativeIdentityContext.Identity.IsValid() ? NativeIdentityContext.Identity->GetUniquePlayerId(0) : FUniqueNetIdPtr();
	const bool bReady = bCurrent && bWasSuccessful && IsLoggedIn() && UserId.IsValid() && Local.IsValid() && *Local == UserId;
	CurrentLoginStatus = IsLoggedIn() ? EEOSLoginStatus::LoggedIn : EEOSLoginStatus::Failed;
	const FString ResultMessage = !bCurrent ? TEXT("Original identity/platform retired.") : !Error.IsEmpty() ? Error : bReady ? TEXT("Native login completed.") : TEXT("Native login did not leave a usable identity.");
	const auto Outcome = CompleteOperation(TEXT("Login"), bReady, !bCurrent ? EEOSOperationCode::Canceled : bReady ? EEOSOperationCode::Succeeded : bSubmissionRejected ? EEOSOperationCode::NativeStartRejected : EEOSOperationCode::NativeFailure,
		ResultMessage, UserId.IsValid() ? UserId.ToString() : FString(), bSubmissionRejected ? FString() : bWasSuccessful ? TEXT("Success") : TEXT("Failure"),
		bSubmissionRejected ? EEOSResultSource::Plugin : EEOSResultSource::NativeCallback);
	FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
	if (bShuttingDown) return;
	if (bReady) AutoConnectLoginAfterAuth();
	OnLoginStatusChanged.Broadcast(CurrentLoginStatus); OnLoginComplete.Broadcast(bReady, bReady ? Error : ResultMessage); OnOperationCompleted.Broadcast(Outcome);
}

void UEEOSAuthSubsystem::HandleLogoutComplete(int32 LocalUserNum, bool bWasSuccessful)
{
	if (LocalUserNum != 0 || !LogoutDelegateHandle.IsValid())
	{ LogCallbackDisposition(TEXT("Logout"), IdentityLease.GetRequestId(), LocalUserNum != 0 ? TEXT("DifferentOwner") : TEXT("Duplicate")); return; }
	const bool bCurrent = bShuttingDown || IsEOSContextCurrent(NativeIdentityContext, false);
	LogCallbackDisposition(TEXT("Logout"), IdentityLease.GetRequestId(), bShuttingDown ? TEXT("ShutdownInternalOnly") : bCurrent ? TEXT("Consumed") : TEXT("StaleGeneration"), NativeIdentityContext.Generation);
	// Consume the DeletePersistentAuth marker up front (re-entrancy safe: a listener
	// starting a new operation from inside a broadcast must see clean state).
	const bool bNotifyPersistentAuthDeleted = bPendingPersistentAuthDeleteViaLogout;
	bPendingPersistentAuthDeleteViaLogout = false;

	if (bWasSuccessful && bCurrent)
	{
		CurrentLoginStatus = EEOSLoginStatus::NotLoggedIn;
		bConnectedToGameServices = false;
		CachedProductUserId.Empty();
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem: Logout successful"));
	}
	else
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem: Logout failed"));
	}

	// Clear the engine delegate registration AND the in-progress guard BEFORE
	// broadcasting (see HandleLoginComplete — same R1/m6 reasoning).
	if (OperationIdentity.IsValid()) OperationIdentity->ClearOnLogoutCompleteDelegate_Handle(0, LogoutDelegateHandle);
	LogoutDelegateHandle.Reset(); IdentityLease.Reset(); OperationIdentity.Reset(); bNativeIdentitySubmitted = false;
	CurrentLoginStatus = IsLoggedIn() ? EEOSLoginStatus::LoggedIn : EEOSLoginStatus::NotLoggedIn;
	const bool bSubmissionRejected = bIdentitySubmissionRejected; bIdentitySubmissionRejected = false;
	const bool bLoggedOut = bCurrent && bWasSuccessful && !IsLoggedIn();
	const auto Outcome = CompleteOperation(TEXT("Logout"), bLoggedOut, !bCurrent ? EEOSOperationCode::Canceled : bLoggedOut ? EEOSOperationCode::Succeeded : bSubmissionRejected ? EEOSOperationCode::NativeStartRejected : EEOSOperationCode::NativeFailure,
		bLoggedOut ? TEXT("Logged out.") : bSubmissionRejected ? TEXT("Native logout refused submission.") : TEXT("Native logout did not clear the identity."), FString(),
		bSubmissionRejected ? FString() : bWasSuccessful ? TEXT("Success") : TEXT("Failure"), bSubmissionRejected ? EEOSResultSource::Plugin : EEOSResultSource::NativeCallback);
	FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
	if (bShuttingDown) return;
	OnLoginStatusChanged.Broadcast(CurrentLoginStatus);
	OnLogoutComplete.Broadcast();

	// The engine's logout path deletes the persistent credentials
	// (FUserManagerEOS::CallEOSAuthLogout → EOS_Auth_DeletePersistentAuth), so a
	// successful DeletePersistentAuth-initiated logout means the deletion happened.
	if (bNotifyPersistentAuthDeleted)
	{
		OnPersistentAuthDeleted.Broadcast(bWasSuccessful && bCurrent);
	}
	OnOperationCompleted.Broadcast(Outcome);
}

void UEEOSAuthSubsystem::AutoConnectLoginAfterAuth()
{
	const UEEOSSettings* Settings = GetEOSSettings();
	if (!Settings || !Settings->bAutoConnectLoginOnStart) return;

	// m8: don't double-connect — skip the auto-chain if we already have a Connect
	// session or one is being established.
	if (bConnectedToGameServices)
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem: Auto-chain Connect login skipped — already connected to Game Services"));
		return;
	}
	if (bConnectLoginInFlight)
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem: Auto-chain Connect login skipped — a Connect login is already pending"));
		return;
	}

	// With bUseEOSConnect=true in [/Script/OnlineSubsystemEOS.EOSSettings], the ENGINE's login
	// flow already performed the Connect login — the identity net id arrives as "EAS|PUID" with
	// a real PUID half. Re-logging-in through the raw SDK here is redundant and actually fails
	// against the backend (product-id mismatch on the re-used auth token). Adopt the engine's
	// Connect session instead; only fall back to the raw chain for EAS-only configurations.
	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		if (IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface())
		{
			if (FUniqueNetIdPtr LocalId = IdentityInterface->GetUniquePlayerId(0))
			{
				const FString EnginePuid = UEEOSBlueprintLibrary::ExtractProductUserId(LocalId->ToString());
				if (!EnginePuid.IsEmpty())
				{
					UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem: Adopting the engine's Connect session (PUID: %s) — no raw Connect login needed"), *FEEOSNativeOperationLease::SafeField(EnginePuid));
					SetConnectLoginResult(true, EnginePuid, TEXT(""));
					return;
				}
			}
		}
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem: Auto-chaining Connect login after Auth success..."));

	// After Epic Auth login, use the Epic ID Token to do Connect login
	// This gets us a ProductUserId from the already-authenticated Epic account
	ConnectLogin(EEOSConnectLoginType::Epic, GetAuthToken());
}


bool UEEOSAuthSubsystem::TickNativeIdentity(float)
{
	if (bShuttingDown) return false;
	if (ConnectLease.IsValid() && !IsEOSContextCurrent(SDKConnectContext))
	{
		const auto Outcome = CompleteOperation(TEXT("ConnectLogin"), false, EEOSOperationCode::Canceled, TEXT("Original identity/platform retired; SDK completion remains owned by its retained context."));
		ActiveSDKConnectRequest = 0; bConnectLoginInFlight = false; ConnectLease.Reset(); ConnectOperationIdentity.Reset();
		CachedProductUserId.Empty(); bConnectedToGameServices = false;
		FEEOSOutcomeDispatchScope Dispatch(this, Outcome);
		OnConnectLoginComplete.Broadcast(false, FString(), FString()); OnOperationCompleted.Broadcast(Outcome);
	}
	if (IdentityLease.IsValid() || ConnectLease.IsValid()) return true;
	const auto Status = IsLoggedIn() ? EEOSLoginStatus::LoggedIn : EEOSLoginStatus::NotLoggedIn;
	if (CurrentLoginStatus != Status)
	{
		CurrentLoginStatus = Status;
		if (Status == EEOSLoginStatus::NotLoggedIn) { CachedProductUserId.Empty(); bConnectedToGameServices = false; }
		OnLoginStatusChanged.Broadcast(Status);
	}
	if (Status == EEOSLoginStatus::LoggedIn)
	{
		IOnlineSubsystem* OSS = GetExistingEOSOnlineSubsystem();
		const auto Identity = OSS ? OSS->GetIdentityInterface() : IOnlineIdentityPtr();
		const auto Local = Identity.IsValid() ? Identity->GetUniquePlayerId(0) : FUniqueNetIdPtr();
		const FString Puid = Local.IsValid() ? UEEOSBlueprintLibrary::ExtractProductUserId(Local->ToString()) : FString();
		CachedProductUserId = GetEOSReadiness().bConnectLoggedIn ? Puid : FString(); bConnectedToGameServices = !CachedProductUserId.IsEmpty();
	}
	return true;
}
