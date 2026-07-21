// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Identity/OnlineIdentityExtendedSteam.h"
#include "Core/OnlineSubsystemExtendedSteam.h"
#include "Auth/OnlineAuthExtendedSteam.h"
#include "ExtendedSteamSharedModule.h"
#include "Shared/ESteamSDK.h"

#include "OnlineError.h"

#if WITH_EXTENDEDSTEAM_SDK
THIRD_PARTY_INCLUDES_START
#include "steam/steam_api.h"
THIRD_PARTY_INCLUDES_END
#endif

namespace ExtendedSteamIdentity
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

uint64 FOnlineIdentityExtendedSteam::GetLocalSteamId64() const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (ExtendedSteamIdentity::IsSteamClientUp() && SteamUser() != nullptr)
	{
		return SteamUser()->GetSteamID().ConvertToUint64();
	}
#endif
	return 0;
}

FUniqueNetIdExtendedSteamPtr FOnlineIdentityExtendedSteam::GetLocalUniqueId() const
{
	if (!CachedLocalUserId.IsValid())
	{
		const uint64 SteamId64 = GetLocalSteamId64();
		if (SteamId64 != 0)
		{
			CachedLocalUserId = FUniqueNetIdExtendedSteam::Create(SteamId64);
		}
	}
	return CachedLocalUserId;
}

void FOnlineIdentityExtendedSteam::EnsureLocalAccount() const
{
	if (LocalUserAccount.IsValid())
	{
		return;
	}

	const FUniqueNetIdExtendedSteamPtr LocalId = GetLocalUniqueId();
	if (LocalId.IsValid() && LocalId->IsValid())
	{
		LocalUserAccount = MakeShared<FUserOnlineAccountExtendedSteam>(LocalId.ToSharedRef(), GetPlayerNickname(0));
	}
}

bool FOnlineIdentityExtendedSteam::Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials)
{
	// Steam has exactly one local user and no interactive login: the credentials are ignored and
	// the call completes synchronously against the Steam client's signed-in user.
	if (LocalUserNum != 0)
	{
		const FString Error = FString::Printf(TEXT("Steam supports a single local user; invalid LocalUserNum %d"), LocalUserNum);
		UE_LOG(LogExtendedSteam, Warning, TEXT("Login: %s"), *Error);
		TriggerOnLoginCompleteDelegates(LocalUserNum, false, *FUniqueNetIdExtendedSteam::EmptyId(), Error);
		return false;
	}

	const FUniqueNetIdExtendedSteamPtr LocalId = GetLocalUniqueId();
	if (!LocalId.IsValid() || !LocalId->IsValid())
	{
		const FString Error = TEXT("Steam client is not initialized or no user is logged on");
		UE_LOG(LogExtendedSteam, Warning, TEXT("Login: %s"), *Error);
		TriggerOnLoginCompleteDelegates(LocalUserNum, false, *FUniqueNetIdExtendedSteam::EmptyId(), Error);
		return false;
	}

	EnsureLocalAccount();

	UE_LOG(LogExtendedSteam, Log, TEXT("Login: local Steam user %s (%s) on subsystem instance %s"),
		*GetPlayerNickname(0), *LocalId->ToDebugString(),
		Subsystem != nullptr ? *Subsystem->GetInstanceName().ToString() : TEXT("<none>"));
	TriggerOnLoginCompleteDelegates(LocalUserNum, true, *LocalId, FString());
	TriggerOnLoginStatusChangedDelegates(LocalUserNum, ELoginStatus::NotLoggedIn, GetLoginStatus(LocalUserNum), *LocalId);
	return true;
}

bool FOnlineIdentityExtendedSteam::Logout(int32 LocalUserNum)
{
	// The local Steam user cannot be signed out from inside the game; mirror the platform OSS
	// behavior and report failure through the delegate.
	UE_LOG(LogExtendedSteam, Verbose, TEXT("Logout: the local Steam user cannot be logged out by the game"));
	TriggerOnLogoutCompleteDelegates(LocalUserNum, false);
	return false;
}

bool FOnlineIdentityExtendedSteam::AutoLogin(int32 LocalUserNum)
{
	return Login(LocalUserNum, FOnlineAccountCredentials());
}

TSharedPtr<FUserOnlineAccount> FOnlineIdentityExtendedSteam::GetUserAccount(const FUniqueNetId& UserId) const
{
	EnsureLocalAccount();
	if (LocalUserAccount.IsValid() && *LocalUserAccount->GetUserId() == UserId)
	{
		return LocalUserAccount;
	}
	return nullptr;
}

TArray<TSharedPtr<FUserOnlineAccount>> FOnlineIdentityExtendedSteam::GetAllUserAccounts() const
{
	TArray<TSharedPtr<FUserOnlineAccount>> Accounts;

	EnsureLocalAccount();
	if (LocalUserAccount.IsValid())
	{
		Accounts.Add(LocalUserAccount);
	}
	return Accounts;
}

FUniqueNetIdPtr FOnlineIdentityExtendedSteam::GetUniquePlayerId(int32 LocalUserNum) const
{
	if (LocalUserNum == 0)
	{
		return GetLocalUniqueId();
	}
	return nullptr;
}

FUniqueNetIdPtr FOnlineIdentityExtendedSteam::CreateUniquePlayerId(uint8* Bytes, int32 Size)
{
	return FUniqueNetIdExtendedSteam::Create(Bytes, Size);
}

FUniqueNetIdPtr FOnlineIdentityExtendedSteam::CreateUniquePlayerId(const FString& Str)
{
	return FUniqueNetIdExtendedSteam::Create(Str);
}

ELoginStatus::Type FOnlineIdentityExtendedSteam::GetLoginStatus(int32 LocalUserNum) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (LocalUserNum == 0
		&& ExtendedSteamIdentity::IsSteamClientUp()
		&& SteamUser() != nullptr
		&& SteamUser()->BLoggedOn())
	{
		return ELoginStatus::LoggedIn;
	}
#endif
	return ELoginStatus::NotLoggedIn;
}

ELoginStatus::Type FOnlineIdentityExtendedSteam::GetLoginStatus(const FUniqueNetId& UserId) const
{
	const FUniqueNetIdPtr LocalId = GetUniquePlayerId(0);
	if (LocalId.IsValid() && *LocalId == UserId)
	{
		return GetLoginStatus(0);
	}
	return ELoginStatus::NotLoggedIn;
}

FString FOnlineIdentityExtendedSteam::GetPlayerNickname(int32 LocalUserNum) const
{
#if WITH_EXTENDEDSTEAM_SDK
	if (LocalUserNum == 0 && ExtendedSteamIdentity::IsSteamClientUp() && SteamFriends() != nullptr)
	{
		return FString(UTF8_TO_TCHAR(SteamFriends()->GetPersonaName()));
	}
#endif
	return FString();
}

FString FOnlineIdentityExtendedSteam::GetPlayerNickname(const FUniqueNetId& UserId) const
{
	const FUniqueNetIdPtr LocalId = GetUniquePlayerId(0);
	if (LocalId.IsValid() && *LocalId == UserId)
	{
		return GetPlayerNickname(0);
	}
	// Nicknames of remote users arrive with the friends interface in a later phase.
	return FString();
}

FString FOnlineIdentityExtendedSteam::GetAuthToken(int32 LocalUserNum) const
{
	// IOnlineIdentity::GetAuthToken is a synchronous FString contract, so it returns a classic
	// hex-encoded GetAuthSessionTicket — the format backends such as PlayFab LoginWithSteam accept.
	//
	// The PREFERRED modern backend path is the web-API ticket (GetTicketForWebApiResponse_t), which
	// is inherently asynchronous and therefore cannot be surfaced through this synchronous getter.
	// Callers that can await a result should instead go through the plugin's Auth interface
	// (FOnlineSubsystemExtendedSteam::GetAuthInterfaceExtended()->GetAuthTicketForWebApi(...)) and
	// receive the ticket via FOnExtendedSteamWebApiTicket; that path is used automatically wherever
	// the SDK supports it (>= 1.57). Here we fall back to the synchronous session ticket, minting it
	// through the same Auth interface so both paths share one implementation.
#if WITH_EXTENDEDSTEAM_SDK
	if (LocalUserNum == 0 && Subsystem != nullptr)
	{
		const FOnlineAuthExtendedSteamPtr Auth = Subsystem->GetAuthInterfaceExtended();
		if (Auth.IsValid())
		{
			TArray<uint8> TicketBytes;
			FString HexTicket;
			// Each call mints a fresh unbound ticket; Steam invalidates it automatically when the app exits.
			if (Auth->GetAuthSessionTicket(TicketBytes, HexTicket) != 0 && !HexTicket.IsEmpty())
			{
				return HexTicket;
			}
			UE_LOG(LogExtendedSteam, Warning, TEXT("GetAuthToken: Auth interface returned no session ticket"));
		}
	}
#endif
	return FString();
}

void FOnlineIdentityExtendedSteam::RevokeAuthToken(const FUniqueNetId& LocalUserId, const FOnRevokeAuthTokenCompleteDelegate& Delegate)
{
	// Steam session tickets are revoked per-handle (CancelAuthTicket); a blanket revoke is not supported.
	UE_LOG(LogExtendedSteam, Verbose, TEXT("RevokeAuthToken: not supported for Steam"));
	Delegate.ExecuteIfBound(LocalUserId, FOnlineError(EOnlineErrorResult::NotImplemented));
}

void FOnlineIdentityExtendedSteam::GetUserPrivilege(const FUniqueNetId& LocalUserId, EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate, EShowPrivilegeResolveUI ShowResolveUI)
{
	// Steam imposes no per-privilege restrictions the client API can query; report success.
	Delegate.ExecuteIfBound(LocalUserId, Privilege, static_cast<uint32>(EPrivilegeResults::NoFailures));
}

FPlatformUserId FOnlineIdentityExtendedSteam::GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& UniqueNetId) const
{
	const FUniqueNetIdPtr LocalId = GetUniquePlayerId(0);
	if (LocalId.IsValid() && *LocalId == UniqueNetId)
	{
		return GetPlatformUserIdFromLocalUserNum(0);
	}
	return PLATFORMUSERID_NONE;
}

FString FOnlineIdentityExtendedSteam::GetAuthType() const
{
	return TEXT("steam");
}
