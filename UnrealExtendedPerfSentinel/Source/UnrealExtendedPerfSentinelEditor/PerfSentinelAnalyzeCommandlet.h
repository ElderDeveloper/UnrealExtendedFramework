// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "PerfSentinelAnalyzeCommandlet.generated.h"

/** Headless UE 5.8 TraceServices extractor used by the asynchronous PerfSentinel analysis pipeline. */
UCLASS()
class UPerfSentinelAnalyzeCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UPerfSentinelAnalyzeCommandlet();
	virtual int32 Main(const FString& Params) override;
};
