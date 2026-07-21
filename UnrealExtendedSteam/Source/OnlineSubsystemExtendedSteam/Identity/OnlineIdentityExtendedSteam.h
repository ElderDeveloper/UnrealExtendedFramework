// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "OnlineSubsystemTypes.h"
#include "Core/OnlineSubsystemExtendedSteamPackage.h"

class FOnlineSubsystemExtendedSteam;

class FUniqueNetIdExtendedSteam;

using FUniqueNetIdExtendedSteamRef = TSharedRef<const FUniqueNetIdExtendedSteam>;
using FUniqueNetIdExtendedSteamPtr = TSharedPtr<const FUniqueNetIdExtendedSteam>;

/**
 * Unique net id wrapping a 64-bit CSteamID for the "EXTENDEDSTEAM" service.
 *
 * Deliberately independent of the engine's OnlineSubsystemSteam plugin (whose FUniqueNetIdSteam
 * is internal to it). String form is the decimal uint64 (SteamID64), byte form is the raw
 * 8-byte value. Instances are immutable and always held by TSharedRef/TSharedPtr.
 */
class FUniqueNetIdExtendedSteam : public FUniqueNetId
{
public:
	static FUniqueNetIdExtendedSteamRef Create(uint64 InSteamId64)
	{
		return MakeShareable(new FUniqueNetIdExtendedSteam(InSteamId64));
	}

	/** Parses a decimal SteamID64 string; null on malformed input. */
	static FUniqueNetIdExtendedSteamPtr Create(const FString& String)
	{
		if (String.IsEmpty() || String.Len() > 20)
		{
			return nullptr;
		}
		for (const TCHAR Char : String)
		{
			if (!FChar::IsDigit(Char))
			{
				return nullptr;
			}
		}

		uint64 Value = 0;
		LexFromString(Value, *String);
		return Create(Value);
	}

	/** Reconstructs an id from its raw byte form; null unless Size is exactly sizeof(uint64). */
	static FUniqueNetIdExtendedSteamPtr Create(const uint8* Bytes, int32 Size)
	{
		if (Bytes == nullptr || Size != sizeof(uint64))
		{
			return nullptr;
		}

		uint64 Value = 0;
		FMemory::Memcpy(&Value, Bytes, sizeof(uint64));
		return Create(Value);
	}

	/** Shared invalid id (SteamID64 = 0), used where a reference must be provided on failure. */
	static const FUniqueNetIdExtendedSteamRef& EmptyId()
	{
		static const FUniqueNetIdExtendedSteamRef Empty = Create(0ull);
		return Empty;
	}

	//~ Begin FUniqueNetId
	virtual FName GetType() const override
	{
		return ESTEAM_SUBSYSTEM;
	}

	virtual const uint8* GetBytes() const override
	{
		return reinterpret_cast<const uint8*>(&SteamId64);
	}

	virtual int32 GetSize() const override
	{
		return sizeof(SteamId64);
	}

	virtual bool IsValid() const override
	{
		return SteamId64 != 0;
	}

	virtual FString ToString() const override
	{
		return LexToString(SteamId64);
	}

	virtual FString ToDebugString() const override
	{
		return FString::Printf(TEXT("ExtendedSteam:%llu"), SteamId64);
	}

	virtual uint32 GetTypeHash() const override
	{
		return ::GetTypeHash(SteamId64);
	}
	//~ End FUniqueNetId

	uint64 GetSteamId64() const
	{
		return SteamId64;
	}

private:
	explicit FUniqueNetIdExtendedSteam(uint64 InSteamId64)
		: SteamId64(InSteamId64)
	{
	}

	uint64 SteamId64 = 0;
};

/** Minimal user account for the local Steam user: id + persona name, no attributes. */
class FUserOnlineAccountExtendedSteam : public FUserOnlineAccount
{
public:
	FUserOnlineAccountExtendedSteam(const FUniqueNetIdRef& InUserId, const FString& InDisplayName)
		: UserId(InUserId)
		, DisplayName(InDisplayName)
	{
	}

	//~ Begin FOnlineUser
	virtual FUniqueNetIdRef GetUserId() const override
	{
		return UserId;
	}

	virtual FString GetRealName() const override
	{
		// Steam does not expose real names through the client API.
		return FString();
	}

	virtual FString GetDisplayName(const FString& Platform = FString()) const override
	{
		return DisplayName;
	}

	virtual bool GetUserAttribute(const FString& AttrName, FString& OutAttrValue) const override
	{
		return false;
	}
	//~ End FOnlineUser

	//~ Begin FUserOnlineAccount
	virtual FString GetAccessToken() const override
	{
		// Session tickets are minted on demand by FOnlineIdentityExtendedSteam::GetAuthToken.
		return FString();
	}

	virtual bool GetAuthAttribute(const FString& AttrName, FString& OutAttrValue) const override
	{
		return false;
	}

	virtual bool SetUserAttribute(const FString& AttrName, const FString& AttrValue) override
	{
		return false;
	}
	//~ End FUserOnlineAccount

private:
	FUniqueNetIdRef UserId;
	FString DisplayName;
};

/**
 * Identity interface for the local Steam user (single local player, LocalUserNum 0).
 *
 * There is no interactive login on Steam: the user is whoever the running Steam client is signed
 * in as. Login/AutoLogin therefore complete synchronously — success when the Steam client API is
 * initialized (via FExtendedSteamSharedModule) and a user is logged on, failure otherwise.
 * GetAuthToken returns a hex-encoded session ticket in the format expected by backends such as
 * PlayFab LoginWithSteam.
 */
class FOnlineIdentityExtendedSteam : public IOnlineIdentity
{
public:
	explicit FOnlineIdentityExtendedSteam(FOnlineSubsystemExtendedSteam* InSubsystem)
		: Subsystem(InSubsystem)
	{
	}

	virtual ~FOnlineIdentityExtendedSteam() = default;

	//~ Begin IOnlineIdentity
	virtual bool Login(int32 LocalUserNum, const FOnlineAccountCredentials& AccountCredentials) override;
	virtual bool Logout(int32 LocalUserNum) override;
	virtual bool AutoLogin(int32 LocalUserNum) override;
	virtual TSharedPtr<FUserOnlineAccount> GetUserAccount(const FUniqueNetId& UserId) const override;
	virtual TArray<TSharedPtr<FUserOnlineAccount>> GetAllUserAccounts() const override;
	virtual FUniqueNetIdPtr GetUniquePlayerId(int32 LocalUserNum) const override;
	virtual FUniqueNetIdPtr CreateUniquePlayerId(uint8* Bytes, int32 Size) override;
	virtual FUniqueNetIdPtr CreateUniquePlayerId(const FString& Str) override;
	virtual ELoginStatus::Type GetLoginStatus(int32 LocalUserNum) const override;
	virtual ELoginStatus::Type GetLoginStatus(const FUniqueNetId& UserId) const override;
	virtual FString GetPlayerNickname(int32 LocalUserNum) const override;
	virtual FString GetPlayerNickname(const FUniqueNetId& UserId) const override;
	virtual FString GetAuthToken(int32 LocalUserNum) const override;
	virtual void RevokeAuthToken(const FUniqueNetId& LocalUserId, const FOnRevokeAuthTokenCompleteDelegate& Delegate) override;
	virtual void GetUserPrivilege(const FUniqueNetId& LocalUserId, EUserPrivileges::Type Privilege, const FOnGetUserPrivilegeCompleteDelegate& Delegate, EShowPrivilegeResolveUI ShowResolveUI = EShowPrivilegeResolveUI::Default) override;
	virtual FPlatformUserId GetPlatformUserIdFromUniqueNetId(const FUniqueNetId& UniqueNetId) const override;
	virtual FString GetAuthType() const override;
	//~ End IOnlineIdentity

private:
	/** SteamID64 of the local user; 0 when the Steam client API is unavailable. */
	uint64 GetLocalSteamId64() const;

	/** Cached local unique id, created on first use once Steam is up. May be null. */
	FUniqueNetIdExtendedSteamPtr GetLocalUniqueId() const;

	/** Builds the cached local user account when possible. */
	void EnsureLocalAccount() const;

	/** Owning subsystem (owns this object; outlives it). */
	FOnlineSubsystemExtendedSteam* Subsystem = nullptr;

	mutable FUniqueNetIdExtendedSteamPtr CachedLocalUserId;
	mutable TSharedPtr<FUserOnlineAccountExtendedSteam> LocalUserAccount;
};
