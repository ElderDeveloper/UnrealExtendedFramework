// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSAuthSubsystem.h"
#include "Shared/EEOSSettings.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "UnrealExtendedEOS.h"

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

	if (const UEEOSSettings* Settings = GetEOSSettings())
	{
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
}

void UEEOSAuthSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

// ── Epic Auth Login ──────────────────────────────────────────────────────────

void UEEOSAuthSubsystem::Login(EEOSLoginType LoginType, const FString& Id, const FString& Token)
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("Login"));
		OnLoginComplete.Broadcast(false, TEXT("EOS is not available"));
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAuthSubsystem::Login — Identity interface is not available"));
		OnLoginComplete.Broadcast(false, TEXT("Identity interface not available"));
		return;
	}

	CurrentLoginStatus = EEOSLoginStatus::LoggingIn;
	UsedLoginType = LoginType;
	OnLoginStatusChanged.Broadcast(CurrentLoginStatus);

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
		Credentials.Type = TEXT("devicecode");
		break;
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
		Credentials.Type = TEXT("externalauth");
		Credentials.Token = Token;
		break;
	}

	LoginDelegateHandle = IdentityInterface->AddOnLoginCompleteDelegate_Handle(0, FOnLoginCompleteDelegate::CreateUObject(this, &UEEOSAuthSubsystem::HandleLoginComplete));
	IdentityInterface->Login(0, Credentials);

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem::Login — Attempting login with type: %s"), *Credentials.Type);
}

void UEEOSAuthSubsystem::LoginWithDefaults()
{
	const UEEOSSettings* Settings = GetEOSSettings();
	if (Settings)
	{
		Login(Settings->DefaultLoginType);
	}
	else
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAuthSubsystem::LoginWithDefaults — Settings not available"));
		OnLoginComplete.Broadcast(false, TEXT("EOS Settings not available"));
	}
}

void UEEOSAuthSubsystem::Logout()
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("Logout"));
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid())
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("EEOSAuthSubsystem::Logout — Identity interface is not available"));
		return;
	}

	LogoutDelegateHandle = IdentityInterface->AddOnLogoutCompleteDelegate_Handle(0, FOnLogoutCompleteDelegate::CreateUObject(this, &UEEOSAuthSubsystem::HandleLogoutComplete));
	IdentityInterface->Logout(0);

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem::Logout — Logging out..."));
}

void UEEOSAuthSubsystem::RefreshAuthToken()
{
	if (!IsLoggedIn())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem::RefreshAuthToken — Not logged in"));
		OnAuthTokenRefreshed.Broadcast(false, TEXT(""));
		return;
	}

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem::RefreshAuthToken — Attempting silent re-auth via PersistentAuth"));
	Login(EEOSLoginType::PersistentAuth);
}

void UEEOSAuthSubsystem::DeletePersistentAuth()
{
	if (!IsEOSAvailable())
	{
		LogEOSUnavailable(TEXT("DeletePersistentAuth"));
		return;
	}

	IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem();
	IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface();
	if (!IdentityInterface.IsValid()) return;

	// EOS persistent auth is managed through the identity interface.
	// Perform a logout to clear stored credentials and revoke the auth token.
	FUniqueNetIdPtr UserId = IdentityInterface->GetUniquePlayerId(0);
	if (UserId.IsValid())
	{
		// Register a one-shot logout completion delegate
		IdentityInterface->AddOnLogoutCompleteDelegate_Handle(0,
			FOnLogoutCompleteDelegate::CreateLambda(
				[this, IdentityInterface](int32 LocalUserNum, bool bWasSuccessful)
				{
					UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem::DeletePersistentAuth — Logout %s, persistent credentials cleared"),
						bWasSuccessful ? TEXT("succeeded") : TEXT("failed"));
					IdentityInterface->ClearOnLogoutCompleteDelegates(0, this);
				}));

		IdentityInterface->Logout(0);
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem::DeletePersistentAuth — Logging out to clear persistent auth credentials"));
	}
	else
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem::DeletePersistentAuth — No logged-in user, nothing to clear"));
	}
}

// ── EOS Connect Login (Game Services) ────────────────────────────────────────

void UEEOSAuthSubsystem::ConnectLogin(EEOSConnectLoginType LoginType, const FString& Token, const FString& DisplayName)
{
#if WITH_EOS_SDK
	PerformConnectLogin(LoginType, Token, DisplayName);
#else
	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem::ConnectLogin — EOS SDK not available"));
	OnConnectLoginComplete.Broadcast(false, TEXT(""), TEXT("EOS SDK not available"));
#endif
}

void UEEOSAuthSubsystem::ConnectLoginWithDeviceId(const FString& DisplayName)
{
#if WITH_EOS_SDK
	IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
	if (!SDKManager)
	{
		OnConnectLoginComplete.Broadcast(false, TEXT(""), TEXT("SDK Manager not available"));
		return;
	}

	TArray<IEOSPlatformHandlePtr> Platforms = SDKManager->GetActivePlatforms();
	if (Platforms.Num() == 0)
	{
		OnConnectLoginComplete.Broadcast(false, TEXT(""), TEXT("No active EOS platform"));
		return;
	}

	EOS_HPlatform PlatformHandle = *Platforms[0];
	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(PlatformHandle);
	if (!ConnectHandle)
	{
		OnConnectLoginComplete.Broadcast(false, TEXT(""), TEXT("Connect interface not available"));
		return;
	}

	// First, create the device ID (idempotent — succeeds or returns DuplicateNotAllowed)
	EOS_Connect_CreateDeviceIdOptions CreateOpts = {};
	CreateOpts.ApiVersion = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
	
	FString DeviceModel = FString::Printf(TEXT("PC Windows - %s"), FPlatformProcess::ComputerName());
	FTCHARToUTF8 DeviceModelUtf8(*DeviceModel);
	CreateOpts.DeviceModel = DeviceModelUtf8.Get();

	struct FDeviceIdContext
	{
		UEEOSAuthSubsystem* Self;
		FString DisplayName;
	};
	FDeviceIdContext* Ctx = new FDeviceIdContext{this, DisplayName};

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem: Creating DeviceId..."));


	// Use a static EOS_CALL wrapper function for the callback
	struct FDeviceIdCallbackWrapper
	{
		static void EOS_CALL Callback(const EOS_Connect_CreateDeviceIdCallbackInfo* Data)
		{
			FDeviceIdContext* C = static_cast<FDeviceIdContext*>(Data->ClientData);
			if (!C) return;

			if (Data->ResultCode == EOS_EResult::EOS_Success ||
				Data->ResultCode == EOS_EResult::EOS_DuplicateNotAllowed)
			{
				UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem: DeviceId ready, performing Connect login..."));
				
				AsyncTask(ENamedThreads::GameThread, [C]()
				{
					C->Self->PerformConnectLogin(EEOSConnectLoginType::DeviceId, TEXT(""), C->DisplayName);
					delete C;
				});
			}
			else
			{
				FString ErrorMsg = FString::Printf(TEXT("CreateDeviceId failed: %hs"), EOS_EResult_ToString(Data->ResultCode));
				UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem: %s"), *ErrorMsg);

				AsyncTask(ENamedThreads::GameThread, [C, ErrorMsg]()
				{
					C->Self->SetConnectLoginResult(false, TEXT(""), ErrorMsg);
					delete C;
				});
			}
		}
	};

	EOS_Connect_CreateDeviceId(ConnectHandle, &CreateOpts, Ctx, &FDeviceIdCallbackWrapper::Callback);
#else
	OnConnectLoginComplete.Broadcast(false, TEXT(""), TEXT("EOS SDK not available"));
#endif
}

void UEEOSAuthSubsystem::ConnectLoginWithDefaults()
{
	const UEEOSSettings* Settings = GetEOSSettings();
	if (!Settings)
	{
		OnConnectLoginComplete.Broadcast(false, TEXT(""), TEXT("Settings not available"));
		return;
	}

	if (Settings->DefaultConnectLoginType == EEOSConnectLoginType::DeviceId)
	{
		ConnectLoginWithDeviceId(TEXT("Player"));
	}
	else
	{
		// Other types need a token — for auto-login this would come from
		// the platform SDK (Steam, PSN, etc.)
		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem: ConnectLoginWithDefaults — Type requires platform token, call ConnectLogin() with token"));
		ConnectLogin(Settings->DefaultConnectLoginType);
	}
}

#if WITH_EOS_SDK

// ── Static EOS_CALL callbacks for Connect ────────────────────────────────────

struct FConnectLoginContext
{
	UEEOSAuthSubsystem* Self;
	bool bAutoCreateUser;
};

static void EOS_CALL OnConnectCreateUserComplete(const EOS_Connect_CreateUserCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;
	FConnectLoginContext* Ctx = static_cast<FConnectLoginContext*>(Data->ClientData);

	if (Data->ResultCode == EOS_EResult::EOS_Success)
	{
		char PuidBuf[EOS_PRODUCTUSERID_MAX_LENGTH + 1] = {};
		int32_t PuidLen = sizeof(PuidBuf);
		EOS_ProductUserId_ToString(Data->LocalUserId, PuidBuf, &PuidLen);
		FString ProductUserId(UTF8_TO_TCHAR(PuidBuf));

		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem: CreateUser succeeded — ProductUserId: %s"), *ProductUserId);

		AsyncTask(ENamedThreads::GameThread, [Ctx, ProductUserId]()
		{
			Ctx->Self->SetConnectLoginResult(true, ProductUserId, TEXT(""));
			delete Ctx;
		});
	}
	else
	{
		FString ErrorMsg = FString::Printf(TEXT("CreateUser failed: %hs"), EOS_EResult_ToString(Data->ResultCode));
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem: %s"), *ErrorMsg);

		AsyncTask(ENamedThreads::GameThread, [Ctx, ErrorMsg]()
		{
			Ctx->Self->SetConnectLoginResult(false, TEXT(""), ErrorMsg);
			delete Ctx;
		});
	}
}

static void EOS_CALL OnConnectLoginCallbackStatic(const EOS_Connect_LoginCallbackInfo* Data)
{
	if (!Data || !Data->ClientData) return;
	FConnectLoginContext* Ctx = static_cast<FConnectLoginContext*>(Data->ClientData);

	if (Data->ResultCode == EOS_EResult::EOS_Success)
	{
		// Login succeeded — extract ProductUserId
		char PuidBuf[EOS_PRODUCTUSERID_MAX_LENGTH + 1] = {};
		int32_t PuidLen = sizeof(PuidBuf);
		EOS_ProductUserId_ToString(Data->LocalUserId, PuidBuf, &PuidLen);
		FString ProductUserId(UTF8_TO_TCHAR(PuidBuf));

		UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem: Connect login succeeded — ProductUserId: %s"), *ProductUserId);

		AsyncTask(ENamedThreads::GameThread, [Ctx, ProductUserId]()
		{
			Ctx->Self->SetConnectLoginResult(true, ProductUserId, TEXT(""));
			delete Ctx;
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

					EOS_Connect_CreateUser(CH, &CreateOpts, Ctx, &OnConnectCreateUserComplete);
					return; // Don't delete Ctx — CreateUser callback will
				}
			}
		}

		// Fallback if we can't get the handle
		AsyncTask(ENamedThreads::GameThread, [Ctx]()
		{
			Ctx->Self->SetConnectLoginResult(false, TEXT(""), TEXT("Failed to create user — Connect interface unavailable"));
			delete Ctx;
		});
	}
	else
	{
		FString ErrorMsg = FString::Printf(TEXT("Connect login failed: %hs"), EOS_EResult_ToString(Data->ResultCode));
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem: %s"), *ErrorMsg);

		AsyncTask(ENamedThreads::GameThread, [Ctx, ErrorMsg]()
		{
			Ctx->Self->SetConnectLoginResult(false, TEXT(""), ErrorMsg);
			delete Ctx;
		});
	}
}

void UEEOSAuthSubsystem::PerformConnectLogin(EEOSConnectLoginType LoginType, const FString& Token, const FString& DisplayName)
{
	IEOSSDKManager* SDKManager = IEOSSDKManager::Get();
	if (!SDKManager)
	{
		OnConnectLoginComplete.Broadcast(false, TEXT(""), TEXT("SDK Manager not available"));
		return;
	}

	TArray<IEOSPlatformHandlePtr> Platforms = SDKManager->GetActivePlatforms();
	if (Platforms.Num() == 0)
	{
		OnConnectLoginComplete.Broadcast(false, TEXT(""), TEXT("No active EOS platform"));
		return;
	}

	EOS_HPlatform PlatformHandle = *Platforms[0];
	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(PlatformHandle);
	if (!ConnectHandle)
	{
		OnConnectLoginComplete.Broadcast(false, TEXT(""), TEXT("Connect interface not available"));
		return;
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

	EOS_Connect_Login(ConnectHandle, &LoginOptions, Ctx, &OnConnectLoginCallbackStatic);
}

#endif // WITH_EOS_SDK

// ── SetConnectLoginResult ────────────────────────────────────────────────────

void UEEOSAuthSubsystem::SetConnectLoginResult(bool bSuccess, const FString& ProductUserId, const FString& Error)
{
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

		// Broadcast token refresh if this was a re-auth
		FString Token = GetAuthToken();
		if (!Token.IsEmpty())
		{
			OnAuthTokenRefreshed.Broadcast(true, Token);
		}

		// Auto-chain Connect login if configured
		AutoConnectLoginAfterAuth();
	}
	else
	{
		CurrentLoginStatus = EEOSLoginStatus::Failed;
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSAuthSubsystem: Login failed — %s"), *Error);
	}

	OnLoginStatusChanged.Broadcast(CurrentLoginStatus);
	OnLoginComplete.Broadcast(bWasSuccessful, Error);

	// Clean up the delegate
	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		if (IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface())
		{
			IdentityInterface->ClearOnLoginCompleteDelegate_Handle(0, LoginDelegateHandle);
			LoginDelegateHandle.Reset();
		}
	}
}

void UEEOSAuthSubsystem::HandleLogoutComplete(int32 LocalUserNum, bool bWasSuccessful)
{
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

	OnLoginStatusChanged.Broadcast(CurrentLoginStatus);
	OnLogoutComplete.Broadcast();

	if (IOnlineSubsystem* EOSSub = GetEOSOnlineSubsystem())
	{
		if (IOnlineIdentityPtr IdentityInterface = EOSSub->GetIdentityInterface())
		{
			IdentityInterface->ClearOnLogoutCompleteDelegate_Handle(0, LogoutDelegateHandle);
			LogoutDelegateHandle.Reset();
		}
	}
}

void UEEOSAuthSubsystem::AutoConnectLoginAfterAuth()
{
	const UEEOSSettings* Settings = GetEOSSettings();
	if (!Settings || !Settings->bAutoConnectLoginOnStart) return;

	UE_LOG(LogExtendedEOS, Log, TEXT("EEOSAuthSubsystem: Auto-chaining Connect login after Auth success..."));

	// After Epic Auth login, use the Epic ID Token to do Connect login
	// This gets us a ProductUserId from the already-authenticated Epic account
	ConnectLogin(EEOSConnectLoginType::Epic, GetAuthToken());
}
