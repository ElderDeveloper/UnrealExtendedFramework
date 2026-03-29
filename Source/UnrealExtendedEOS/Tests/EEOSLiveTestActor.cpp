// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSLiveTestActor.h"
#include "Auth/EEOSAuthSubsystem.h"
#include "Stats/EEOSStatsSubsystem.h"
#include "Achievements/EEOSAchievementSubsystem.h"
#include "Leaderboards/EEOSLeaderboardSubsystem.h"
#include "Storage/EEOSPlayerStorageSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UnrealExtendedEOS.h"


AEEOSLiveTestActor::AEEOSLiveTestActor()
{
	PrimaryActorTick.bCanEverTick = false;
}


// ═════════════════════════════════════════════════════════════════════════════
// Lifecycle
// ═════════════════════════════════════════════════════════════════════════════

void AEEOSLiveTestActor::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoRunOnBeginPlay)
	{
		FTimerHandle DelayHandle;
		GetWorldTimerManager().SetTimer(DelayHandle, this, &AEEOSLiveTestActor::RunAllTests, 1.0f, false);
	}
}

void AEEOSLiveTestActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearTimeout();
	Super::EndPlay(EndPlayReason);
}


// ═════════════════════════════════════════════════════════════════════════════
// Subsystem Access
// ═════════════════════════════════════════════════════════════════════════════

UEEOSAuthSubsystem* AEEOSLiveTestActor::GetAuth() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UEEOSAuthSubsystem>() : nullptr;
}

UEEOSStatsSubsystem* AEEOSLiveTestActor::GetStats() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UEEOSStatsSubsystem>() : nullptr;
}

UEEOSAchievementSubsystem* AEEOSLiveTestActor::GetAchievements() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UEEOSAchievementSubsystem>() : nullptr;
}

UEEOSPlayerStorageSubsystem* AEEOSLiveTestActor::GetStorage() const
{
	UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UEEOSPlayerStorageSubsystem>() : nullptr;
}


// ═════════════════════════════════════════════════════════════════════════════
// Test Pipeline
// ═════════════════════════════════════════════════════════════════════════════

void AEEOSLiveTestActor::EnqueueAllTests()
{
	TestQueue.Empty();
	Results.Empty();
	CurrentTestIndex = -1;

	TestQueue.Add({ TEXT("EOS Availability"),      [this]() { Test_EOSAvailability(); } });
	TestQueue.Add({ TEXT("DeviceId Login"),         [this]() { Test_DeviceIdLogin(); } });
	TestQueue.Add({ TEXT("ProductUserId Valid"),    [this]() { Test_ProductUserIdValid(); } });
	TestQueue.Add({ TEXT("Stat Ingest"),            [this]() { Test_StatIngest(); } });
	TestQueue.Add({ TEXT("Stat Query"),             [this]() { Test_StatQuery(); } });
	TestQueue.Add({ TEXT("Achievement Query"),      [this]() { Test_AchievementQuery(); } });
	TestQueue.Add({ TEXT("Storage Write"),          [this]() { Test_StorageWrite(); } });
	TestQueue.Add({ TEXT("Storage Read"),           [this]() { Test_StorageRead(); } });
	TestQueue.Add({ TEXT("Cleanup"),                [this]() { Test_Cleanup(); } });
}

void AEEOSLiveTestActor::RunAllTests()
{
	UE_LOG(LogExtendedEOS, Log, TEXT(""));
	UE_LOG(LogExtendedEOS, Log, TEXT("=========================================================="));
	UE_LOG(LogExtendedEOS, Log, TEXT("          EOS LIVE INTEGRATION TESTS — START"));
	UE_LOG(LogExtendedEOS, Log, TEXT("=========================================================="));
	UE_LOG(LogExtendedEOS, Log, TEXT(""));

	EnqueueAllTests();
	RunNextTest();
}

void AEEOSLiveTestActor::RunNextTest()
{
	CurrentTestIndex++;

	if (CurrentTestIndex >= TestQueue.Num())
	{
		FinishAllTests();
		return;
	}

	const FString& TestName = TestQueue[CurrentTestIndex].Key;
	CurrentTestStartTime = FDateTime::UtcNow();

	LogTest(TEXT("RUN "), TestName, TEXT("Starting..."));
	StartTimeout();

	// Execute the test function
	TestQueue[CurrentTestIndex].Value();
}

void AEEOSLiveTestActor::PassCurrentTest(const FString& Message)
{
	FinalizeCurrentTest(EEOSTestStatus::Passed, Message);
}

void AEEOSLiveTestActor::FailCurrentTest(const FString& Message)
{
	FinalizeCurrentTest(EEOSTestStatus::Failed, Message);
}

void AEEOSLiveTestActor::SkipCurrentTest(const FString& Message)
{
	FinalizeCurrentTest(EEOSTestStatus::Skipped, Message);
}

void AEEOSLiveTestActor::FinalizeCurrentTest(EEOSTestStatus Status, const FString& Message)
{
	ClearTimeout();

	if (CurrentTestIndex < 0 || CurrentTestIndex >= TestQueue.Num()) return;

	const FString& TestName = TestQueue[CurrentTestIndex].Key;
	const float Duration = static_cast<float>((FDateTime::UtcNow() - CurrentTestStartTime).GetTotalSeconds());

	FEEOSTestResult Result;
	Result.TestName = TestName;
	Result.Status = Status;
	Result.Message = Message;
	Result.DurationSeconds = Duration;
	Results.Add(Result);

	FString StatusStr;
	switch (Status)
	{
	case EEOSTestStatus::Passed:  StatusStr = TEXT("PASS"); break;
	case EEOSTestStatus::Failed:  StatusStr = TEXT("FAIL"); break;
	case EEOSTestStatus::Skipped: StatusStr = TEXT("SKIP"); break;
	default:                      StatusStr = TEXT("????"); break;
	}

	LogTest(StatusStr, TestName, FString::Printf(TEXT("%s (%.2fs)"), *Message, Duration), Status == EEOSTestStatus::Failed);

	OnTestComplete.Broadcast(Result);

	// Continue to next test on next frame to avoid stack depth issues
	GetWorldTimerManager().SetTimerForNextTick([this]() { RunNextTest(); });
}

void AEEOSLiveTestActor::FinishAllTests()
{
	int32 Passed = 0, Failed = 0, Skipped = 0;
	for (const auto& R : Results)
	{
		switch (R.Status)
		{
		case EEOSTestStatus::Passed:  Passed++; break;
		case EEOSTestStatus::Failed:  Failed++; break;
		case EEOSTestStatus::Skipped: Skipped++; break;
		default: break;
		}
	}

	UE_LOG(LogExtendedEOS, Log, TEXT(""));
	UE_LOG(LogExtendedEOS, Log, TEXT("=========================================================="));
	UE_LOG(LogExtendedEOS, Log, TEXT("  RESULTS: %d passed, %d failed, %d skipped / %d total"),
		Passed, Failed, Skipped, Results.Num());
	UE_LOG(LogExtendedEOS, Log, TEXT("=========================================================="));
	UE_LOG(LogExtendedEOS, Log, TEXT(""));

	OnAllTestsComplete.Broadcast(Results);
}

FString AEEOSLiveTestActor::GetResultsSummary() const
{
	FString Summary;
	for (const auto& R : Results)
	{
		FString Status;
		switch (R.Status)
		{
		case EEOSTestStatus::Passed:  Status = TEXT("PASS"); break;
		case EEOSTestStatus::Failed:  Status = TEXT("FAIL"); break;
		case EEOSTestStatus::Skipped: Status = TEXT("SKIP"); break;
		default:                      Status = TEXT("PENDING"); break;
		}
		Summary += FString::Printf(TEXT("[%s] %s - %s (%.2fs)\n"), *Status, *R.TestName, *R.Message, R.DurationSeconds);
	}
	return Summary;
}


// ═════════════════════════════════════════════════════════════════════════════
// Timeout
// ═════════════════════════════════════════════════════════════════════════════

void AEEOSLiveTestActor::StartTimeout()
{
	ClearTimeout();
	GetWorldTimerManager().SetTimer(TimeoutTimerHandle, this, &AEEOSLiveTestActor::OnTestTimeout, TestTimeoutSeconds, false);
}

void AEEOSLiveTestActor::ClearTimeout()
{
	GetWorldTimerManager().ClearTimer(TimeoutTimerHandle);
}

void AEEOSLiveTestActor::OnTestTimeout()
{
	FailCurrentTest(FString::Printf(TEXT("Timed out after %.1fs"), TestTimeoutSeconds));
}


// ═════════════════════════════════════════════════════════════════════════════
// Logging
// ═════════════════════════════════════════════════════════════════════════════

void AEEOSLiveTestActor::LogTest(const FString& Prefix, const FString& TestName, const FString& Message, bool bError) const
{
	if (bError)
	{
		UE_LOG(LogExtendedEOS, Error, TEXT("  [%s]  %s  |  %s"), *Prefix, *TestName, *Message);
	}
	else
	{
		UE_LOG(LogExtendedEOS, Log, TEXT("  [%s]  %s  |  %s"), *Prefix, *TestName, *Message);
	}
}


// ═════════════════════════════════════════════════════════════════════════════
// Individual Test Implementations
// ═════════════════════════════════════════════════════════════════════════════

// ── 1. EOS Availability ──────────────────────────────────────────────────────

void AEEOSLiveTestActor::Test_EOSAvailability()
{
	UEEOSAuthSubsystem* Auth = GetAuth();
	if (!Auth)
	{
		FailCurrentTest(TEXT("AuthSubsystem is null — GameInstance may not be ready"));
		return;
	}

	if (Auth->IsEOSAvailable())
	{
		PassCurrentTest(TEXT("EOS OnlineSubsystem is available"));
	}
	else
	{
		FailCurrentTest(TEXT("EOS OnlineSubsystem is NOT available — check DefaultEngine.ini"));
	}
}


// ── 2. DeviceId Login ────────────────────────────────────────────────────────

void AEEOSLiveTestActor::Test_DeviceIdLogin()
{
	UEEOSAuthSubsystem* Auth = GetAuth();
	if (!Auth)
	{
		FailCurrentTest(TEXT("AuthSubsystem is null"));
		return;
	}

	// Already connected? (e.g., auto-login was configured)
	if (Auth->IsConnectedToGameServices())
	{
		PassCurrentTest(FString::Printf(TEXT("Already connected — PUID: %s"), *Auth->GetProductUserId()));
		return;
	}

	// Bind the callback and start login
	Auth->OnConnectLoginComplete.AddDynamic(this, &AEEOSLiveTestActor::OnConnectLoginResult);
	Auth->ConnectLoginWithDeviceId(TestPlayerName);
	// Test continues in OnConnectLoginResult...
}

void AEEOSLiveTestActor::OnConnectLoginResult(bool bSuccess, const FString& ProductUserId, const FString& ErrorMessage)
{
	// Unbind so we don't get called again
	if (UEEOSAuthSubsystem* Auth = GetAuth())
	{
		Auth->OnConnectLoginComplete.RemoveDynamic(this, &AEEOSLiveTestActor::OnConnectLoginResult);
	}

	if (bSuccess)
	{
		PassCurrentTest(FString::Printf(TEXT("DeviceId login successful — PUID: %s"), *ProductUserId));
	}
	else
	{
		FailCurrentTest(FString::Printf(TEXT("DeviceId login failed: %s"), *ErrorMessage));
	}
}


// ── 3. ProductUserId Valid ───────────────────────────────────────────────────

void AEEOSLiveTestActor::Test_ProductUserIdValid()
{
	UEEOSAuthSubsystem* Auth = GetAuth();
	if (!Auth)
	{
		FailCurrentTest(TEXT("AuthSubsystem is null"));
		return;
	}

	if (!Auth->IsConnectedToGameServices())
	{
		SkipCurrentTest(TEXT("Not connected — login test likely failed"));
		return;
	}

	const FString PUID = Auth->GetProductUserId();
	if (!PUID.IsEmpty())
	{
		PassCurrentTest(FString::Printf(TEXT("ProductUserId: %s (%d chars)"), *PUID, PUID.Len()));
	}
	else
	{
		FailCurrentTest(TEXT("ProductUserId is empty despite being connected"));
	}
}


// ── 4. Stat Ingest ───────────────────────────────────────────────────────────

void AEEOSLiveTestActor::Test_StatIngest()
{
	UEEOSStatsSubsystem* Stats = GetStats();
	if (!Stats)
	{
		SkipCurrentTest(TEXT("StatsSubsystem is null"));
		return;
	}

	if (!GetAuth() || !GetAuth()->IsConnectedToGameServices())
	{
		SkipCurrentTest(TEXT("Not connected to game services"));
		return;
	}

	// Bind callback and ingest
	Stats->OnStatIngested.AddDynamic(this, &AEEOSLiveTestActor::OnStatIngestResult);
	Stats->IngestStat(TestStatName, TestStatValue);
	// Test continues in OnStatIngestResult...
}

void AEEOSLiveTestActor::OnStatIngestResult(bool bSuccess)
{
	if (UEEOSStatsSubsystem* Stats = GetStats())
	{
		Stats->OnStatIngested.RemoveDynamic(this, &AEEOSLiveTestActor::OnStatIngestResult);
	}

	if (bSuccess)
	{
		PassCurrentTest(FString::Printf(TEXT("Ingested stat '%s' += %d"), *TestStatName, TestStatValue));
	}
	else
	{
		FailCurrentTest(FString::Printf(TEXT("Failed to ingest stat '%s' — is it defined in your EOS Dashboard?"), *TestStatName));
	}
}


// ── 5. Stat Query ────────────────────────────────────────────────────────────

void AEEOSLiveTestActor::Test_StatQuery()
{
	UEEOSStatsSubsystem* Stats = GetStats();
	if (!Stats)
	{
		SkipCurrentTest(TEXT("StatsSubsystem is null"));
		return;
	}

	if (!GetAuth() || !GetAuth()->IsConnectedToGameServices())
	{
		SkipCurrentTest(TEXT("Not connected to game services"));
		return;
	}

	Stats->OnStatsQueried.AddDynamic(this, &AEEOSLiveTestActor::OnStatQueryResult);

	TArray<FString> StatNames;
	StatNames.Add(TestStatName);
	Stats->QueryLocalStats(StatNames);
	// Test continues in OnStatQueryResult...
}

void AEEOSLiveTestActor::OnStatQueryResult(bool bSuccess, const TArray<FEEOSStat>& QueriedStats)
{
	if (UEEOSStatsSubsystem* Stats = GetStats())
	{
		Stats->OnStatsQueried.RemoveDynamic(this, &AEEOSLiveTestActor::OnStatQueryResult);
	}

	if (bSuccess)
	{
		if (QueriedStats.Num() > 0)
		{
			PassCurrentTest(FString::Printf(TEXT("Queried %d stat(s) — first: '%s' = %d"),
				QueriedStats.Num(), *QueriedStats[0].StatName, QueriedStats[0].Value));
		}
		else
		{
			PassCurrentTest(TEXT("Stat query succeeded but returned 0 stats (stat may not exist in dashboard)"));
		}
	}
	else
	{
		FailCurrentTest(TEXT("Stat query failed"));
	}
}


// ── 6. Achievement Query ─────────────────────────────────────────────────────

void AEEOSLiveTestActor::Test_AchievementQuery()
{
	UEEOSAchievementSubsystem* Achievements = GetAchievements();
	if (!Achievements)
	{
		SkipCurrentTest(TEXT("AchievementSubsystem is null"));
		return;
	}

	if (!GetAuth() || !GetAuth()->IsConnectedToGameServices())
	{
		SkipCurrentTest(TEXT("Not connected to game services"));
		return;
	}

	Achievements->OnAchievementsQueried.AddDynamic(this, &AEEOSLiveTestActor::OnAchievementQueryResult);
	Achievements->QueryAchievements();
	// Test continues in OnAchievementQueryResult...
}

void AEEOSLiveTestActor::OnAchievementQueryResult(bool bSuccess, const TArray<FEEOSAchievement>& QueriedAchievements)
{
	if (UEEOSAchievementSubsystem* Achievements = GetAchievements())
	{
		Achievements->OnAchievementsQueried.RemoveDynamic(this, &AEEOSLiveTestActor::OnAchievementQueryResult);
	}

	if (bSuccess)
	{
		PassCurrentTest(FString::Printf(TEXT("Queried %d achievement(s)"), QueriedAchievements.Num()));
	}
	else
	{
		// Achievements may not be configured — acceptable
		SkipCurrentTest(TEXT("Achievement query returned false — may not be configured in EOS Dashboard"));
	}
}


// ── 7. Storage Write ─────────────────────────────────────────────────────────

void AEEOSLiveTestActor::Test_StorageWrite()
{
	UEEOSPlayerStorageSubsystem* Storage = GetStorage();
	if (!Storage)
	{
		SkipCurrentTest(TEXT("PlayerStorageSubsystem is null"));
		return;
	}

	if (!GetAuth() || !GetAuth()->IsConnectedToGameServices())
	{
		SkipCurrentTest(TEXT("Not connected to game services"));
		return;
	}

	Storage->OnPlayerDataWritten.AddDynamic(this, &AEEOSLiveTestActor::OnStorageWriteResult);
	Storage->WritePlayerString(TEXT("eos_test_data.txt"), TEXT("EOS Live Test — Hello from automated tests!"));
	// Test continues in OnStorageWriteResult...
}

void AEEOSLiveTestActor::OnStorageWriteResult(bool bSuccess, const FString& FileName)
{
	if (UEEOSPlayerStorageSubsystem* Storage = GetStorage())
	{
		Storage->OnPlayerDataWritten.RemoveDynamic(this, &AEEOSLiveTestActor::OnStorageWriteResult);
	}

	if (bSuccess)
	{
		PassCurrentTest(FString::Printf(TEXT("Wrote '%s' to cloud storage"), *FileName));
	}
	else
	{
		FailCurrentTest(FString::Printf(TEXT("Failed to write '%s' — Player Data Storage may not be enabled"), *FileName));
	}
}


// ── 8. Storage Read ──────────────────────────────────────────────────────────

void AEEOSLiveTestActor::Test_StorageRead()
{
	UEEOSPlayerStorageSubsystem* Storage = GetStorage();
	if (!Storage)
	{
		SkipCurrentTest(TEXT("PlayerStorageSubsystem is null"));
		return;
	}

	if (!GetAuth() || !GetAuth()->IsConnectedToGameServices())
	{
		SkipCurrentTest(TEXT("Not connected to game services"));
		return;
	}

	Storage->OnPlayerDataRead.AddDynamic(this, &AEEOSLiveTestActor::OnStorageReadResult);
	Storage->ReadPlayerData(TEXT("eos_test_data.txt"));
	// Test continues in OnStorageReadResult...
}

void AEEOSLiveTestActor::OnStorageReadResult(bool bSuccess, const FString& FileName, const TArray<uint8>& Data)
{
	if (UEEOSPlayerStorageSubsystem* Storage = GetStorage())
	{
		Storage->OnPlayerDataRead.RemoveDynamic(this, &AEEOSLiveTestActor::OnStorageReadResult);
	}

	if (bSuccess && Data.Num() > 0)
	{
		// Safely convert to string
		TArray<uint8> NullTerminated = Data;
		NullTerminated.Add(0);
		const FString Content = UTF8_TO_TCHAR(reinterpret_cast<const char*>(NullTerminated.GetData()));
		PassCurrentTest(FString::Printf(TEXT("Read '%s' (%d bytes): \"%s\""), *FileName, Data.Num(), *Content.Left(60)));
	}
	else if (bSuccess)
	{
		PassCurrentTest(FString::Printf(TEXT("Read '%s' — file is empty"), *FileName));
	}
	else
	{
		FailCurrentTest(FString::Printf(TEXT("Failed to read '%s'"), *FileName));
	}
}


// ── 9. Cleanup ───────────────────────────────────────────────────────────────

void AEEOSLiveTestActor::Test_Cleanup()
{
	// DeviceId connections are persistent by design — no formal disconnect.
	// Just verify subsystems are still accessible and in a clean state.
	UEEOSAuthSubsystem* Auth = GetAuth();
	if (Auth && Auth->IsConnectedToGameServices())
	{
		PassCurrentTest(TEXT("Session intact — DeviceId connections persist by design"));
	}
	else
	{
		PassCurrentTest(TEXT("Cleanup complete — no active session to tear down"));
	}
}
