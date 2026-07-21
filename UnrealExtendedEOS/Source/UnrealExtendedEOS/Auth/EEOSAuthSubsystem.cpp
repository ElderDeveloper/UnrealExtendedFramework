// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSAuthSubsystem.h"
#include "Auth/EEOSConnectSubsystem.h"
#include "Shared/EEOSSettings.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "UnrealExtendedEOS.h"
#include "Engine/GameInstance.h"
#include "TimerManager.h"

#if WITH_EOS_SDK
#include "IEOSSDKManager.h"
#include "eos_connect.h"
#include "eos_auth.h"
#include "eos_sdk.h"
#endif

// ── Initialize / Deinitialize ────────────────────────────────────────────────

void UEEOSAuthSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UEEOSSettings* Settings = GetEOSSettings();
	if (Settings && (Settings->bAutoLoginOnStart || Settings->bAutoConnectLoginOnStart))
	{
		// Defer one tick so Blueprints (GameInstance, early widgets) can bind to the login
		// delegates before any auto-login result broadcasts. The GameInstance timer manager
		// is safe to use here: it is created in the UGameInstance constructor, while
		// subsystems initialize later inside UGameInstance::Init.
		GetGameInstance()->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				KickOffAutoLogin();
			}));
	}
}

void UEEOSAuthSubsystem::KickOffAutoLogin()
{
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
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAuthSubsystem::Login — DeviceCode login is not supported by this EOS SDK (superseded by ExternalAuth). Use LoginWithExternalAuth instead."));
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
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAuthSubsystem::Login — ExternalAuth requires the external token type. Call LoginWithExternalAuth(CredentialType, Token) instead."));
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
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAuthSubsystem::LoginWithExternalAuth — Invalid external credential type (%d)"), static_cast<int32>(CredentialType));
		BroadcastLoginPreflightFailure(TEXT("Invalid external credential type"));
		return false;
	}

	if (Token.IsEmpty())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAuthSubsystem::LoginWithExternalAuth — Token is empty (type=%s)"), *TokenType);
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
	if (LoginDelegateHandle.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem: Pre-flight login failure ('%s') while another login is in flight — not broadcasting OnLoginComplete for this call"), *Error);
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
	if (LoginDelegateHandle.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem::Login — A login is already in progress, rejecting this call (no delegate will fire for it)"));
		return false;
	}

	// The engine rejects Login while logged in ("Already logged in") — guard here so the
	// rejection can't flip CurrentLoginStatus from LoggedIn to Failed. Log-only (R1).
	if (IsLoggedIn())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem::Login — Already logged in, rejecting this call (no delegate will fire for it)"));
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
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAuthSubsystem::Login — Identity interface is not available"));
		OnLoginComplete.Broadcast(false, TEXT("Identity interface not available"));
		return false;
	}

	CurrentLoginStatus = EEOSLoginStatus::LoggingIn;
	UsedLoginType = LoginType;
	OnLoginStatusChanged.Broadcast(CurrentLoginStatus);

	LoginDelegateHandle = IdentityInterface->AddOnLoginCompleteDelegate_Handle(0, FOnLoginCompleteDelegate::CreateUObject(this, &UEEOSAuthSubsystem::HandleLoginComplete));
	IdentityInterface->Login(0, Credentials);

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem::Login — Attempting login with type: %s"), *Credentials.Type);
	return true;
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
		return Login(Settings->DefaultLoginType);
	}

	UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAuthSubsystem::LoginWithDefaults — Settings not available"));
	BroadcastLoginPreflightFailure(TEXT("EOS Settings not available"));
	return false;
}

bool UEEOSAuthSubsystem::Logout()
{
	// In-progress guard FIRST (R1): don't stack a second delegate registration and don't
	// broadcast for the rejected duplicate. The pending logout's completion is the single
	// OnLogoutComplete broadcast for both calls (the delegate has no failure payload to
	// report a per-call rejection with).
	if (LogoutDelegateHandle.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem::Logout — A logout is already in progress, ignoring duplicate call (the pending logout will broadcast OnLogoutComplete)"));
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
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAuthSubsystem::Logout — Identity interface is not available"));
		OnLogoutComplete.Broadcast();
		return false;
	}

	LogoutDelegateHandle = IdentityInterface->AddOnLogoutCompleteDelegate_Handle(0, FOnLogoutCompleteDelegate::CreateUObject(this, &UEEOSAuthSubsystem::HandleLogoutComplete));
	IdentityInterface->Logout(0);

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
	if (LogoutDelegateHandle.IsValid())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem::DeletePersistentAuth — A logout is already in progress, rejecting this call (no delegate will fire for it)"));
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
		LogoutDelegateHandle = IdentityInterface->AddOnLogoutCompleteDelegate_Handle(0, FOnLogoutCompleteDelegate::CreateUObject(this, &UEEOSAuthSubsystem::HandleLogoutComplete));
		IdentityInterface->Logout(0);

		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem::DeletePersistentAuth — Logging out to clear persistent auth credentials"));
		return true;
	}

	// Sessionless path (the primary use case — clearing 'remember me' from a login
	// screen): the engine's logout route needs a live session and would delete nothing.
	// Do what the engine itself does in sessionless contexts: call
	// EOS_Auth_DeletePersistentAuth directly on the platform handle.
#if WITH_EOS_SDK
	EOS_HPlatform PlatformHandle = GetPlatformHandle();
	if (!PlatformHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAuthSubsystem::DeletePersistentAuth — Platform handle not available"));
		OnPersistentAuthDeleted.Broadcast(false);
		return false;
	}

	EOS_HAuth AuthHandle = EOS_Platform_GetAuthInterface(PlatformHandle);
	if (!AuthHandle)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAuthSubsystem::DeletePersistentAuth — Auth interface not available"));
		OnPersistentAuthDeleted.Broadcast(false);
		return false;
	}

	EOS_Auth_DeletePersistentAuthOptions Options = {};
	Options.ApiVersion = EOS_AUTH_DELETEPERSISTENTAUTH_API_LATEST;
	// RefreshToken is Console-only (the caller manages token storage there); on Desktop
	// and Mobile it must be NULL (eos_auth_types.h, EOS_Auth_DeletePersistentAuthOptions).
	Options.RefreshToken = nullptr;

	struct FDeletePersistentAuthContext
	{
		TWeakObjectPtr<UEEOSAuthSubsystem> Self;
	};

	struct FDeletePersistentAuthCallbackWrapper
	{
		static void EOS_CALL Callback(const EOS_Auth_DeletePersistentAuthCallbackInfo* Data)
		{
			if (!Data || !Data->ClientData) return;
			TUniquePtr<FDeletePersistentAuthContext> Ctx(static_cast<FDeletePersistentAuthContext*>(Data->ClientData));
			if (!Ctx->Self.IsValid()) return; // Subsystem destroyed while in flight — context freed by TUniquePtr

			const bool bSuccess = (Data->ResultCode == EOS_EResult::EOS_Success);
			if (bSuccess)
			{
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem::DeletePersistentAuth — Persistent auth credentials deleted"));
			}
			else
			{
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem::DeletePersistentAuth — Failed: %hs"), EOS_EResult_ToString(Data->ResultCode));
			}

			AsyncTask(ENamedThreads::GameThread, [WeakSelf = Ctx->Self, bSuccess]()
			{
				if (UEEOSAuthSubsystem* Self = WeakSelf.Get())
				{
					Self->OnPersistentAuthDeleted.Broadcast(bSuccess);
				}
			});
		}
	};

	EOS_Auth_DeletePersistentAuth(AuthHandle, &Options, new FDeletePersistentAuthContext{this}, &FDeletePersistentAuthCallbackWrapper::Callback);

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem::DeletePersistentAuth — No logged-in user, deleting persistent auth credentials directly via EOS_Auth_DeletePersistentAuth"));
	return true;
#else
	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem::DeletePersistentAuth — EOS SDK not available and no logged-in user"));
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
	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem::ConnectLogin — EOS SDK not available"));
	OnConnectLoginComplete.Broadcast(false, TEXT(""), TEXT("EOS SDK not available"));
	return false;
#endif
}

bool UEEOSAuthSubsystem::ConnectLoginWithDeviceId(const FString& DisplayName)
{
#if WITH_EOS_SDK
	// In-flight guard FIRST (R1/m8): the create→login chain counts as a Connect login in
	// flight. Log + reject; no delegate fires for this call.
	if (bConnectLoginInFlight)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem::ConnectLoginWithDeviceId — A Connect login is already in progress, rejecting this call (no delegate will fire for it)"));
		return false;
	}

	IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
	if (!SDKManager)
	{
		OnConnectLoginComplete.Broadcast(false, TEXT(""), TEXT("SDK Manager not available"));
		return false;
	}

	TArray<IEOSPlatformHandlePtr> Platforms = SDKManager->GetActivePlatforms();
	if (Platforms.Num() == 0)
	{
		OnConnectLoginComplete.Broadcast(false, TEXT(""), TEXT("No active EOS platform"));
		return false;
	}

	EOS_HPlatform PlatformHandle = *Platforms[0];
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

	struct FDeviceIdContext
	{
		TWeakObjectPtr<UEEOSAuthSubsystem> Self;
		FString DisplayName;
	};
	FDeviceIdContext* Ctx = new FDeviceIdContext{this, DisplayName};

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem: Creating DeviceId..."));


	// Use a static EOS_CALL wrapper function for the callback
	struct FDeviceIdCallbackWrapper
	{
		static void EOS_CALL Callback(const EOS_Connect_CreateDeviceIdCallbackInfo* Data)
		{
			TUniquePtr<FDeviceIdContext> C(static_cast<FDeviceIdContext*>(Data->ClientData));
			if (!C.IsValid()) return;
			if (!C->Self.IsValid()) return; // Subsystem destroyed while in flight — context freed by TUniquePtr

			if (Data->ResultCode == EOS_EResult::EOS_Success ||
				Data->ResultCode == EOS_EResult::EOS_DuplicateNotAllowed)
			{
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem: DeviceId ready, performing Connect login..."));

				AsyncTask(ENamedThreads::GameThread, [WeakSelf = C->Self, DisplayName = C->DisplayName]()
				{
					if (UEEOSAuthSubsystem* Self = WeakSelf.Get())
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
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem: %s"), *ErrorMsg);

				AsyncTask(ENamedThreads::GameThread, [WeakSelf = C->Self, ErrorMsg]()
				{
					if (UEEOSAuthSubsystem* Self = WeakSelf.Get())
					{
						Self->SetConnectLoginResult(false, TEXT(""), ErrorMsg);
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
			UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem::ConnectLoginWithDefaults — Settings not available and a Connect login is in flight — not broadcasting for this call"));
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
	UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAuthSubsystem: %s"), *ErrorMsg);
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
};

static void EOS_CALL OnConnectCreateUserComplete(const EOS_Connect_CreateUserCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;
	TUniquePtr<FConnectLoginContext> Ctx(static_cast<FConnectLoginContext*>(Data->ClientData));
	if (!Ctx->Self.IsValid()) return; // Subsystem destroyed while in flight — context freed by TUniquePtr

	if (Data->ResultCode == EOS_EResult::EOS_Success)
	{
		char PuidBuf[EOS_PRODUCTUSERID_MAX_LENGTH + 1] = {};
		int32_t PuidLen = sizeof(PuidBuf);
		EOS_ProductUserId_ToString(Data->LocalUserId, PuidBuf, &PuidLen);
		FString ProductUserId(UTF8_TO_TCHAR(PuidBuf));

		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem: CreateUser succeeded — ProductUserId: %s"), *ProductUserId);

		AsyncTask(ENamedThreads::GameThread, [WeakSelf = Ctx->Self, ProductUserId]()
		{
			if (UEEOSAuthSubsystem* Self = WeakSelf.Get())
			{
				Self->SetConnectLoginResult(true, ProductUserId, TEXT(""));
			}
		});
	}
	else
	{
		FString ErrorMsg = FString::Printf(TEXT("CreateUser failed: %hs"), EOS_EResult_ToString(Data->ResultCode));
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem: %s"), *ErrorMsg);

		AsyncTask(ENamedThreads::GameThread, [WeakSelf = Ctx->Self, ErrorMsg]()
		{
			if (UEEOSAuthSubsystem* Self = WeakSelf.Get())
			{
				Self->SetConnectLoginResult(false, TEXT(""), ErrorMsg);
			}
		});
	}
}

static void EOS_CALL OnConnectLoginCallbackStatic(const EOS_Connect_LoginCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;
	TUniquePtr<FConnectLoginContext> Ctx(static_cast<FConnectLoginContext*>(Data->ClientData));
	if (!Ctx->Self.IsValid()) return; // Subsystem destroyed while in flight — context freed by TUniquePtr

	if (Data->ResultCode == EOS_EResult::EOS_Success)
	{
		// Login succeeded — extract ProductUserId
		char PuidBuf[EOS_PRODUCTUSERID_MAX_LENGTH + 1] = {};
		int32_t PuidLen = sizeof(PuidBuf);
		EOS_ProductUserId_ToString(Data->LocalUserId, PuidBuf, &PuidLen);
		FString ProductUserId(UTF8_TO_TCHAR(PuidBuf));

		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem: Connect login succeeded — ProductUserId: %s"), *ProductUserId);

		AsyncTask(ENamedThreads::GameThread, [WeakSelf = Ctx->Self, ProductUserId]()
		{
			if (UEEOSAuthSubsystem* Self = WeakSelf.Get())
			{
				Self->SetConnectLoginResult(true, ProductUserId, TEXT(""));
			}
		});
	}
	else if (Data->ResultCode == EOS_EResult::EOS_InvalidUser && Ctx->bAutoCreateUser && Data->ContinuanceToken)
	{
		// First-time user — create a new ProductUser
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem: Connect login returned InvalidUser, creating new ProductUser..."));

		// Get the Connect handle to call CreateUser
		IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
		if (SDKManager)
		{
			TArray<IEOSPlatformHandlePtr> Platforms = SDKManager->GetActivePlatforms();
			if (Platforms.Num() > 0)
			{
				EOS_HPlatform PH = *Platforms[0];
				EOS_HConnect CH = EOS_Platform_GetConnectInterface(PH);
				if (CH)
				{
					EOS_Connect_CreateUserOptions CreateOpts = {};
					CreateOpts.ApiVersion = EOS_CONNECT_CREATEUSER_API_LATEST;
					CreateOpts.ContinuanceToken = Data->ContinuanceToken;

					EOS_Connect_CreateUser(CH, &CreateOpts, Ctx.Release(), &OnConnectCreateUserComplete);
					return; // Ownership handed off — CreateUser callback frees the context
				}
			}
		}

		// Fallback if we can't get the handle
		AsyncTask(ENamedThreads::GameThread, [WeakSelf = Ctx->Self]()
		{
			if (UEEOSAuthSubsystem* Self = WeakSelf.Get())
			{
				Self->SetConnectLoginResult(false, TEXT(""), TEXT("Failed to create user — Connect interface unavailable"));
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
		AsyncTask(ENamedThreads::GameThread, [WeakSelf = Ctx->Self, ContinuanceToken]()
		{
			if (UEEOSAuthSubsystem* Self = WeakSelf.Get())
			{
				if (UGameInstance* GameInstance = Self->GetGameInstance())
				{
					if (UEEOSConnectSubsystem* ConnectSubsystem = GameInstance->GetSubsystem<UEEOSConnectSubsystem>())
					{
						ConnectSubsystem->StoreContinuanceToken(ContinuanceToken);
					}
				}
				Self->SetConnectLoginResult(false, TEXT(""),
					TEXT("Connect login returned EOS_InvalidUser — no product user exists for these credentials. A ContinuanceToken was stored: call LinkAccount to link an existing user, or enable bAutoCreateProductUser."));
			}
		});
	}
	else
	{
		FString ErrorMsg = FString::Printf(TEXT("Connect login failed: %hs"), EOS_EResult_ToString(Data->ResultCode));
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem: %s"), *ErrorMsg);

		AsyncTask(ENamedThreads::GameThread, [WeakSelf = Ctx->Self, ErrorMsg]()
		{
			if (UEEOSAuthSubsystem* Self = WeakSelf.Get())
			{
				Self->SetConnectLoginResult(false, TEXT(""), ErrorMsg);
			}
		});
	}
}

bool UEEOSAuthSubsystem::PerformConnectLogin(EEOSConnectLoginType LoginType, const FString& Token, const FString& DisplayName)
{
	// In-flight guard FIRST (R1/m8): a second raw Connect login while one is pending
	// would double-connect and last-writer-win CachedProductUserId. Log + reject; no
	// delegate fires for this call.
	if (bConnectLoginInFlight)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem::PerformConnectLogin — A Connect login is already in progress, rejecting this call (no delegate will fire for it)"));
		return false;
	}

	// Pre-flight failures below broadcast: no Connect login is in flight (guard above).
	IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
	if (!SDKManager)
	{
		OnConnectLoginComplete.Broadcast(false, TEXT(""), TEXT("SDK Manager not available"));
		return false;
	}

	TArray<IEOSPlatformHandlePtr> Platforms = SDKManager->GetActivePlatforms();
	if (Platforms.Num() == 0)
	{
		OnConnectLoginComplete.Broadcast(false, TEXT(""), TEXT("No active EOS platform"));
		return false;
	}

	EOS_HPlatform PlatformHandle = *Platforms[0];
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

	FConnectLoginContext* Ctx = new FConnectLoginContext{this, bAutoCreate};

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

void UEEOSAuthSubsystem::SetConnectLoginResult(bool bSuccess, const FString& ProductUserId, const FString& Error)
{
	// Release the in-flight guard BEFORE broadcasting so listeners can retry/chain a new
	// Connect login from inside the completion broadcast (R1).
	bConnectLoginInFlight = false;

	if (bSuccess)
	{
		CachedProductUserId = ProductUserId;
		bConnectedToGameServices = true;
	}
	OnConnectLoginComplete.Broadcast(bSuccess, ProductUserId, Error);
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
	if (!IsEOSAvailable()) return FString();

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid()) return FString();

	return IdentityInterface->GetPlayerNickname(0);
}

bool UEEOSAuthSubsystem::IsLoggedIn() const
{
	return CurrentLoginStatus == EEOSLoginStatus::LoggedIn;
}

FString UEEOSAuthSubsystem::GetAuthToken() const
{
	if (!IsEOSAvailable()) return FString();

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid()) return FString();

	return IdentityInterface->GetAuthToken(0);
}

EEOSLoginType UEEOSAuthSubsystem::GetCurrentLoginType() const
{
	return UsedLoginType;
}

FString UEEOSAuthSubsystem::GetProductUserId() const
{
	return CachedProductUserId;
}

bool UEEOSAuthSubsystem::IsConnectedToGameServices() const
{
	return bConnectedToGameServices;
}

// ── Handlers ─────────────────────────────────────────────────────────────────

void UEEOSAuthSubsystem::HandleLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
	if (bWasSuccessful)
	{
		CurrentLoginStatus = EEOSLoginStatus::LoggedIn;
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem: Login successful — UserId: %s"), *UserId.ToString());

		// Note: OnAuthTokenRefreshed deliberately does NOT fire here — it fires only from
		// RefreshAuthToken(). An ordinary login result is reported via OnLoginComplete.

		// Auto-chain Connect login if configured
		AutoConnectLoginAfterAuth();
	}
	else
	{
		CurrentLoginStatus = EEOSLoginStatus::Failed;
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem: Login failed — %s"), *Error);
	}

	// Clear the engine delegate registration AND the in-progress guard BEFORE
	// broadcasting: a listener retrying/chaining Login() from inside the completion
	// broadcast must not be rejected by the still-armed guard (R1). The Reset() lives
	// OUTSIDE the OSS null-check — a null OSS at completion time must not leave the
	// guard armed forever (m6).
	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		if (IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface())
		{
			IdentityInterface->ClearOnLoginCompleteDelegate_Handle(0, LoginDelegateHandle);
		}
	}
	LoginDelegateHandle.Reset();

	OnLoginStatusChanged.Broadcast(CurrentLoginStatus);
	OnLoginComplete.Broadcast(bWasSuccessful, Error);
}

void UEEOSAuthSubsystem::HandleLogoutComplete(int32 LocalUserNum, bool bWasSuccessful)
{
	// Consume the DeletePersistentAuth marker up front (re-entrancy safe: a listener
	// starting a new operation from inside a broadcast must see clean state).
	const bool bNotifyPersistentAuthDeleted = bPendingPersistentAuthDeleteViaLogout;
	bPendingPersistentAuthDeleteViaLogout = false;

	if (bWasSuccessful)
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
	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		if (IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface())
		{
			IdentityInterface->ClearOnLogoutCompleteDelegate_Handle(0, LogoutDelegateHandle);
		}
	}
	LogoutDelegateHandle.Reset();

	OnLoginStatusChanged.Broadcast(CurrentLoginStatus);
	OnLogoutComplete.Broadcast();

	// The engine's logout path deletes the persistent credentials
	// (FUserManagerEOS::CallEOSAuthLogout → EOS_Auth_DeletePersistentAuth), so a
	// successful DeletePersistentAuth-initiated logout means the deletion happened.
	if (bNotifyPersistentAuthDeleted)
	{
		OnPersistentAuthDeleted.Broadcast(bWasSuccessful);
	}
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

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem: Auto-chaining Connect login after Auth success..."));

	// After Epic Auth login, use the Epic ID Token to do Connect login
	// This gets us a ProductUserId from the already-authenticated Epic account
	ConnectLogin(EEOSConnectLoginType::Epic, GetAuthToken());
}
