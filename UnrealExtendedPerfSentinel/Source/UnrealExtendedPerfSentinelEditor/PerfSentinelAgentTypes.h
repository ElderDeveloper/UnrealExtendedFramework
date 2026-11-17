// Copyright Kemal Erdem YILMAZ. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "PerfSentinelAgentTypes.generated.h"

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
