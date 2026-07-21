// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "DebugLog.h"
#include "StringUtils.h"
#include "AccountHelpers.h"
#include "Player.h"
#include "Main.h"
#include "Platform.h"
#include "GameEvent.h"
#include "Metrics.h"

FMetrics::FMetrics()
{
		
}

FMetrics::~FMetrics()
{
		
}

void FMetrics::Init()
{
	if (FPlatform::GetPlatformHandle() != nullptr)
	{
		MetricsHandle = EOS_Platform_GetMetricsInterface(FPlatform::GetPlatformHandle());
	}
}

void FMetrics::BeginPlayerSession(FEpicAccountId UserId)
{
	if (MetricsHandle == nullptr)
	{
		FDebugLog::LogError(L"MetricsHandle is invalid, can't begin player session metrics!");
		return;
	}

	if (!UserId.IsValid())
	{
		FDebugLog::LogError(L"User Id invalid, can't begin player session metrics!");
		return;
	}

	FDebugLog::Log(L"[EOS SDK] Metrics - Begin Player Session");

	std::wstring LocalDisplayName = FPlayerManager::Get().GetDisplayName(UserId);
	static char DisplayName[128];
	sprintf_s(DisplayName, sizeof(DisplayName), "%s", FStringUtils::Narrow(LocalDisplayName).c_str());

	// Begin Player Session Metrics
	EOS_Metrics_BeginPlayerSessionOptions MetricsOptions = {};
	MetricsOptions.ApiVersion = EOS_METRICS_BEGINPLAYERSESSION_API_LATEST;
	MetricsOptions.AccountIdType = EOS_EMetricsAccountIdType::EOS_MAIT_Epic;
	MetricsOptions.AccountId.Epic = UserId;
	MetricsOptions.DisplayName = DisplayName;
	MetricsOptions.ControllerType = EOS_EUserControllerType::EOS_UCT_MouseKeyboard;
	MetricsOptions.ServerIp = nullptr;
	MetricsOptions.GameSessionId = nullptr;

	const EOS_EResult Result = EOS_Metrics_BeginPlayerSession(MetricsHandle, &MetricsOptions);
	if (Result != EOS_EResult::EOS_Success)
	{
		FDebugLog::LogError(L"[EOS SDK] Metrics - Begin Player Session Failed! Result: %ls", FStringUtils::Widen(EOS_EResult_ToString(Result)).c_str());
	}
}

void FMetrics::EndPlayerSession(FEpicAccountId UserId)
{
	if (MetricsHandle == nullptr)
	{
		FDebugLog::LogError(L"MetricsHandle is invalid, can't end player session metrics!");
		return;
	}

	if (!UserId.IsValid())
	{
		FDebugLog::LogError(L"User Id invalid, can't end player session metrics!");
		return;
	}

	FDebugLog::Log(L"[EOS SDK] Metrics - End Player Session");

	// End Player Session Metrics
	EOS_Metrics_EndPlayerSessionOptions MetricsOptions = {};
	MetricsOptions.ApiVersion = EOS_METRICS_ENDPLAYERSESSION_API_LATEST;
	MetricsOptions.AccountIdType = EOS_EMetricsAccountIdType::EOS_MAIT_Epic;
	MetricsOptions.AccountId.Epic = UserId;

	const EOS_EResult Result = EOS_Metrics_EndPlayerSession(MetricsHandle, &MetricsOptions);
	if (Result != EOS_EResult::EOS_Success)
	{
		FDebugLog::LogError(L"[EOS SDK] Metrics - End Player Session Failed! Result: %ls", FStringUtils::Widen(EOS_EResult_ToString(Result)).c_str());
	}
}

void FMetrics::OnGameEvent(const FGameEvent& Event)
{
	if (Event.GetType() == EGameEventType::PlayerSessionBegin)
	{
		FEpicAccountId UserId = Event.GetUserId();
		BeginPlayerSession(UserId);
	}
	else if (Event.GetType() == EGameEventType::PlayerSessionEnd)
	{
		FEpicAccountId UserId = Event.GetUserId();
		EndPlayerSession(UserId);
	}
}