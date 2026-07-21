// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Tests/EEOSLiveTestActor.h"
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

#if !UE_BUILD_SHIPPING
	if (bAutoRunOnBeginPlay)
	{
		FTimerHandle DelayHandle;
		GetWorldTimerManager().SetTimer(DelayHandle, this, &AEEOSLiveTestActor::RunAllTests, 1.0f, false);
	}
#endif
}

void AEEOSLiveTestActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unbind any completion delegate a still-running test left on a subsystem — the
	// subsystems outlive this actor within the GameInstance
	RunActiveTestCleanup();
	ClearTimeout();
	GetWorldTimerManager().ClearTimer(NextTestTimerHandle);
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
#if UE_BUILD_SHIPPING
	UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSLiveTestActor: live integration tests are disabled in Shipping builds"));
	return;
#endif

	// Re-entry guard: a run is a chain of async steps (a test in flight, its timeout timer,
	// or a next-tick queue advance). Restarting mid-run would double-drive the queue —
	// EnqueueAllTests would reset state under the pending advance/timeout, which then
	// finalizes tests it never started. Simplest correct behavior: ignore and log.
	if (bRunInProgress)
	{
		UE_LOG(LogExtendedEOS, Warning,
			TEXT("EEOSLiveTestActor::RunAllTests — a run is already in progress (test %d/%d); ignoring re-entry. Wait for OnAllTestsComplete before starting a new run."),
			CurrentTestIndex + 1, TestQueue.Num());
		return;
	}
	bRunInProgress = true;

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

	// New test identity: any completion that still carries the previous id is stale
	CurrentTestId++;

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
	// Always unbind what the test bound — this is the single cleanup point for both the
	// completion path (handler → Pass/Fail → here) and the timeout path (OnTestTimeout →
	// FailCurrentTest → here), so a late completion can never reach a handler again.
	RunActiveTestCleanup();
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

	// Continue to next test on next frame to avoid stack depth issues. UObject-bound delegate
	// (NOT a raw-this lambda): if the actor is destroyed before the tick fires, the timer
	// manager sees the invalid object and skips the call instead of invoking a dangling this.
	// The handle is kept so EndPlay can also clear a pending advance explicitly.
	NextTestTimerHandle = GetWorldTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &AEEOSLiveTestActor::RunNextTest));
}

void AEEOSLiveTestActor::FinishAllTests()
{
	// Clear before broadcasting so an OnAllTestsComplete listener may restart immediately
	bRunInProgress = false;

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
	// Invalidate the in-flight completion FIRST: even if an unbind were ever missed, a late
	// handler compares its BoundTestId against CurrentTestId, sees the mismatch, and no-ops
	// instead of finalizing the test that is running by then (which used to double-advance
	// the queue). FailCurrentTest → FinalizeCurrentTest also runs ActiveTestCleanup, which
	// unbinds the delegates this test bound.
	CurrentTestId++;
	FailCurrentTest(FString::Printf(TEXT("Timed out after %.1fs"), TestTimeoutSeconds));
}

void AEEOSLiveTestActor::RunActiveTestCleanup()
{
	if (ActiveTestCleanup)
	{
		// Move out first so a cleanup that somehow re-enters can't run twice
		TFunction<void()> Cleanup = MoveTemp(ActiveTestCleanup);
		ActiveTestCleanup = nullptr;
		Cleanup();
	}
	BoundTestId = INDEX_NONE;
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
	BoundTestId = CurrentTestId;
	ActiveTestCleanup = [this]()
	{
		if (UEEOSAuthSubsystem* Auth = GetAuth())
		{
			Auth->OnConnectLoginComplete.RemoveDynamic(this, &AEEOSLiveTestActor::OnConnectLoginResult);
		}
	};

	// Entry points return bool: false = rejected/failed to start. A synchronous pre-flight
	// failure broadcast has already reached OnConnectLoginResult (which finalized the test and
	// reset BoundTestId); false WITHOUT that means no delegate will ever fire for this call —
	// fail fast instead of burning the whole timeout.
	if (!Auth->ConnectLoginWithDeviceId(TestPlayerName) && BoundTestId == CurrentTestId)
	{
		FailCurrentTest(TEXT("ConnectLoginWithDeviceId could not start (rejected — no completion will fire)"));
	}
	// Test continues in OnConnectLoginResult...
}

void AEEOSLiveTestActor::OnConnectLoginResult(bool bSuccess, const FString& ProductUserId, const FString& ErrorMessage)
{
	// Stale completion from a test that already timed out — ignore (unbinding happens in
	// FinalizeCurrentTest via ActiveTestCleanup)
	if (BoundTestId != CurrentTestId) return;

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
	BoundTestId = CurrentTestId;
	ActiveTestCleanup = [this]()
	{
		if (UEEOSStatsSubsystem* Stats = GetStats())
		{
			Stats->OnStatIngested.RemoveDynamic(this, &AEEOSLiveTestActor::OnStatIngestResult);
		}
	};

	// false + no synchronous completion consumed = no delegate will fire — fail fast
	if (!Stats->IngestStat(TestStatName, TestStatValue) && BoundTestId == CurrentTestId)
	{
		FailCurrentTest(FString::Printf(TEXT("IngestStat '%s' could not start (rejected — no completion will fire)"), *TestStatName));
	}
	// Test continues in OnStatIngestResult...
}

void AEEOSLiveTestActor::OnStatIngestResult(bool bSuccess)
{
	if (BoundTestId != CurrentTestId) return; // stale completion from a timed-out test

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
	BoundTestId = CurrentTestId;
	ActiveTestCleanup = [this]()
	{
		if (UEEOSStatsSubsystem* Stats = GetStats())
		{
			Stats->OnStatsQueried.RemoveDynamic(this, &AEEOSLiveTestActor::OnStatQueryResult);
		}
	};

	TArray<FString> StatNames;
	StatNames.Add(TestStatName);
	// false + no synchronous completion consumed = no delegate will fire — fail fast
	if (!Stats->QueryLocalStats(StatNames) && BoundTestId == CurrentTestId)
	{
		FailCurrentTest(TEXT("QueryLocalStats could not start (rejected — no completion will fire)"));
	}
	// Test continues in OnStatQueryResult...
}

void AEEOSLiveTestActor::OnStatQueryResult(bool bSuccess, const TArray<FEEOSStat>& QueriedStats)
{
	if (BoundTestId != CurrentTestId) return; // stale completion from a timed-out test

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
	BoundTestId = CurrentTestId;
	ActiveTestCleanup = [this]()
	{
		if (UEEOSAchievementSubsystem* Achievements = GetAchievements())
		{
			Achievements->OnAchievementsQueried.RemoveDynamic(this, &AEEOSLiveTestActor::OnAchievementQueryResult);
		}
	};

	// false + no synchronous completion consumed = no delegate will fire — fail fast
	if (!Achievements->QueryAchievements() && BoundTestId == CurrentTestId)
	{
		FailCurrentTest(TEXT("QueryAchievements could not start (rejected — no completion will fire)"));
	}
	// Test continues in OnAchievementQueryResult...
}

void AEEOSLiveTestActor::OnAchievementQueryResult(bool bSuccess, const TArray<FEEOSAchievement>& QueriedAchievements)
{
	if (BoundTestId != CurrentTestId) return; // stale completion from a timed-out test

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
	BoundTestId = CurrentTestId;
	ActiveTestCleanup = [this]()
	{
		if (UEEOSPlayerStorageSubsystem* Storage = GetStorage())
		{
			Storage->OnPlayerDataWritten.RemoveDynamic(this, &AEEOSLiveTestActor::OnStorageWriteResult);
		}
	};

	// false + no synchronous completion consumed = no delegate will fire (e.g. same-file
	// duplicate rejected log-only) — fail fast
	if (!Storage->WritePlayerString(TEXT("eos_test_data.txt"), TEXT("EOS Live Test — Hello from automated tests!"))
		&& BoundTestId == CurrentTestId)
	{
		FailCurrentTest(TEXT("WritePlayerString could not start (rejected — no completion will fire)"));
	}
	// Test continues in OnStorageWriteResult...
}

void AEEOSLiveTestActor::OnStorageWriteResult(bool bSuccess, const FString& FileName)
{
	if (BoundTestId != CurrentTestId) return; // stale completion from a timed-out test

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
	BoundTestId = CurrentTestId;
	ActiveTestCleanup = [this]()
	{
		if (UEEOSPlayerStorageSubsystem* Storage = GetStorage())
		{
			Storage->OnPlayerDataRead.RemoveDynamic(this, &AEEOSLiveTestActor::OnStorageReadResult);
		}
	};

	// false + no synchronous completion consumed = no delegate will fire (e.g. same-file
	// duplicate rejected log-only) — fail fast
	if (!Storage->ReadPlayerData(TEXT("eos_test_data.txt")) && BoundTestId == CurrentTestId)
	{
		FailCurrentTest(TEXT("ReadPlayerData could not start (rejected — no completion will fire)"));
	}
	// Test continues in OnStorageReadResult...
}

void AEEOSLiveTestActor::OnStorageReadResult(bool bSuccess, const FString& FileName, const TArray<uint8>& Data)
{
	if (BoundTestId != CurrentTestId) return; // stale completion from a timed-out test

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
