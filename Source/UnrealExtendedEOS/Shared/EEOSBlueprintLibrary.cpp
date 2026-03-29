// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSBlueprintLibrary.h"
#include "OnlineSubsystem.h"
#include "OnlineSubsystemUtils.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "UnrealExtendedEOS.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

// Subsystem includes
#include "Stats/EEOSStatsSubsystem.h"
#include "Achievements/EEOSAchievementSubsystem.h"
#include "Auth/EEOSAuthSubsystem.h"
#include "Social/EEOSFriendsSubsystem.h"
#include "Social/EEOSPresenceSubsystem.h"
#include "Leaderboards/EEOSLeaderboardSubsystem.h"
#include "Sessions/EEOSSessionSubsystem.h"
#include "Sessions/EEOSLobbySubsystem.h"
#include "Matchmaking/EEOSMatchmakingSubsystem.h"
#include "Voice/EEOSVoiceSubsystem.h"
#include "Chat/EEOSChatSubsystem.h"
#include "Ecom/EEOSEcomSubsystem.h"
#include "Storage/EEOSPlayerStorageSubsystem.h"
#include "Sanctions/EEOSSanctionsSubsystem.h"
#include "UserInfo/EEOSUserInfoSubsystem.h"
#include "P2P/EEOSP2PSubsystem.h"
#include "Metrics/EEOSMetricsSubsystem.h"
#include "UI/EEOSUISubsystem.h"

// ── Internal Helpers ─────────────────────────────────────────────────────────

IOnlineSubsystem* UEEOSBlueprintLibrary::GetEOS()
{
	return Online::GetSubsystem(nullptr, EOS_SUBSYSTEM);
}

template<typename T>
T* UEEOSBlueprintLibrary::GetGISubsystem(UObject* WorldContext)
{
	if (!WorldContext) return nullptr;
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
	if (!World) return nullptr;
	UGameInstance* GI = World->GetGameInstance();
	if (!GI) return nullptr;
	return GI->GetSubsystem<T>();
}

// ── EOS Status ───────────────────────────────────────────────────────────────

bool UEEOSBlueprintLibrary::IsEOSInitialized()
{
	return GetEOS() != nullptr;
}

bool UEEOSBlueprintLibrary::IsLoggedIn(int32 LocalUserNum)
{
	IOnlineSubsystem* EOSSub = GetEOS();
	if (!EOSSub) return false;

	IOnlineIdentityPtr Identity = EOSSub->GetIdentityInterface();
	if (!Identity.IsValid()) return false;

	return Identity->GetLoginStatus(LocalUserNum) == ELoginStatus::LoggedIn;
}

// ── Local User Info ──────────────────────────────────────────────────────────

FString UEEOSBlueprintLibrary::GetLocalEpicAccountId(int32 LocalUserNum)
{
	IOnlineSubsystem* EOSSub = GetEOS();
	if (!EOSSub) return FString();

	IOnlineIdentityPtr Identity = EOSSub->GetIdentityInterface();
	if (!Identity.IsValid()) return FString();

	FUniqueNetIdPtr UserId = Identity->GetUniquePlayerId(LocalUserNum);
	if (!UserId.IsValid()) return FString();

	return UserId->ToString();
}

FString UEEOSBlueprintLibrary::GetLocalDisplayName(int32 LocalUserNum)
{
	IOnlineSubsystem* EOSSub = GetEOS();
	if (!EOSSub) return FString();

	IOnlineIdentityPtr Identity = EOSSub->GetIdentityInterface();
	if (!Identity.IsValid()) return FString();

	return Identity->GetPlayerNickname(LocalUserNum);
}

FString UEEOSBlueprintLibrary::GetLocalProductUserId(int32 LocalUserNum)
{
	IOnlineSubsystem* EOSSub = GetEOS();
	if (!EOSSub) return FString();

	IOnlineIdentityPtr Identity = EOSSub->GetIdentityInterface();
	if (!Identity.IsValid()) return FString();

	FUniqueNetIdPtr UserId = Identity->GetUniquePlayerId(LocalUserNum);
	if (!UserId.IsValid()) return FString();

	return UserId->ToString();
}

FString UEEOSBlueprintLibrary::GetLoginStatus(int32 LocalUserNum)
{
	IOnlineSubsystem* EOSSub = GetEOS();
	if (!EOSSub) return TEXT("NotLoggedIn");

	IOnlineIdentityPtr Identity = EOSSub->GetIdentityInterface();
	if (!Identity.IsValid()) return TEXT("NotLoggedIn");

	ELoginStatus::Type Status = Identity->GetLoginStatus(LocalUserNum);
	switch (Status)
	{
	case ELoginStatus::LoggedIn: return TEXT("LoggedIn");
	case ELoginStatus::UsingLocalProfile: return TEXT("UsingLocalProfile");
	case ELoginStatus::NotLoggedIn: return TEXT("NotLoggedIn");
	default: return TEXT("Unknown");
	}
}

// ── ID Validation ────────────────────────────────────────────────────────────

bool UEEOSBlueprintLibrary::IsValidEpicAccountId(const FString& AccountId)
{
	if (AccountId.Len() != 32) return false;

	for (TCHAR Char : AccountId)
	{
		if (!FChar::IsHexDigit(Char)) return false;
	}
	return true;
}

bool UEEOSBlueprintLibrary::IsValidProductUserId(const FString& ProductUserId)
{
	if (ProductUserId.Len() < 16 || ProductUserId.Len() > 64) return false;

	for (TCHAR Char : ProductUserId)
	{
		if (!FChar::IsHexDigit(Char)) return false;
	}
	return true;
}

// ── String Conversions ───────────────────────────────────────────────────────

FString UEEOSBlueprintLibrary::ByteArrayToHexString(const TArray<uint8>& Data)
{
	return FString::FromHexBlob(Data.GetData(), Data.Num());
}

TArray<uint8> UEEOSBlueprintLibrary::HexStringToByteArray(const FString& HexString)
{
	TArray<uint8> Result;
	Result.SetNum(HexString.Len() / 2);
	FString::ToHexBlob(HexString, Result.GetData(), Result.Num());
	return Result;
}

FString UEEOSBlueprintLibrary::BytesToString(const TArray<uint8>& Data)
{
	if (Data.Num() == 0) return FString();

	FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Data.GetData()), Data.Num());
	return FString(Converter.Length(), Converter.Get());
}

TArray<uint8> UEEOSBlueprintLibrary::StringToBytes(const FString& String)
{
	TArray<uint8> Result;
	if (String.IsEmpty()) return Result;

	FTCHARToUTF8 Converter(*String);
	Result.Append(reinterpret_cast<const uint8*>(Converter.Get()), Converter.Length());
	return Result;
}

// ── Platform Info ────────────────────────────────────────────────────────────

FString UEEOSBlueprintLibrary::GetEOSSubsystemName()
{
	IOnlineSubsystem* EOSSub = GetEOS();
	if (!EOSSub) return TEXT("None");
	return EOSSub->GetSubsystemName().ToString();
}

int32 UEEOSBlueprintLibrary::GetNumLocalUsers()
{
	IOnlineSubsystem* EOSSub = GetEOS();
	if (!EOSSub) return 0;

	IOnlineIdentityPtr Identity = EOSSub->GetIdentityInterface();
	if (!Identity.IsValid()) return 0;

	int32 Count = 0;
	for (int32 i = 0; i < 4; i++)
	{
		if (Identity->GetLoginStatus(i) != ELoginStatus::NotLoggedIn)
		{
			Count++;
		}
	}
	return Count;
}

FString UEEOSBlueprintLibrary::GetAuthToken(int32 LocalUserNum)
{
	IOnlineSubsystem* EOSSub = GetEOS();
	if (!EOSSub) return FString();

	IOnlineIdentityPtr Identity = EOSSub->GetIdentityInterface();
	if (!Identity.IsValid()) return FString();

	return Identity->GetAuthToken(LocalUserNum);
}

// ── Subsystem Accessors ──────────────────────────────────────────────────────

UEEOSStatsSubsystem* UEEOSBlueprintLibrary::GetStatsSubsystem(UObject* WorldContext)
{
	return GetGISubsystem<UEEOSStatsSubsystem>(WorldContext);
}

UEEOSAchievementSubsystem* UEEOSBlueprintLibrary::GetAchievementSubsystem(UObject* WorldContext)
{
	return GetGISubsystem<UEEOSAchievementSubsystem>(WorldContext);
}

UEEOSAuthSubsystem* UEEOSBlueprintLibrary::GetAuthSubsystem(UObject* WorldContext)
{
	return GetGISubsystem<UEEOSAuthSubsystem>(WorldContext);
}

UEEOSFriendsSubsystem* UEEOSBlueprintLibrary::GetFriendsSubsystem(UObject* WorldContext)
{
	return GetGISubsystem<UEEOSFriendsSubsystem>(WorldContext);
}

UEEOSPresenceSubsystem* UEEOSBlueprintLibrary::GetPresenceSubsystem(UObject* WorldContext)
{
	return GetGISubsystem<UEEOSPresenceSubsystem>(WorldContext);
}

UEEOSLeaderboardSubsystem* UEEOSBlueprintLibrary::GetLeaderboardSubsystem(UObject* WorldContext)
{
	return GetGISubsystem<UEEOSLeaderboardSubsystem>(WorldContext);
}

UEEOSSessionSubsystem* UEEOSBlueprintLibrary::GetSessionSubsystem(UObject* WorldContext)
{
	return GetGISubsystem<UEEOSSessionSubsystem>(WorldContext);
}

UEEOSLobbySubsystem* UEEOSBlueprintLibrary::GetLobbySubsystem(UObject* WorldContext)
{
	return GetGISubsystem<UEEOSLobbySubsystem>(WorldContext);
}

UEEOSMatchmakingSubsystem* UEEOSBlueprintLibrary::GetMatchmakingSubsystem(UObject* WorldContext)
{
	return GetGISubsystem<UEEOSMatchmakingSubsystem>(WorldContext);
}

UEEOSVoiceSubsystem* UEEOSBlueprintLibrary::GetVoiceSubsystem(UObject* WorldContext)
{
	return GetGISubsystem<UEEOSVoiceSubsystem>(WorldContext);
}

UEEOSChatSubsystem* UEEOSBlueprintLibrary::GetChatSubsystem(UObject* WorldContext)
{
	return GetGISubsystem<UEEOSChatSubsystem>(WorldContext);
}

UEEOSEcomSubsystem* UEEOSBlueprintLibrary::GetEcomSubsystem(UObject* WorldContext)
{
	return GetGISubsystem<UEEOSEcomSubsystem>(WorldContext);
}

UEEOSPlayerStorageSubsystem* UEEOSBlueprintLibrary::GetPlayerStorageSubsystem(UObject* WorldContext)
{
	return GetGISubsystem<UEEOSPlayerStorageSubsystem>(WorldContext);
}

UEEOSSanctionsSubsystem* UEEOSBlueprintLibrary::GetSanctionsSubsystem(UObject* WorldContext)
{
	return GetGISubsystem<UEEOSSanctionsSubsystem>(WorldContext);
}

UEEOSUserInfoSubsystem* UEEOSBlueprintLibrary::GetUserInfoSubsystem(UObject* WorldContext)
{
	return GetGISubsystem<UEEOSUserInfoSubsystem>(WorldContext);
}

UEEOSP2PSubsystem* UEEOSBlueprintLibrary::GetP2PSubsystem(UObject* WorldContext)
{
	return GetGISubsystem<UEEOSP2PSubsystem>(WorldContext);
}

UEEOSMetricsSubsystem* UEEOSBlueprintLibrary::GetMetricsSubsystem(UObject* WorldContext)
{
	return GetGISubsystem<UEEOSMetricsSubsystem>(WorldContext);
}

UEEOSUISubsystem* UEEOSBlueprintLibrary::GetUISubsystem(UObject* WorldContext)
{
	return GetGISubsystem<UEEOSUISubsystem>(WorldContext);
}

// ── Presence Helpers ─────────────────────────────────────────────────────────

void UEEOSBlueprintLibrary::SetRichPresence(UObject* WorldContext, const FString& RichText)
{
	auto* Sub = GetGISubsystem<UEEOSPresenceSubsystem>(WorldContext);
	if (Sub)
	{
		Sub->SetPresence(TEXT("Online"), RichText);
	}
}

void UEEOSBlueprintLibrary::SetPresenceStatus(UObject* WorldContext, const FString& StatusString, const FString& RichText)
{
	auto* Sub = GetGISubsystem<UEEOSPresenceSubsystem>(WorldContext);
	if (Sub)
	{
		Sub->SetPresence(StatusString, RichText);
	}
}

// ── Time Helpers ─────────────────────────────────────────────────────────────

FString UEEOSBlueprintLibrary::UnixTimestampToString(int64 UnixTimestamp)
{
	FDateTime DateTime = FDateTime::FromUnixTimestamp(UnixTimestamp);
	return DateTime.ToString(TEXT("%Y-%m-%d %H:%M:%S"));
}

int64 UEEOSBlueprintLibrary::GetCurrentUnixTimestamp()
{
	return FDateTime::UtcNow().ToUnixTimestamp();
}

// ── Stat & Achievement Helpers ───────────────────────────────────────────────

int32 UEEOSBlueprintLibrary::FindStatValue(const TArray<FEEOSStat>& Stats, const FString& StatName)
{
	for (const FEEOSStat& Stat : Stats)
	{
		if (Stat.StatName == StatName)
		{
			return Stat.Value;
		}
	}
	return -1;
}

bool UEEOSBlueprintLibrary::IsAchievementUnlocked(const TArray<FEEOSAchievement>& Achievements, const FString& AchievementId)
{
	for (const FEEOSAchievement& Achievement : Achievements)
	{
		if (Achievement.AchievementId == AchievementId)
		{
			return Achievement.Progress >= 1.0f;
		}
	}
	return false;
}

bool UEEOSBlueprintLibrary::GetAchievementById(const TArray<FEEOSAchievement>& Achievements, const FString& AchievementId, FEEOSAchievement& OutAchievement)
{
	for (const FEEOSAchievement& Achievement : Achievements)
	{
		if (Achievement.AchievementId == AchievementId)
		{
			OutAchievement = Achievement;
			return true;
		}
	}
	return false;
}
