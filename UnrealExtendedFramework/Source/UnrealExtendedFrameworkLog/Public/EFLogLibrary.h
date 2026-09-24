// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EFLogTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "EFLogLibrary.generated.h"

/** Blueprint access to Extended Log: the same category files EF_LOG writes. */
UCLASS()
class UNREALEXTENDEDFRAMEWORKLOG_API UEFLogLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Writes one line into Saved/Logs/Extended/<Category>.log. The category is created by the first
	 * line that names it; an empty Category writes to "Blueprint". The line is labelled with the
	 * calling world's PIE instance (Server, Client 1, ...) and, as its source, the calling Blueprint.
	 * Nothing is written in Shipping.
	 */
	UFUNCTION(BlueprintCallable, Category = "Extended|Log", meta = (DisplayName = "EF Log", WorldContext = "WorldContextObject", CallableWithoutWorldContext, DevelopmentOnly, Keywords = "log print extended category"))
	static void WriteEFLog(const UObject* WorldContextObject, FName Category, const FString& Message, EEFLogVerbosity Verbosity = EEFLogVerbosity::Log);
};
