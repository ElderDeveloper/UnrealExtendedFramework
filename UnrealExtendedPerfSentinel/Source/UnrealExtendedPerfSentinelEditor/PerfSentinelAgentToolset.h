// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/AgentSkill.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "PerfSentinelAgentToolset.generated.h"

/** Versioned async envelope. Parse ResultJson only after State becomes completed. */
USTRUCT(BlueprintType)
struct FPerfSentinelAgentResponse
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="PerfSentinel Agent") int32 SchemaVersion = 1;
	UPROPERTY(BlueprintReadOnly, Category="PerfSentinel Agent") FString RequestId;
	UPROPERTY(BlueprintReadOnly, Category="PerfSentinel Agent") FString Operation;
	UPROPERTY(BlueprintReadOnly, Category="PerfSentinel Agent") FString State = TEXT("failed");
	UPROPERTY(BlueprintReadOnly, Category="PerfSentinel Agent") bool bSuccess = false;
	UPROPERTY(BlueprintReadOnly, Category="PerfSentinel Agent") FString Error;
	UPROPERTY(BlueprintReadOnly, Category="PerfSentinel Agent") FString ResultJson;
};

/** Allowlisted filters; empty strings mean no filter. Times require an explicit Timebase. */
USTRUCT(BlueprintType)
struct FPerfSentinelAgentFilter
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PerfSentinel Agent") FString Domain;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PerfSentinel Agent") FString MetricId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PerfSentinel Agent") FString ScopeIdentity;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PerfSentinel Agent") FString Status;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PerfSentinel Agent") FString Table;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PerfSentinel Agent") FString SourceFile;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PerfSentinel Agent") FString AssetPath;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PerfSentinel Agent") FString InstanceId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PerfSentinel Agent") FString OccurrenceId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PerfSentinel Agent") FString Timebase;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PerfSentinel Agent") FString ExpectedGenerationId;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PerfSentinel Agent") double StartSeconds = -1.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PerfSentinel Agent") double EndSeconds = -1.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PerfSentinel Agent") int32 Offset = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PerfSentinel Agent") int32 Limit = 100;
};

/** Read Perf Sentinel evidence first when investigating game performance or network costs.
 * Report queries return queued IDs immediately; use GetQueryStatus in subsequent calls.
 * SessionId is the report-directory key returned by ListSessions, not an arbitrary path.
 */
UCLASS()
class UPerfSentinelAgentToolset : public UToolsetDefinition
{
	GENERATED_BODY()
public:
	virtual FString GetToolsetVersion() const override { return TEXT("1.0.0"); }
	/** List report-directory session keys and compact context. Maximum page size is 200. */
	UFUNCTION(meta=(AICallable), Category="PerfSentinel Agent")
	static FPerfSentinelAgentResponse ListSessions(const FString& NameFilter = TEXT(""), int32 Offset = 0, int32 Limit = 50);
	/** Read compact overview, evidence levels, and highest-impact findings. */
	UFUNCTION(meta=(AICallable), Category="PerfSentinel Agent")
	static FPerfSentinelAgentResponse GetOverview(const FString& SessionId);
	/** Read collector/domain coverage; missing data is distinct from a measured zero. */
	UFUNCTION(meta=(AICallable), Category="PerfSentinel Agent")
	static FPerfSentinelAgentResponse GetCoverage(const FString& SessionId);
	/** Query typed metric records with bounded allowlisted filters. No arbitrary SQL. */
	UFUNCTION(meta=(AICallable), Category="PerfSentinel Agent")
	static FPerfSentinelAgentResponse QueryMetrics(const FString& SessionId, const FPerfSentinelAgentFilter& Filter);
	/** Resolve an exact stable evidence ID or page filtered evidence rows. */
	UFUNCTION(meta=(AICallable), Category="PerfSentinel Agent")
	static FPerfSentinelAgentResponse GetEvidence(const FString& SessionId, const FPerfSentinelAgentFilter& Filter, const FString& EvidenceId = TEXT(""));
	/** Compare comparable measured metrics; incompatible context and unknown baselines stay explicit. */
	UFUNCTION(meta=(AICallable), Category="PerfSentinel Agent")
	static FPerfSentinelAgentResponse CompareRuns(const FString& SessionId, const FString& BaselineSessionId, const FString& Domain = TEXT(""), int32 Offset = 0, int32 Limit = 100, const FString& MetricId = TEXT(""));
	/** Recommend the smallest capture that closes current evidence gaps; does not start a capture. */
	UFUNCTION(meta=(AICallable), Category="PerfSentinel Agent")
	static FPerfSentinelAgentResponse RecommendCapture(const FString& SessionId, const FString& Question = TEXT(""));
	/** Correlate occurrence stages across supplied run IDs; unsynchronized clocks never yield cross-process durations. */
	UFUNCTION(meta=(AICallable), Category="PerfSentinel Agent")
	static FPerfSentinelAgentResponse QueryRunTimeline(const FString& RunId, const FString& OccurrenceId = TEXT(""), int32 Offset = 0, int32 Limit = 100);
	/** Immediate read-only status from the active runtime capture controller. */
	UFUNCTION(meta=(AICallable), Category="PerfSentinel Agent")
	static FPerfSentinelAgentResponse GetCaptureStatus();
	/** Immediate read-only status from the active analysis manager. */
	UFUNCTION(meta=(AICallable), Category="PerfSentinel Agent")
	static FPerfSentinelAgentResponse GetAnalysisStatus();
	/** Set run/role/build context for the next capture while idle; does not persist project settings. */
	UFUNCTION(meta=(AICallable), Category="PerfSentinel Agent")
	static FPerfSentinelAgentResponse ConfigureCaptureContext(const FString& RunId, const FString& ProcessRole, const FString& BuildId);
	/** Start only this plugin's capture controller; does not take over an external trace or relaunch. */
	UFUNCTION(meta=(AICallable), Category="PerfSentinel Agent")
	static FPerfSentinelAgentResponse StartCapture(const FString& ScenarioName = TEXT("AgentCapture"));
	/** Stop only the capture owned by this plugin; no external recorder is stopped. */
	UFUNCTION(meta=(AICallable), Category="PerfSentinel Agent")
	static FPerfSentinelAgentResponse StopCapture();
	/** Queue asynchronous analysis of the last completed owned capture; no arbitrary trace path. */
	UFUNCTION(meta=(AICallable), Category="PerfSentinel Agent")
	static FPerfSentinelAgentResponse AnalyzeLastCapture();
	/** Poll a bounded query job. Terminal states are completed, failed, cancelled, or expired/not-found. */
	UFUNCTION(meta=(AICallable), Category="PerfSentinel Agent")
	static FPerfSentinelAgentResponse GetQueryStatus(const FString& RequestId);
	/** Cancel only this evidence query job, preserving capture and analysis jobs. */
	UFUNCTION(meta=(AICallable), Category="PerfSentinel Agent")
	static FPerfSentinelAgentResponse CancelQuery(const FString& RequestId);
};

/** Native source skill; discoverable through AgentSkillToolset without a Blueprint asset. */
UCLASS()
class UPerfSentinelInvestigationSkill : public UAgentSkill
{
	GENERATED_BODY()
public:
	UPerfSentinelInvestigationSkill();
};
