// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Shared/EEOSTypes.h"
#include "EEOSLiveTestActor.generated.h"

class UEEOSAuthSubsystem;
class UEEOSStatsSubsystem;
class UEEOSAchievementSubsystem;
class UEEOSLeaderboardSubsystem;
class UEEOSPlayerStorageSubsystem;

UENUM(BlueprintType)
enum class EEOSTestStatus : uint8
{
	Pending,
	Running,
	Passed,
	Failed,
	Skipped
};

USTRUCT(BlueprintType)
struct FEEOSTestResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Test")
	FString TestName;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Test")
	EEOSTestStatus Status = EEOSTestStatus::Pending;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Test")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "EOS|Test")
	float DurationSeconds = 0.f;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEOSTestComplete, const FEEOSTestResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAllEOSTestsComplete, const TArray<FEEOSTestResult>&, Results);

/**
 * Actor that runs live EOS integration tests when placed in a map.
 *
 * Usage:
 * 1. Place this actor in any test map
 * 2. Make sure EOS credentials are configured in Project Settings > Extended Framework > Extended EOS
 * 3. Press Play — tests run automatically (or call RunAllTests from Blueprint)
 * 4. Results are logged to Output Log and stored in the Results array
 *
 * Tests use DeviceId login (anonymous) — no Epic account required.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisplayName = "EOS Live Test Actor"))
class EXTENDEDEOSTESTING_API AEEOSLiveTestActor : public AActor
{
	GENERATED_BODY()

public:
	AEEOSLiveTestActor();

	// ── Configuration ────────────────────────────────────────────────────

	/** If true, tests start automatically on BeginPlay. Ignored in Shipping builds — the tests
	 *  perform live backend calls (logins, stat ingests, cloud writes) and must never auto-fire
	 *  from an actor accidentally left in a production map. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EOS|Test")
	bool bAutoRunOnBeginPlay = false;

	/** Timeout in seconds for each individual test step */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EOS|Test", meta = (ClampMin = 1.0, ClampMax = 60.0))
	float TestTimeoutSeconds = 15.0f;

	/** Display name for the test player (used in DeviceId login) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EOS|Test")
	FString TestPlayerName = TEXT("EOS_TestPlayer");

	/** Stat name to use for the write/read stat test (must exist in your EOS dashboard) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EOS|Test")
	FString TestStatName = TEXT("TestStat");

	/** Value to ingest for the stat test */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EOS|Test")
	int32 TestStatValue = 1;

	// ── Results ──────────────────────────────────────────────────────────

	/** All test results (populated as tests complete) */
	UPROPERTY(BlueprintReadOnly, Category = "EOS|Test")
	TArray<FEEOSTestResult> Results;

	/** Fired after each individual test completes */
	UPROPERTY(BlueprintAssignable, Category = "EOS|Test")
	FOnEOSTestComplete OnTestComplete;

	/** Fired when all tests are done */
	UPROPERTY(BlueprintAssignable, Category = "EOS|Test")
	FOnAllEOSTestsComplete OnAllTestsComplete;

	// ── Actions ──────────────────────────────────────────────────────────

	/** Run all tests sequentially */
	UFUNCTION(BlueprintCallable, Category = "EOS|Test")
	void RunAllTests();

	/** Get a summary string of all results */
	UFUNCTION(BlueprintPure, Category = "EOS|Test")
	FString GetResultsSummary() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	// ── Test Pipeline ────────────────────────────────────────────────────

	TArray<TPair<FString, TFunction<void()>>> TestQueue;
	int32 CurrentTestIndex = -1;
	FDateTime CurrentTestStartTime;
	FTimerHandle TimeoutTimerHandle;

	/** Handle of the next-tick advance scheduled by FinalizeCurrentTest, so EndPlay can clear
	 *  a pending advance. The delegate is UObject-bound (CreateUObject), so even an uncleared
	 *  timer cannot fire into a destroyed actor. */
	FTimerHandle NextTestTimerHandle;

	/** True from RunAllTests until FinishAllTests. A run is a chain of async steps (tests in
	 *  flight, timeout timers, next-tick advances) — re-entering RunAllTests mid-run would
	 *  double-drive the queue, so re-entry is logged and ignored while this is set. */
	bool bRunInProgress = false;

	/** Monotonic id of the running test. Incremented on every test start AND on timeout, so a
	 *  late async completion from a timed-out test can be recognized as stale and ignored
	 *  instead of finalizing the WRONG test and double-advancing the queue. */
	int32 CurrentTestId = 0;

	/** Value of CurrentTestId at the moment the active test bound its completion delegate.
	 *  Every completion handler no-ops when this no longer matches CurrentTestId. */
	int32 BoundTestId = INDEX_NONE;

	/** Unbinds whatever delegate(s) the active test bound. Each async test sets this right
	 *  after binding; it is invoked exactly once from FinalizeCurrentTest, which covers both
	 *  the completion and the timeout path (timeout finalizes via FailCurrentTest). */
	TFunction<void()> ActiveTestCleanup;

	void EnqueueAllTests();
	void RunNextTest();
	void PassCurrentTest(const FString& Message = TEXT(""));
	void FailCurrentTest(const FString& Message);
	void SkipCurrentTest(const FString& Message);
	void FinalizeCurrentTest(EEOSTestStatus Status, const FString& Message);
	void FinishAllTests();
	void RunActiveTestCleanup();

	void StartTimeout();
	void ClearTimeout();
	void OnTestTimeout();

	// ── Subsystem Access ─────────────────────────────────────────────────

	UEEOSAuthSubsystem* GetAuth() const;
	UEEOSStatsSubsystem* GetStats() const;
	UEEOSAchievementSubsystem* GetAchievements() const;
	UEEOSPlayerStorageSubsystem* GetStorage() const;

	// ── Individual Tests ─────────────────────────────────────────────────

	void Test_EOSAvailability();
	void Test_DeviceIdLogin();
	void Test_ProductUserIdValid();
	void Test_StatIngest();
	void Test_StatQuery();
	void Test_AchievementQuery();
	void Test_StorageWrite();
	void Test_StorageRead();
	void Test_Cleanup();

	// ── Delegate Callbacks (UFUNCTION required for dynamic delegates) ────

	UFUNCTION()
	void OnConnectLoginResult(bool bSuccess, const FString& ProductUserId, const FString& ErrorMessage);

	UFUNCTION()
	void OnStatIngestResult(bool bSuccess);

	UFUNCTION()
	void OnStatQueryResult(bool bSuccess, const TArray<FEEOSStat>& Stats);

	UFUNCTION()
	void OnAchievementQueryResult(bool bSuccess, const TArray<FEEOSAchievement>& Achievements);

	UFUNCTION()
	void OnStorageWriteResult(bool bSuccess, const FString& FileName);

	UFUNCTION()
	void OnStorageReadResult(bool bSuccess, const FString& FileName, const TArray<uint8>& Data);

	// ── Logging ──────────────────────────────────────────────────────────

	void LogTest(const FString& Prefix, const FString& TestName, const FString& Message, bool bError = false) const;
};
