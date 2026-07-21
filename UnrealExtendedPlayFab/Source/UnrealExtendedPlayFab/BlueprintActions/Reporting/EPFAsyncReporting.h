// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "EPFAsyncReporting.generated.h"


UCLASS(meta = (DisplayName = "PlayFab: Report Player"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncReportPlayer : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Report Player"), Category = "PlayFab|Async|Reporting")
	static UEPFAsyncReportPlayer* ReportPlayer(UObject* WorldContext, const FString& ReporteePlayFabId, const FString& Comment = TEXT(""));

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncSimpleSuccess OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;

	virtual void Activate() override;

private:
	FString ReporteeId, Comment;
	TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};
