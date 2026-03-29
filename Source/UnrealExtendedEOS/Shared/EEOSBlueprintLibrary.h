// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Shared/EEOSTypes.h"
#include "EEOSBlueprintLibrary.generated.h"

class IOnlineSubsystem;

/**
 * Static Blueprint utility library for common EOS operations.
 * Accessible from any Blueprint node without needing a subsystem reference.
 */
UCLASS()
class UNREALEXTENDEDEOS_API UEEOSBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// ── EOS Status ───────────────────────────────────────────────────────────

	/** Check if the EOS Online Subsystem is initialized and ready */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EOS|Utilities")
	static bool IsEOSInitialized();

	/** Check if the local user is logged in */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EOS|Utilities")
	static bool IsLoggedIn(int32 LocalUserNum = 0);

	// ── Local User Info ──────────────────────────────────────────────────────

	/** Get the local user's Epic Account ID as a string */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EOS|Utilities")
	static FString GetLocalEpicAccountId(int32 LocalUserNum = 0);

	/** Get the local user's display name */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EOS|Utilities")
	static FString GetLocalDisplayName(int32 LocalUserNum = 0);

	/** Get the local user's product user ID as a string */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EOS|Utilities")
	static FString GetLocalProductUserId(int32 LocalUserNum = 0);

	/** Get the login status of the local user */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EOS|Utilities")
	static FString GetLoginStatus(int32 LocalUserNum = 0);

	// ── ID Validation ────────────────────────────────────────────────────────

	/** Check if a given string is a valid Epic Account ID format */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EOS|Utilities")
	static bool IsValidEpicAccountId(const FString& AccountId);

	/** Check if a given string is a valid Product User ID format */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EOS|Utilities")
	static bool IsValidProductUserId(const FString& ProductUserId);

	// ── String Conversions ───────────────────────────────────────────────────

	/** Convert a byte array to a hex string */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EOS|Utilities")
	static FString ByteArrayToHexString(const TArray<uint8>& Data);

	/** Convert a hex string to a byte array */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EOS|Utilities")
	static TArray<uint8> HexStringToByteArray(const FString& HexString);

	/** Convert a string to a UTF-8 byte array (useful for P2P/Chat payloads) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EOS|Utilities")
	static TArray<uint8> StringToBytes(const FString& String);

	/** Convert a UTF-8 byte array to a string */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EOS|Utilities")
	static FString BytesToString(const TArray<uint8>& Data);

	// ── Platform Info ────────────────────────────────────────────────────────

	/** Get the EOS subsystem name (e.g., "EOS", "EOSPlus") */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EOS|Utilities")
	static FString GetEOSSubsystemName();

	/** Get the number of registered local users */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EOS|Utilities")
	static int32 GetNumLocalUsers();

	/** Get the auth token for the local user (if available) */
	UFUNCTION(BlueprintCallable, Category = "EOS|Utilities")
	static FString GetAuthToken(int32 LocalUserNum = 0);

	// ── Subsystem Accessors ──────────────────────────────────────────────────
	// Quick access to subsystems from any Blueprint — avoids manual "Get Game Instance → Get Subsystem" chains.

	/** Get the Stats subsystem */
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (WorldContext = "WorldContext"), Category = "EOS|Subsystems")
	static class UEEOSStatsSubsystem* GetStatsSubsystem(UObject* WorldContext);

	/** Get the Achievements subsystem */
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (WorldContext = "WorldContext"), Category = "EOS|Subsystems")
	static class UEEOSAchievementSubsystem* GetAchievementSubsystem(UObject* WorldContext);

	/** Get the Auth subsystem */
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (WorldContext = "WorldContext"), Category = "EOS|Subsystems")
	static class UEEOSAuthSubsystem* GetAuthSubsystem(UObject* WorldContext);

	/** Get the Friends subsystem */
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (WorldContext = "WorldContext"), Category = "EOS|Subsystems")
	static class UEEOSFriendsSubsystem* GetFriendsSubsystem(UObject* WorldContext);

	/** Get the Presence subsystem */
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (WorldContext = "WorldContext"), Category = "EOS|Subsystems")
	static class UEEOSPresenceSubsystem* GetPresenceSubsystem(UObject* WorldContext);

	/** Get the Leaderboard subsystem */
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (WorldContext = "WorldContext"), Category = "EOS|Subsystems")
	static class UEEOSLeaderboardSubsystem* GetLeaderboardSubsystem(UObject* WorldContext);

	/** Get the Session subsystem */
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (WorldContext = "WorldContext"), Category = "EOS|Subsystems")
	static class UEEOSSessionSubsystem* GetSessionSubsystem(UObject* WorldContext);

	/** Get the Lobby subsystem */
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (WorldContext = "WorldContext"), Category = "EOS|Subsystems")
	static class UEEOSLobbySubsystem* GetLobbySubsystem(UObject* WorldContext);

	/** Get the Matchmaking subsystem */
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (WorldContext = "WorldContext"), Category = "EOS|Subsystems")
	static class UEEOSMatchmakingSubsystem* GetMatchmakingSubsystem(UObject* WorldContext);

	/** Get the Voice subsystem */
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (WorldContext = "WorldContext"), Category = "EOS|Subsystems")
	static class UEEOSVoiceSubsystem* GetVoiceSubsystem(UObject* WorldContext);

	/** Get the Chat subsystem */
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (WorldContext = "WorldContext"), Category = "EOS|Subsystems")
	static class UEEOSChatSubsystem* GetChatSubsystem(UObject* WorldContext);

	/** Get the Ecom (Store) subsystem */
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (WorldContext = "WorldContext"), Category = "EOS|Subsystems")
	static class UEEOSEcomSubsystem* GetEcomSubsystem(UObject* WorldContext);

	/** Get the Player Storage subsystem */
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (WorldContext = "WorldContext"), Category = "EOS|Subsystems")
	static class UEEOSPlayerStorageSubsystem* GetPlayerStorageSubsystem(UObject* WorldContext);

	/** Get the Sanctions subsystem */
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (WorldContext = "WorldContext"), Category = "EOS|Subsystems")
	static class UEEOSSanctionsSubsystem* GetSanctionsSubsystem(UObject* WorldContext);

	/** Get the User Info subsystem */
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (WorldContext = "WorldContext"), Category = "EOS|Subsystems")
	static class UEEOSUserInfoSubsystem* GetUserInfoSubsystem(UObject* WorldContext);

	/** Get the P2P subsystem */
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (WorldContext = "WorldContext"), Category = "EOS|Subsystems")
	static class UEEOSP2PSubsystem* GetP2PSubsystem(UObject* WorldContext);

	/** Get the Metrics subsystem */
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (WorldContext = "WorldContext"), Category = "EOS|Subsystems")
	static class UEEOSMetricsSubsystem* GetMetricsSubsystem(UObject* WorldContext);

	/** Get the UI subsystem */
	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (WorldContext = "WorldContext"), Category = "EOS|Subsystems")
	static class UEEOSUISubsystem* GetUISubsystem(UObject* WorldContext);

	// ── Presence Helpers ─────────────────────────────────────────────────────

	/** Quickly set rich presence text (e.g., "In Lobby", "Playing Match on Forest Map") */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContext"), Category = "EOS|Presence")
	static void SetRichPresence(UObject* WorldContext, const FString& RichText);

	/** Quickly set presence status + rich text in one call */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContext"), Category = "EOS|Presence")
	static void SetPresenceStatus(UObject* WorldContext, const FString& StatusString, const FString& RichText);

	// ── Time Helpers ─────────────────────────────────────────────────────────

	/** Convert a Unix timestamp (seconds since epoch) to a human-readable date string */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EOS|Utilities")
	static FString UnixTimestampToString(int64 UnixTimestamp);

	/** Get the current time as a Unix timestamp (seconds since epoch) */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EOS|Utilities")
	static int64 GetCurrentUnixTimestamp();

	// ── Stat Helpers ─────────────────────────────────────────────────────────

	/** Find a stat value by name from a stat array. Returns -1 if not found. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EOS|Stats")
	static int32 FindStatValue(const TArray<FEEOSStat>& Stats, const FString& StatName);

	/** Check if an achievement is unlocked in an achievement array. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EOS|Achievements")
	static bool IsAchievementUnlocked(const TArray<FEEOSAchievement>& Achievements, const FString& AchievementId);

	/** Get an achievement by ID. Returns false if not found. */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "EOS|Achievements")
	static bool GetAchievementById(const TArray<FEEOSAchievement>& Achievements, const FString& AchievementId, FEEOSAchievement& OutAchievement);

private:

	/** Internal helper to get the EOS subsystem */
	static IOnlineSubsystem* GetEOS();

	/** Internal helper for game instance subsystem access */
	template<typename T>
	static T* GetGISubsystem(UObject* WorldContext);
};
