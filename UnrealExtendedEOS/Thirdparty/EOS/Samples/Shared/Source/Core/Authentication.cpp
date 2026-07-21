// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "DebugLog.h"
#include "StringUtils.h"
#include "AccountHelpers.h"
#include "CommandLine.h"
#include "Platform.h"
#include "Game.h"
#include "GameEvent.h"
#include "Users.h"
#include "Player.h"
#include "SampleConstants.h"
#include "Settings.h"
#include "Authentication.h"

/**
 * User context passed to EOS_Connect_Login, so we know what AccountId is logging in
 */
struct FConnectLoginContext
{
	EOS_EpicAccountId AccountId;
};

FAuthentication::FAuthentication()
{
	
}

FAuthentication::~FAuthentication()
{
	
}

bool FAuthentication::Login(ELoginMode LoginMode, std::wstring FirstStr, std::wstring SecondStr, ELoginExternalType ExternalType)
{
	if (!FPlatform::IsInitialized())
	{
		FDebugLog::LogError(L"[EOS SDK] Can't Log In - EOS SDK Not Initialized!");
		return false;
	}

	AuthHandle = EOS_Platform_GetAuthInterface(FPlatform::GetPlatformHandle());
	assert(AuthHandle != nullptr);

	ConnectHandle = EOS_Platform_GetConnectInterface(FPlatform::GetPlatformHandle());
	assert(ConnectHandle != nullptr);

	AddNotifyLoginStatusChanged();

	EOS_Auth_Credentials Credentials = {};
	Credentials.ApiVersion = EOS_AUTH_CREDENTIALS_API_LATEST;

	EOS_Auth_LoginOptions LoginOptions = {};
	memset(&LoginOptions, 0, sizeof(LoginOptions));
	LoginOptions.ApiVersion = EOS_AUTH_LOGIN_API_LATEST;

	if (FCommandLine::Get().HasParam(CommandLineConstants::ScopesNoFlags))
	{
		LoginOptions.ScopeFlags |= EOS_EAuthScopeFlags::EOS_AS_NoFlags;
	}
	else if (FCommandLine::Get().HasParam(CommandLineConstants::ScopesBasicProfile) ||
			 FCommandLine::Get().HasParam(CommandLineConstants::ScopesFriendsList) ||
			 FCommandLine::Get().HasParam(CommandLineConstants::ScopesPresence) ||
			 FCommandLine::Get().HasParam(CommandLineConstants::ScopesFriendsManagement) ||
			 FCommandLine::Get().HasParam(CommandLineConstants::ScopesEmail) ||
			 FCommandLine::Get().HasParam(CommandLineConstants::ScopesCountry))
	{
		if (FCommandLine::Get().HasParam(CommandLineConstants::ScopesBasicProfile))
		{
			LoginOptions.ScopeFlags |= EOS_EAuthScopeFlags::EOS_AS_BasicProfile;
		}
		if (FCommandLine::Get().HasParam(CommandLineConstants::ScopesFriendsList))
		{
			LoginOptions.ScopeFlags |= EOS_EAuthScopeFlags::EOS_AS_FriendsList;
		}
		if (FCommandLine::Get().HasParam(CommandLineConstants::ScopesPresence))
		{
			LoginOptions.ScopeFlags |= EOS_EAuthScopeFlags::EOS_AS_Presence;
		}
		if (FCommandLine::Get().HasParam(CommandLineConstants::ScopesFriendsManagement))
		{
			LoginOptions.ScopeFlags |= EOS_EAuthScopeFlags::EOS_AS_FriendsManagement;
		}
		if (FCommandLine::Get().HasParam(CommandLineConstants::ScopesEmail))
		{
			LoginOptions.ScopeFlags |= EOS_EAuthScopeFlags::EOS_AS_Email;
		}
		if (FCommandLine::Get().HasParam(CommandLineConstants::ScopesCountry))
		{
			LoginOptions.ScopeFlags |= EOS_EAuthScopeFlags::EOS_AS_Country;
		}
	}
	else
	{
		LoginOptions.ScopeFlags = DefaultLoginScope;
	}

	// We need to use a big enough size to support a steam auth session ticket (see where GetAuthSessionTicket is called).
	// The "*2 +1" is because this is the hexadecimal string we will be passing to the SDK.
	char FirstParamStr[4096*2+1];
	if (FirstStr.size() > sizeof(FirstParamStr) - 1)
	{
		FDebugLog::LogError(L"[EOS SDK] Can't Log in - entered string is too long!");
		return false;
	}
	sprintf_s(FirstParamStr, sizeof(FirstParamStr), "%s", FStringUtils::Narrow(FirstStr).c_str());

	char SecondParamStr[256];
	if (SecondStr.size() > sizeof(SecondParamStr) - 1)
	{
		FDebugLog::LogError(L"[EOS SDK] Can't Log in - entered string is too long!");
		return false;
	}
	sprintf_s(SecondParamStr, sizeof(SecondParamStr), "%s", FStringUtils::Narrow(SecondStr).c_str());

	switch (LoginMode)
	{
		case ELoginMode::IDPassword:
		{
			FDebugLog::Log(L"[EOS SDK] Logging In as User Id: %ls", FirstStr.c_str());
			Credentials.Id = FirstParamStr;
			Credentials.Token = SecondParamStr;
			Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_Password;
			break;
		}
		case ELoginMode::ExchangeCode:
		{
			FDebugLog::Log(L"[EOS SDK] Logging In with Exchange Code");
			Credentials.Token = FirstParamStr;
			Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_ExchangeCode;
			break;
		}
		case ELoginMode::DevAuth:
		{
			FDebugLog::Log(L"[EOS SDK] Logging In with Host: %ls", FirstStr.c_str());
			Credentials.Id = FirstParamStr;
			Credentials.Token = SecondParamStr;
			Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_Developer;

			// Save to settings and write out to file
			FSettings::Get().Set(SettingsConstants::DevAuthHost, FirstStr);
			FSettings::Get().Set(SettingsConstants::DevAuthCred, SecondStr);
			break;
		}
		case ELoginMode::AccountPortal:
		{
			FDebugLog::Log(L"[EOS SDK] Logging In with Account Portal");
			Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_AccountPortal;
			break;
		}
		case ELoginMode::PersistentAuth:
		{
			FDebugLog::Log(L"[EOS SDK] Logging In with Persistent Auth");
			Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_PersistentAuth;
			break;
		}
		case ELoginMode::ExternalAuth:
		{
			FDebugLog::Log(L"[EOS SDK] Logging In with External Auth");
			Credentials.Type = EOS_ELoginCredentialType::EOS_LCT_ExternalAuth;
			Credentials.Token = FirstParamStr;

			switch (ExternalType)
			{
				case ELoginExternalType::Steam:
				{
					FDebugLog::Log(L"[EOS SDK] Logging In with Steam");
					Credentials.ExternalType = EOS_EExternalCredentialType::EOS_ECT_STEAM_SESSION_TICKET;
					break;
				}
				default:
					break;
			}
			break;
		}
	}

	LoginOptions.Credentials = &Credentials;

	CurrentLoginMode = LoginMode;

	EOS_Auth_Login(AuthHandle, &LoginOptions, this, LoginCompleteCallbackFn);

	AddConnectAuthExpirationNotification();

	return true;
}

void FAuthentication::Logout(FEpicAccountId UserId)
{
	FDebugLog::Log(L"[EOS SDK] Logging Out");

	EOS_Auth_LogoutOptions LogoutOptions = {};
	LogoutOptions.ApiVersion = EOS_AUTH_LOGOUT_API_LATEST;
	LogoutOptions.LocalUserId = UserId.AccountId;

	assert(AuthHandle != nullptr);
	EOS_Auth_Logout(AuthHandle, &LogoutOptions, NULL, LogoutCompleteCallbackFn);
}

void FAuthentication::AddNotifyLoginStatusChanged()
{
	if (LoginStatusChangedId == EOS_INVALID_NOTIFICATIONID)
	{
		EOS_Auth_AddNotifyLoginStatusChangedOptions LoginStatusChangedOptions{};
		LoginStatusChangedOptions.ApiVersion = EOS_AUTH_ADDNOTIFYLOGINSTATUSCHANGED_API_LATEST;

		LoginStatusChangedId = EOS_Auth_AddNotifyLoginStatusChanged(AuthHandle, &LoginStatusChangedOptions, NULL, LoginStatusChangedCb);
	}
}

void FAuthentication::RemoveNotifyLoginStatusChanged()
{
	if (LoginStatusChangedId != EOS_INVALID_NOTIFICATIONID)
	{
		EOS_Auth_RemoveNotifyLoginStatusChanged(AuthHandle, LoginStatusChangedId);
	}
}

void FAuthentication::AddConnectAuthExpirationNotification()
{
	if (ConnectAuthExpirationId == EOS_INVALID_NOTIFICATIONID)
	{
		EOS_Connect_AddNotifyAuthExpirationOptions Options{};
		Options.ApiVersion = EOS_CONNECT_ADDNOTIFYAUTHEXPIRATION_API_LATEST;

		ConnectAuthExpirationId = EOS_Connect_AddNotifyAuthExpiration(ConnectHandle, &Options, NULL, ConnectAuthExpirationCb);
	}
}

void FAuthentication::RemoveConnectAuthExpirationNotification()
{
	if (ConnectAuthExpirationId != EOS_INVALID_NOTIFICATIONID)
	{
		EOS_Connect_RemoveNotifyAuthExpiration(ConnectHandle, ConnectAuthExpirationId);

		ConnectAuthExpirationId = EOS_INVALID_NOTIFICATIONID;
	}
}

void FAuthentication::Shutdown()
{
	FDebugLog::Log(L"[EOS SDK] Shutting Down ...");

	RemoveNotifyLoginStatusChanged();

	RemoveConnectAuthExpirationNotification();

	if (FPlatform::GetPlatformHandle())
	{
		EOS_Platform_Release(FPlatform::GetPlatformHandle());
	}

	EOS_EResult ShutdownResult = EOS_Shutdown();
	if (ShutdownResult != EOS_EResult::EOS_Success)
	{
		FDebugLog::LogError(L"[EOS SDK] Failed to shutdown Error: %ls", FStringUtils::Widen(EOS_EResult_ToString(ShutdownResult)).c_str());
	}

	FDebugLog::Log(L"[EOS SDK] Shutdown Complete");

	FPlatform::Release();
}

void FAuthentication::CheckAutoLogin()
{
	// SteamManager starts login automatically
#ifndef EOS_STEAM_ENABLED
	// Check for Launcher commandline params to log in via Exchange Code
	if (FCommandLine::Get().HasParam(CommandLineConstants::EpicPortal) &&
		FCommandLine::Get().HasParam(CommandLineConstants::LauncherAuthType) &&
		FCommandLine::Get().HasParam(CommandLineConstants::LauncherAuthPassword))
	{
		if (FCommandLine::Get().GetParamValue(CommandLineConstants::LauncherAuthType) == L"exchangecode")
		{
			std::wstring LauncherExchangeCode = FCommandLine::Get().GetParamValue(CommandLineConstants::LauncherAuthPassword);
			if (!LauncherExchangeCode.empty())
			{
				// Auto login with Exchange Code
				FGameEvent Event(EGameEventType::StartUserLogin, (int)ELoginMode::ExchangeCode, LauncherExchangeCode);
				FGame::Get().OnGameEvent(Event);
				return;
			}
		}
	}

	bool bAutoLoginEnabled = false;
	if (FCommandLine::Get().HasParam(CommandLineConstants::AutoLogin))
	{
		bAutoLoginEnabled = true;
	}
	else
	{
		FSettings::Get().TryGetAsBool(SettingsConstants::AutoLogin, bAutoLoginEnabled);
	}

	if (bAutoLoginEnabled)
	{
#ifdef DEV_BUILD
		std::wstring UserId = FCommandLine::Get().GetParamValue(CommandLineConstants::DevUsername);
		std::wstring UserPwd = FCommandLine::Get().GetParamValue(CommandLineConstants::DevPassword);

		if (!UserId.empty() && !UserPwd.empty())
		{
			// Auto login with Username and Password
			FGameEvent Event(EGameEventType::StartUserLogin, (int)ELoginMode::IDPassword, UserId, UserPwd);
			FGame::Get().OnGameEvent(Event);
			return;
		}
#endif
		std::wstring DevHost = FCommandLine::Get().GetParamValue(CommandLineConstants::DevAuthHost);
		std::wstring DevCred = FCommandLine::Get().GetParamValue(CommandLineConstants::DevAuthCred);
		if (!DevHost.empty() && !DevCred.empty())
		{
			// Auto login with Developer Authentication Tool
			FGameEvent Event(EGameEventType::StartUserLogin, (int)ELoginMode::DevAuth, DevHost, DevCred);
			FGame::Get().OnGameEvent(Event);
			return;
		}

		// Auto login with persistent auth
		FGameEvent Event(EGameEventType::StartUserLogin, (int)ELoginMode::PersistentAuth);
		FGame::Get().OnGameEvent(Event);
	}
#endif
}

void FAuthentication::ConnectLogin(FEpicAccountId UserId)
{
	FDebugLog::Log(L"[EOS SDK] Connect Login - User ID: %ls", FEpicAccountId(UserId).ToString().c_str());

	EOS_Auth_Token* UserAuthToken = nullptr;

	assert(AuthHandle != nullptr);

	EOS_Auth_CopyUserAuthTokenOptions CopyTokenOptions = { 0 };
	CopyTokenOptions.ApiVersion = EOS_AUTH_COPYUSERAUTHTOKEN_API_LATEST;

	if (EOS_Auth_CopyUserAuthToken(AuthHandle, &CopyTokenOptions, UserId, &UserAuthToken) == EOS_EResult::EOS_Success)
	{
		EOS_Connect_Credentials Credentials = {};
		Credentials.ApiVersion = EOS_CONNECT_CREDENTIALS_API_LATEST;
		Credentials.Token = UserAuthToken->AccessToken;
		Credentials.Type = EOS_EExternalCredentialType::EOS_ECT_EPIC;

		EOS_Connect_LoginOptions Options = { 0 };
		Options.ApiVersion = EOS_CONNECT_LOGIN_API_LATEST;
		Options.Credentials = &Credentials;
		Options.UserLoginInfo = nullptr;

		// Setup a context so the callback knows what AccountId is logging in.
		std::unique_ptr<FConnectLoginContext> ClientData(new FConnectLoginContext);
		ClientData->AccountId = UserId.AccountId;

		assert(ConnectHandle != nullptr);
		EOS_Connect_Login(ConnectHandle, &Options, ClientData.release(), ConnectLoginCompleteCb);
		EOS_Auth_Token_Release(UserAuthToken);
	}
}

void EOS_CALL FAuthentication::LoginCompleteCallbackFn(const EOS_Auth_LoginCallbackInfo* Data)
{
	assert(Data != NULL);

	FDebugLog::Log(L"[EOS SDK] Login Complete - User ID: %ls", FEpicAccountId(Data->LocalUserId).ToString().c_str());

	FAuthentication* ThisAuth = NULL;
	if (Data->ClientData)
	{
		ThisAuth = (FAuthentication*)Data->ClientData;
		if (ThisAuth)
		{
			FDebugLog::Log(L" - Login Mode: %d", ThisAuth->CurrentLoginMode);
		}
	}

	EOS_HAuth AuthHandle = EOS_Platform_GetAuthInterface(FPlatform::GetPlatformHandle());
	assert(AuthHandle != nullptr);

	if (Data->ResultCode == EOS_EResult::EOS_Success)
	{
		const int32_t AccountsCount = EOS_Auth_GetLoggedInAccountsCount(AuthHandle);
		for (int32_t AccountIdx = 0; AccountIdx < AccountsCount; ++AccountIdx)
		{
			FEpicAccountId AccountId;
			AccountId = EOS_Auth_GetLoggedInAccountByIndex(AuthHandle, AccountIdx);

			EOS_ELoginStatus LoginStatus;
			LoginStatus = EOS_Auth_GetLoginStatus(AuthHandle, Data->LocalUserId);

			FDebugLog::Log(L"[EOS SDK] [%d] - Account ID: %ls, Status: %d", AccountIdx, FEpicAccountId(AccountId).ToString().c_str(), (int32_t)LoginStatus);
		}

		FGameEvent Event(EGameEventType::UserLoggedIn, Data->LocalUserId);
		FGame::Get().OnGameEvent(Event);
	}
	else if (Data->ResultCode == EOS_EResult::EOS_Auth_MFARequired)
	{
		FDebugLog::LogWarning(L"[EOS SDK] MFA Code needs to be entered before logging in");

		FGameEvent Event(EGameEventType::UserLoginRequiresMFA, Data->LocalUserId);
		FGame::Get().OnGameEvent(Event);
	}
	else if (Data->ResultCode == EOS_EResult::EOS_InvalidUser)
	{
		if (Data->ContinuanceToken != NULL)
		{
			FDebugLog::Log(L"[EOS SDK] Login failed, external account not found");

			// Store continuance token so we can continue login
			FGameEvent Event(EGameEventType::ContinuanceToken, Data->ContinuanceToken);
			FGame::Get().OnGameEvent(Event);

			// Continue login
			FGameEvent ContinueEvent(EGameEventType::ContinueLogin);
			FGame::Get().OnGameEvent(ContinueEvent);
		}
		else
		{
			FDebugLog::LogError(L"[EOS SDK] Continuation Token is Invalid!");
		}
	}
	else if (EOS_EResult_IsOperationComplete(Data->ResultCode))
	{
		FDebugLog::LogError(L"[EOS SDK] Login Failed - Error Code: %ls", FStringUtils::Widen(EOS_EResult_ToString(Data->ResultCode)).c_str());

		FGameEvent Event(EGameEventType::UserLoginFailed, Data->LocalUserId);
		FGame::Get().OnGameEvent(Event);

		if (ThisAuth)
		{
			if (ThisAuth->CurrentLoginMode == ELoginMode::PersistentAuth)
			{
				// Delete saved persistent auth token if token has expired or auth is invalid
				// Don't delete for other errors (e.g. EOS_EResult::EOS_NoConnection), the auth token may still be valid in these cases
				if (Data->ResultCode == EOS_EResult::EOS_Auth_Expired ||
					Data->ResultCode == EOS_EResult::EOS_InvalidAuth)
				{
					ThisAuth->DeletePersistentAuth();
				}
			}
		}
	}
}

void EOS_CALL FAuthentication::LinkAccountCompleteCallbackFn(const EOS_Auth_LinkAccountCallbackInfo* Data)
{
	assert(Data != NULL);

	FDebugLog::Log(L"[EOS SDK] Link Account Complete - User ID: %ls", FEpicAccountId(Data->LocalUserId).ToString().c_str());

	EOS_HAuth AuthHandle = EOS_Platform_GetAuthInterface(FPlatform::GetPlatformHandle());
	assert(AuthHandle != nullptr);

	if (Data->ResultCode == EOS_EResult::EOS_Success)
	{
		const int32_t AccountsCount = EOS_Auth_GetLoggedInAccountsCount(AuthHandle);
		for (int32_t AccountIdx = 0; AccountIdx < AccountsCount; ++AccountIdx)
		{
			FEpicAccountId AccountId;
			AccountId = EOS_Auth_GetLoggedInAccountByIndex(AuthHandle, AccountIdx);

			EOS_ELoginStatus LoginStatus;
			LoginStatus = EOS_Auth_GetLoginStatus(AuthHandle, Data->LocalUserId);

			FDebugLog::Log(L"[EOS SDK] [%d] - Account ID: %ls, Status: %d", AccountIdx, FEpicAccountId(AccountId).ToString().c_str(), (int32_t)LoginStatus);
		}

		FGameEvent Event(EGameEventType::UserLoggedIn, Data->LocalUserId);
		FGame::Get().OnGameEvent(Event);
	}
	else if (EOS_EResult_IsOperationComplete(Data->ResultCode))
	{
		FDebugLog::LogError(L"[EOS SDK] Link Account Failed - Error Code: %ls", FStringUtils::Widen(EOS_EResult_ToString(Data->ResultCode)).c_str());

		FGameEvent Event(EGameEventType::UserLoginFailed, Data->LocalUserId);
		FGame::Get().OnGameEvent(Event);
	}
}

void FAuthentication::ContinueLogin(EOS_ContinuanceToken ContinuanceToken)
{
	if (ContinuanceToken == NULL)
	{
		FDebugLog::LogError(L"[EOS SDK] ContinueLogin - ContinuanceToken is Invalid!");
	}

	EOS_HAuth AuthHandle = EOS_Platform_GetAuthInterface(FPlatform::GetPlatformHandle());
	assert(AuthHandle != nullptr);

	EOS_Auth_LinkAccountOptions Options{};
	Options.ApiVersion = EOS_AUTH_LINKACCOUNT_API_LATEST;
	Options.LinkAccountFlags = EOS_ELinkAccountFlags::EOS_LA_NoFlags;
	Options.ContinuanceToken = ContinuanceToken;
	Options.LocalUserId = nullptr;

	EOS_Auth_LinkAccount(AuthHandle, &Options, NULL, LinkAccountCompleteCallbackFn);
}

std::string FAuthentication::GetConnectIdToken(EOS_ProductUserId LocalUserId)
{
	if (!FPlatform::IsInitialized())
	{
		FDebugLog::LogError(L"[EOS SDK] Can't get Connect ID Token - EOS SDK Not Initialized!");
		return std::string();
	}

	EOS_Connect_CopyIdTokenOptions Options = {};
	Options.ApiVersion = EOS_CONNECT_COPYIDTOKEN_API_LATEST;
	Options.LocalUserId = LocalUserId;

	EOS_Connect_IdToken* IdToken = nullptr;
	assert(ConnectHandle != nullptr);
	EOS_EResult Result = EOS_Connect_CopyIdToken(ConnectHandle, &Options, &IdToken);
	if (Result != EOS_EResult::EOS_Success)
	{
		FDebugLog::LogError(L"[EOS SDK] Failed to copy Connect ID token Error: %ls", FStringUtils::Widen(EOS_EResult_ToString(Result)).c_str());
		return std::string();
	}

	// Copy the JWT member out to an std::string to be returned.
	std::string IdTokenString = IdToken->JsonWebToken;

	// Free the token memory allocation we received from the SDK.
	EOS_Connect_IdToken_Release(IdToken);

	return IdTokenString;
}

void FAuthentication::VerifyConnectIdToken(EOS_ProductUserId ExpectedUserId, const char* ConnectIdTokenJWT)
{
	EOS_Connect_IdToken Token = {};
	Token.ApiVersion = EOS_CONNECT_IDTOKEN_API_LATEST;
	Token.ProductUserId = ExpectedUserId;
	Token.JsonWebToken = ConnectIdTokenJWT;

	EOS_Connect_VerifyIdTokenOptions Options = {};
	Options.ApiVersion = EOS_CONNECT_VERIFYIDTOKEN_API_LATEST;
	Options.IdToken = &Token;

	EOS_Connect_VerifyIdToken(ConnectHandle, &Options, NULL, VerifyConnectIdTokenCompleteCallbackFn);
}

void EOS_CALL FAuthentication::VerifyConnectIdTokenCompleteCallbackFn(const EOS_Connect_VerifyIdTokenCallbackInfo* Data)
{
	if (Data->ResultCode == EOS_EResult::EOS_Success)
	{
		FGameEvent Event(EGameEventType::UserConnectIdTokenVerified, FProductUserId(Data->ProductUserId));
		FGame::Get().OnGameEvent(Event);
	}
	else
	{
		FGameEvent Event(EGameEventType::UserConnectIdTokenVerificationFailed, FProductUserId(Data->ProductUserId));
		FGame::Get().OnGameEvent(Event);
	}
}

void FAuthentication::DeletePersistentAuth()
{
	if (!FPlatform::IsInitialized())
	{
		FDebugLog::LogError(L"[EOS SDK] Can't Delete Persistent Auth - EOS SDK Not Initialized!");
		return;
	}

	EOS_Auth_DeletePersistentAuthOptions Options = {};
	Options.ApiVersion = EOS_AUTH_DELETEPERSISTENTAUTH_API_LATEST;
	assert(AuthHandle != nullptr);
	EOS_Auth_DeletePersistentAuth(AuthHandle, &Options, NULL, DeletePersistentAuthCompleteCallbackFn);
}

void EOS_CALL FAuthentication::DeletePersistentAuthCompleteCallbackFn(const EOS_Auth_DeletePersistentAuthCallbackInfo* Data)
{
	assert(Data != NULL);
	if (Data->ResultCode == EOS_EResult::EOS_Success)
	{
		FDebugLog::Log(L"[EOS SDK] DeletePersistentAuth Success.");
	}
	else if (Data->ResultCode == EOS_EResult::EOS_NotFound)
	{
		FDebugLog::Log(L"[EOS SDK] DeletePersistentAuth Success - No existing credentials were present for deletion.");
	}
	else
	{
		FDebugLog::Log(L"[EOS SDK] DeletePersistentAuth Failed - Result: %ls", FStringUtils::Widen(EOS_EResult_ToString(Data->ResultCode)).c_str());
	}
}

void EOS_CALL FAuthentication::LogoutCompleteCallbackFn(const EOS_Auth_LogoutCallbackInfo* Data)
{
	assert(Data != NULL);

	if (Data->ResultCode == EOS_EResult::EOS_Success)
	{
		FDebugLog::Log(L"[EOS SDK] Logout Complete - User: %ls", FEpicAccountId(Data->LocalUserId).ToString().c_str());

		// UserLoggedOut event is triggered in LoginStatusChangedCb
	}
	else
	{
		FDebugLog::Log(L"[EOS SDK] Logout Failed - User: %ls, Result: %ls", FEpicAccountId(Data->LocalUserId).ToString().c_str(), FStringUtils::Widen(EOS_EResult_ToString(Data->ResultCode)).c_str());
	}
}

void EOS_CALL FAuthentication::ConnectLoginCompleteCb(const EOS_Connect_LoginCallbackInfo* Data)
{
	assert(Data != NULL);

	// Automated delete to avoid leaks independently of the code path
	std::unique_ptr<FConnectLoginContext> ClientData(static_cast<FConnectLoginContext*>(Data->ClientData));
	
	if (Data->ResultCode == EOS_EResult::EOS_Success)
	{
		FDebugLog::Log(L"[EOS SDK] Connect Login Complete - ProductUserId: %ls", FProductUserId(Data->LocalUserId).ToString().c_str());

		OnConnectLoginComplete(Data->ResultCode, ClientData->AccountId, Data->LocalUserId);
	}
	else if (Data->ResultCode == EOS_EResult::EOS_InvalidUser)
	{
		EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(FPlatform::GetPlatformHandle());
		assert(ConnectHandle != NULL);

		EOS_Connect_CreateUserOptions Options = {};
		Options.ApiVersion = EOS_CONNECT_CREATEUSER_API_LATEST;

		if (Data->ContinuanceToken != NULL)
		{
			Options.ContinuanceToken = Data->ContinuanceToken;
		}

		// NOTE: We're not deleting the received context because we're passing it down to another SDK call
		EOS_Connect_CreateUser(ConnectHandle, &Options, ClientData.release(), ConnectCreateUserCompleteCb);
	}
	else
	{
		FDebugLog::Log(L"[EOS SDK] Connect Login Complete failed with %ls", FStringUtils::Widen(EOS_EResult_ToString(Data->ResultCode)).c_str());
	}
}

void EOS_CALL FAuthentication::ConnectCreateUserCompleteCb(const EOS_Connect_CreateUserCallbackInfo* Data)
{
	assert(Data != NULL);

	std::unique_ptr<FConnectLoginContext> ClientData(static_cast<FConnectLoginContext*>(Data->ClientData));
	FDebugLog::Log(L"[EOS SDK] Connect Create User Complete - ProductUserId: %ls, Result: %ls", FProductUserId(Data->LocalUserId).ToString().c_str(), FStringUtils::Widen(EOS_EResult_ToString(Data->ResultCode)).c_str());

	OnConnectLoginComplete(Data->ResultCode, ClientData->AccountId, Data->LocalUserId);
}

void EOS_CALL FAuthentication::ConnectAuthExpirationCb(const EOS_Connect_AuthExpirationCallbackInfo* Data)
{
	assert(Data != NULL);

	FDebugLog::Log(L"[EOS SDK] Connect Auth Expiring - ProductUserId: %ls", FProductUserId(Data->LocalUserId).ToString().c_str());

	FGameEvent Event(EGameEventType::UserConnectAuthExpiration, FProductUserId(Data->LocalUserId));
	FGame::Get().OnGameEvent(Event);
}

void EOS_CALL FAuthentication::LoginStatusChangedCb(const EOS_Auth_LoginStatusChangedCallbackInfo* Data)
{
	assert(Data != NULL);

	FDebugLog::Log(L"[EOS SDK] Auth Login Status Changed - UserId: %ls, Prev: %d, New: %d", FEpicAccountId(Data->LocalUserId).ToString().c_str(), (int32_t)Data->PrevStatus, (int32_t)Data->CurrentStatus);

	if (Data->PrevStatus == EOS_ELoginStatus::EOS_LS_LoggedIn && Data->CurrentStatus == EOS_ELoginStatus::EOS_LS_NotLoggedIn)
	{
		// Log user out
		FGameEvent Event(EGameEventType::UserLoggedOut, Data->LocalUserId);
		FGame::Get().OnGameEvent(Event);
	}
}

void FAuthentication::OnConnectLoginComplete(EOS_EResult Result, EOS_EpicAccountId UserId, EOS_ProductUserId LocalUserId)
{
	PrintEOSAuthUsers();

	if (Result == EOS_EResult::EOS_Success)
	{
		FGameEvent Event(EGameEventType::UserConnectLoggedIn, UserId, LocalUserId);
		FGame::Get().OnGameEvent(Event);
	}
}

void FAuthentication::PrintAuthToken(EOS_Auth_Token* InAuthToken)
{
	FDebugLog::Log(L"[EOS SDK] AuthToken");
	FDebugLog::Log(L" - App: %ls", FStringUtils::Widen(InAuthToken->App).c_str());
	FDebugLog::Log(L" - ClientId: %ls", FStringUtils::Widen(InAuthToken->ClientId).c_str());
	FDebugLog::Log(L" - AccountId: %ls", FEpicAccountId(InAuthToken->AccountId).ToString().c_str());

	FDebugLog::Log(L" - AccessToken: %ls", FStringUtils::Widen(InAuthToken->AccessToken).c_str());
	FDebugLog::Log(L" - ExpiresIn: %0.2f", InAuthToken->ExpiresIn);
	FDebugLog::Log(L" - ExpiresAt: %ls", FStringUtils::Widen(InAuthToken->ExpiresAt).c_str());
}

void FAuthentication::PrintEOSAuthUsers()
{
	EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(FPlatform::GetPlatformHandle());
	assert(ConnectHandle != NULL);

	uint32_t NumAccounts = EOS_Connect_GetLoggedInUsersCount(ConnectHandle);
	for (uint32_t i = 0; i < NumAccounts; ++i)
	{
		EOS_ProductUserId ProductUserId = EOS_Connect_GetLoggedInUserByIndex(ConnectHandle, i);

		EOS_ELoginStatus LoginStatus = EOS_Connect_GetLoginStatus(ConnectHandle, ProductUserId);
		assert(LoginStatus == EOS_ELoginStatus::EOS_LS_LoggedIn);

		FDebugLog::Log(L"[EOS SDK] Product User Id: %ls, Status: %d", FProductUserId(ProductUserId).ToString().c_str(), LoginStatus);
	}
}

void FAuthentication::OnGameEvent(const FGameEvent& Event)
{
	if (Event.GetType() == EGameEventType::CheckAutoLogin)
	{
		CheckAutoLogin();
	}
	else if (Event.GetType() == EGameEventType::StartUserLogin)
	{
		ELoginMode LoginMode = (ELoginMode)Event.GetFirstExtendedType();
		ELoginExternalType LoginExternalType = (ELoginExternalType)Event.GetSecondExtendedType();
		if (!Login(LoginMode, Event.GetFirstStr(), Event.GetSecondStr(), LoginExternalType))
		{
			FGameEvent LoginFailedEvent(EGameEventType::UserLoginFailed, Event.GetUserId());
			FGame::Get().OnGameEvent(LoginFailedEvent);
		}
	}
	else if (Event.GetType() == EGameEventType::StartUserLogout)
	{
		FEpicAccountId UserId = Event.GetUserId();
		Logout(UserId);
	}
	else if (Event.GetType() == EGameEventType::DeletePersistentAuth)
	{
		DeletePersistentAuth();
	}
	else if (Event.GetType() == EGameEventType::UserLoginEnteredMFA)
	{
		SendMFACode(Event.GetFirstStr());
	}
	else if (Event.GetType() == EGameEventType::UserLoggedIn)
	{
		assert(AuthHandle != nullptr);

		FEpicAccountId UserId = Event.GetUserId();

		EOS_Auth_Token* UserAuthToken = nullptr;

		EOS_Auth_CopyUserAuthTokenOptions CopyTokenOptions = { 0 };
		CopyTokenOptions.ApiVersion = EOS_AUTH_COPYUSERAUTHTOKEN_API_LATEST;

		if (EOS_Auth_CopyUserAuthToken(AuthHandle, &CopyTokenOptions, UserId, &UserAuthToken) == EOS_EResult::EOS_Success)
		{
			PrintAuthToken(UserAuthToken);
			EOS_Auth_Token_Release(UserAuthToken);
		}

		ConnectLogin(UserId);
	}
	else if (Event.GetType() == EGameEventType::UserConnectAuthExpiration)
	{
		FProductUserId ProductUserId = Event.GetProductUserId();
		PlayerPtr Player = FPlayerManager::Get().GetPlayer(ProductUserId);
		if (Player != nullptr)
		{
			ConnectLogin(Player->GetUserID());
		}
		else
		{
			FDebugLog::LogError(L"[EOS SDK] Player not found, Product User Id: %ls", FProductUserId(ProductUserId).ToString().c_str());
		}
	}
	else if (Event.GetType() == EGameEventType::ContinuanceToken)
	{
		PendingContinuanceToken = Event.GetContinuanceToken();
	}
	else if (Event.GetType() == EGameEventType::ContinueLogin)
	{
		ContinueLogin(PendingContinuanceToken);
	}
	else if (Event.GetType() == EGameEventType::PrintAuth)
	{
		PrintAuthTokenInfo();
	}
}

void FAuthentication::SetDefaultLoginScope(EOS_EAuthScopeFlags LoginScope)
{
	DefaultLoginScope = LoginScope;
}

void FAuthentication::SendMFACode(std::wstring MFAStr)
{
	FDebugLog::LogWarning(L"[EOS SDK] MFA Code not supported yet, please use an account without MFA or use an alternative login method");

	// TODO: Send MFA Code via EOS SDK, just fail for now
	FGameEvent Event(EGameEventType::UserLoginFailed);
	FGame::Get().OnGameEvent(Event);
}

void FAuthentication::PrintAuthTokenInfo()
{
	PlayerPtr Player = FPlayerManager::Get().GetPlayer(FPlayerManager::Get().GetCurrentUser());
	if (Player == nullptr)
	{
		FDebugLog::LogError(L"[EOS SDK] Current player is invalid!");
		return;
	}

	FEpicAccountId UserId = Player->GetUserID();

	EOS_Auth_Token* UserAuthToken = nullptr;

	EOS_Auth_CopyUserAuthTokenOptions CopyTokenOptions = { 0 };
	CopyTokenOptions.ApiVersion = EOS_AUTH_COPYUSERAUTHTOKEN_API_LATEST;

	if (EOS_Auth_CopyUserAuthToken(AuthHandle, &CopyTokenOptions, UserId, &UserAuthToken) == EOS_EResult::EOS_Success)
	{
		PrintAuthToken(UserAuthToken);
		EOS_Auth_Token_Release(UserAuthToken);
	}
	else
	{
		FDebugLog::LogError(L"[EOS SDK] User Auth Token is invalid");
	}
}
