// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFConnectionMonitor.h"
#include "UnrealExtendedPlayFab.h"
#include "Dom/JsonObject.h"
#include "TimerManager.h"
#include "Engine/GameInstance.h"

void UEPFConnectionMonitor::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bIsConnected = true;
	bIsMonitoring = false;
	ConsecutiveFailures = 0;
}

void UEPFConnectionMonitor::Deinitialize()
{
	StopMonitoring();
	Super::Deinitialize();
}

void UEPFConnectionMonitor::StartMonitoring(float IntervalSeconds)
{
	if (bIsMonitoring) return;

	const float Interval = FMath::Max(5.0f, IntervalSeconds);
	bIsMonitoring = true;

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UWorld* World = GI->GetWorld())
		{
			World->GetTimerManager().SetTimer(
				PingTimerHandle,
				this,
				&UEPFConnectionMonitor::PerformPing,
				Interval,
				true,  // looping
				0.0f   // first ping immediately
			);
		}
	}

	UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFConnectionMonitor — Started monitoring (interval: %.0fs)"), Interval);
}

void UEPFConnectionMonitor::StopMonitoring()
{
	if (!bIsMonitoring) return;

	bIsMonitoring = false;

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UWorld* World = GI->GetWorld())
		{
			World->GetTimerManager().ClearTimer(PingTimerHandle);
		}
	}

	UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFConnectionMonitor — Stopped monitoring"));
}

void UEPFConnectionMonitor::Ping()
{
	PerformPing();
}

bool UEPFConnectionMonitor::IsMonitoring() const
{
	return bIsMonitoring;
}

bool UEPFConnectionMonitor::IsConnected() const
{
	return bIsConnected;
}

float UEPFConnectionMonitor::GetLastPingTime() const
{
	return LastPingTime;
}

int32 UEPFConnectionMonitor::GetConsecutiveFailures() const
{
	return ConsecutiveFailures;
}

void UEPFConnectionMonitor::PerformPing()
{
	if (!IsConfigured()) return;

	// Use GetTime as a lightweight ping — minimal server load
	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();

	SendPlayFabRequest(
		TEXT("/Client/GetTime"),
		Body,
		false, // GetTime does not require auth
		FOnPlayFabResponse::CreateUObject(this, &UEPFConnectionMonitor::HandlePingResponse)
	);
}

void UEPFConnectionMonitor::HandlePingResponse(bool bSuccess, TSharedPtr<FJsonObject> Response)
{
	if (bSuccess)
	{
		ConsecutiveFailures = 0;

		if (UGameInstance* GI = GetGameInstance())
		{
			if (UWorld* World = GI->GetWorld())
			{
				LastPingTime = World->GetTimeSeconds();
			}
		}

		if (!bIsConnected)
		{
			bIsConnected = true;
			UE_LOG(LogExtendedPlayFab, Log, TEXT("EPFConnectionMonitor — Connection RESTORED"));
			OnConnectionRestored.Broadcast();
		}
	}
	else
	{
		ConsecutiveFailures++;

		if (bIsConnected && ConsecutiveFailures >= FailureThreshold)
		{
			bIsConnected = false;
			UE_LOG(LogExtendedPlayFab, Warning, TEXT("EPFConnectionMonitor — Connection LOST (%d consecutive failures)"), ConsecutiveFailures);
			OnConnectionLost.Broadcast();
		}
		else if (bIsConnected)
		{
			UE_LOG(LogExtendedPlayFab, Verbose, TEXT("EPFConnectionMonitor — Ping failed (%d/%d before declaring lost)"), ConsecutiveFailures, FailureThreshold);
		}
	}
}
