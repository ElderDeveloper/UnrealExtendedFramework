// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/ESteamWebSubsystem.h"
#include "ESteamWebCheatReportingSubsystem.generated.h"

/**
 * ICheatReportingService — game-ban style cheat reporting, ban management and VAC session status
 * (partner host, publisher Web API key required; server-side only).
 */
UCLASS()
class EXTENDEDSTEAMWEB_API UESteamWebCheatReportingSubsystem : public UESteamWebSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * ICheatReportingService/ReportPlayerCheating/v1 (POST) — files a cheating report against a player.
	 * SteamIdReporter (the reporting player) is omitted when empty; AppData (game-specific report
	 * context), GameMode, SuspicionStartTime (Unix time) and Severity are omitted when <= 0.
	 * bHeuristic/bDetection/bPlayerReport describe the evidence source; bNoReportId skips
	 * report-id generation for high-volume reporting.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Cheat Reporting", meta = (AutoCreateRefTerm = "OnResponse"))
	void ReportPlayerCheating(FString SteamId, int32 AppId, FString SteamIdReporter, int64 AppData, bool bHeuristic, bool bDetection, bool bPlayerReport, bool bNoReportId, int32 GameMode, int32 SuspicionStartTime, int32 Severity, const FOnSteamWebResponse& OnResponse);

	/**
	 * ICheatReportingService/RequestPlayerGameBan/v1 (POST) — requests a game ban on a player,
	 * referencing the ReportId from ReportPlayerCheating. Duration is in seconds (0 = permanent);
	 * bDelayBan delays the ban to obfuscate the detection.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Cheat Reporting", meta = (AutoCreateRefTerm = "OnResponse"))
	void RequestPlayerGameBan(FString SteamId, int32 AppId, int64 ReportId, FString CheatDescription, int32 Duration, bool bDelayBan, const FOnSteamWebResponse& OnResponse);

	/** ICheatReportingService/RemovePlayerGameBan/v1 (POST) — lifts a previously issued game ban from the player. */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Cheat Reporting", meta = (AutoCreateRefTerm = "OnResponse"))
	void RemovePlayerGameBan(FString SteamId, int32 AppId, const FOnSteamWebResponse& OnResponse);

	/**
	 * ICheatReportingService/GetCheatingReports/v1 — cheating reports for the app in the
	 * [TimeBegin, TimeEnd] Unix-time window, starting at ReportIdMin. bIncludeReports/bIncludeBans
	 * select the record types; SteamId filters to one player (omitted when empty).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Cheat Reporting", meta = (AutoCreateRefTerm = "OnResponse"))
	void GetCheatingReports(int32 AppId, int32 TimeEnd, int32 TimeBegin, FString ReportIdMin, bool bIncludeReports, bool bIncludeBans, FString SteamId, const FOnSteamWebResponse& OnResponse);

	/**
	 * ICheatReportingService/ReportCheatData/v1 (POST) — reports concrete cheat data observed on a
	 * player: the cheat's path/file name or web URL, observation times (Unix), cheat name,
	 * process ids and game-defined cheat parameters. Empty strings and <= 0 values are omitted.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Cheat Reporting", meta = (AutoCreateRefTerm = "OnResponse"))
	void ReportCheatData(FString SteamId, int32 AppId, FString PathAndFileName, FString WebCheatUrl, int64 TimeNow, int64 TimeStarted, int64 TimeStopped, FString CheatName, int32 GameProcessId, int32 CheatProcessId, int64 CheatParam1, int64 CheatParam2, const FOnSteamWebResponse& OnResponse);

	/**
	 * ICheatReportingService/RequestVacStatusForUser/v1 (POST) — VAC ban/session status for a user (publisher key).
	 * AppId <= 0 falls back to the configured AppId; SessionId (from a secure multiplayer session) is omitted when empty.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Cheat Reporting", meta = (AutoCreateRefTerm = "OnResponse"))
	void RequestVacStatusForUser(FString SteamId, int32 AppId, FString SessionId, const FOnSteamWebResponse& OnResponse);

	/**
	 * ICheatReportingService/StartSecureMultiplayerSession/v1 (POST) — opens a VAC-monitored secure
	 * session for a player (publisher key). The response carries a session id to feed back into
	 * RequestVacStatusForUser and EndSecureMultiplayerSession. An empty SteamId falls back to the
	 * configured DevSteamId; AppId <= 0 falls back to the configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Cheat Reporting", meta = (AutoCreateRefTerm = "OnResponse"))
	void StartSecureMultiplayerSession(int32 AppId, FString SteamId, const FOnSteamWebResponse& OnResponse);

	/**
	 * ICheatReportingService/EndSecureMultiplayerSession/v1 (POST) — closes the secure session opened
	 * by StartSecureMultiplayerSession (publisher key). SessionId is the session_id returned from that
	 * call. An empty SteamId falls back to the configured DevSteamId; AppId <= 0 falls back to the
	 * configured AppId.
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam Web|Cheat Reporting", meta = (AutoCreateRefTerm = "OnResponse"))
	void EndSecureMultiplayerSession(int32 AppId, FString SteamId, FString SessionId, const FOnSteamWebResponse& OnResponse);
};
