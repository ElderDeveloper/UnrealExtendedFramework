// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "EPFAsyncReporting.generated.h"


UCLASS(meta = (DisplayName = "PlayFab: Report Player"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncReportPlayer : public UEPFAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Report Player"), Category = "PlayFab|Async|Reporting")
	static UEPFAsyncReportPlayer* ReportPlayer(UObject* WorldContext, const FString& ReporteePlayFabId, const FString& Comment = TEXT(""));

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncSimpleSuccess OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	FString ReporteeId, ReportComment;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};
