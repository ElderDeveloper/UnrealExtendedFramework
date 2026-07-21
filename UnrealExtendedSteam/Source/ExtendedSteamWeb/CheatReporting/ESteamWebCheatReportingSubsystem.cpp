// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "CheatReporting/ESteamWebCheatReportingSubsystem.h"
#include "Core/ESteamWebSettings.h"

void UESteamWebCheatReportingSubsystem::ReportPlayerCheating(FString SteamId, int32 AppId, FString SteamIdReporter, int64 AppData, bool bHeuristic, bool bDetection, bool bPlayerReport, bool bNoReportId, int32 GameMode, int32 SuspicionStartTime, int32 Severity, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ICheatReportingService"), TEXT("ReportPlayerCheating"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), SteamId);
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	if (!SteamIdReporter.IsEmpty())
	{
		Request.AddParam(TEXT("steamidreporter"), SteamIdReporter);
	}
	if (AppData > 0)
	{
		Request.AddParam(TEXT("appdata"), AppData);
	}
	Request.AddParam(TEXT("heuristic"), bHeuristic);
	Request.AddParam(TEXT("detection"), bDetection);
	Request.AddParam(TEXT("playerreport"), bPlayerReport);
	Request.AddParam(TEXT("noreportid"), bNoReportId);
	if (GameMode > 0)
	{
		Request.AddParam(TEXT("gamemode"), GameMode);
	}
	if (SuspicionStartTime > 0)
	{
		Request.AddParam(TEXT("suspicionstarttime"), SuspicionStartTime);
	}
	if (Severity > 0)
	{
		Request.AddParam(TEXT("severity"), Severity);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebCheatReportingSubsystem::RequestPlayerGameBan(FString SteamId, int32 AppId, int64 ReportId, FString CheatDescription, int32 Duration, bool bDelayBan, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ICheatReportingService"), TEXT("RequestPlayerGameBan"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), SteamId);
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("reportid"), ReportId);
	Request.AddParam(TEXT("cheatdescription"), CheatDescription);
	Request.AddParam(TEXT("duration"), Duration);
	Request.AddParam(TEXT("delayban"), bDelayBan);

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebCheatReportingSubsystem::RemovePlayerGameBan(FString SteamId, int32 AppId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ICheatReportingService"), TEXT("RemovePlayerGameBan"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), SteamId);
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebCheatReportingSubsystem::GetCheatingReports(int32 AppId, int32 TimeEnd, int32 TimeBegin, FString ReportIdMin, bool bIncludeReports, bool bIncludeBans, FString SteamId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ICheatReportingService"), TEXT("GetCheatingReports"), 1, EESteamWebVerb::Get, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("timeend"), TimeEnd);
	Request.AddParam(TEXT("timebegin"), TimeBegin);
	if (!ReportIdMin.IsEmpty())
	{
		Request.AddParam(TEXT("reportidmin"), ReportIdMin);
	}
	Request.AddParam(TEXT("includereports"), bIncludeReports);
	Request.AddParam(TEXT("includebans"), bIncludeBans);
	if (!SteamId.IsEmpty())
	{
		Request.AddParam(TEXT("steamid"), SteamId);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebCheatReportingSubsystem::ReportCheatData(FString SteamId, int32 AppId, FString PathAndFileName, FString WebCheatUrl, int64 TimeNow, int64 TimeStarted, int64 TimeStopped, FString CheatName, int32 GameProcessId, int32 CheatProcessId, int64 CheatParam1, int64 CheatParam2, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ICheatReportingService"), TEXT("ReportCheatData"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), SteamId);
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	if (!PathAndFileName.IsEmpty())
	{
		Request.AddParam(TEXT("pathandfilename"), PathAndFileName);
	}
	if (!WebCheatUrl.IsEmpty())
	{
		Request.AddParam(TEXT("webcheaturl"), WebCheatUrl);
	}
	if (TimeNow > 0)
	{
		Request.AddParam(TEXT("time_now"), TimeNow);
	}
	if (TimeStarted > 0)
	{
		Request.AddParam(TEXT("time_started"), TimeStarted);
	}
	if (TimeStopped > 0)
	{
		Request.AddParam(TEXT("time_stopped"), TimeStopped);
	}
	if (!CheatName.IsEmpty())
	{
		Request.AddParam(TEXT("cheatname"), CheatName);
	}
	if (GameProcessId > 0)
	{
		Request.AddParam(TEXT("game_process_id"), GameProcessId);
	}
	if (CheatProcessId > 0)
	{
		Request.AddParam(TEXT("cheat_process_id"), CheatProcessId);
	}
	if (CheatParam1 > 0)
	{
		Request.AddParam(TEXT("cheat_param_1"), CheatParam1);
	}
	if (CheatParam2 > 0)
	{
		Request.AddParam(TEXT("cheat_param_2"), CheatParam2);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebCheatReportingSubsystem::RequestVacStatusForUser(FString SteamId, int32 AppId, FString SessionId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ICheatReportingService"), TEXT("RequestVacStatusForUser"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	if (!SessionId.IsEmpty())
	{
		Request.AddParam(TEXT("session_id"), SessionId);
	}

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebCheatReportingSubsystem::StartSecureMultiplayerSession(int32 AppId, FString SteamId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ICheatReportingService"), TEXT("StartSecureMultiplayerSession"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));

	SendWebRequest(MoveTemp(Request), OnResponse);
}

void UESteamWebCheatReportingSubsystem::EndSecureMultiplayerSession(int32 AppId, FString SteamId, FString SessionId, const FOnSteamWebResponse& OnResponse)
{
	FESteamWebRequest Request = MakeRequest(TEXT("ICheatReportingService"), TEXT("EndSecureMultiplayerSession"), 1, EESteamWebVerb::Post, /*bUsePartnerHost*/ true);

	Request.AddParam(TEXT("steamid"), ResolveSteamId(SteamId));
	Request.AddParam(TEXT("appid"), ResolveAppId(AppId));
	Request.AddParam(TEXT("session_id"), SessionId);

	SendWebRequest(MoveTemp(Request), OnResponse);
}
