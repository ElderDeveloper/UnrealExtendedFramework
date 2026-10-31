// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "EPFAsyncTitleData.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEPFAsyncTitleDataSuccess);

UCLASS(meta = (DisplayName = "PlayFab: Get Title Data"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGetTitleData : public UEPFAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get Title Data"), Category = "PlayFab|Async|TitleData")
	static UEPFAsyncGetTitleData* GetTitleData(UObject* WorldContext, const TArray<FString>& Keys);

	/** Fires on success — call GetCachedValue() / GetAllCachedData() on the TitleData subsystem to read data */
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncTitleDataSuccess OnSuccess;

protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;

private:
	TArray<FString> RequestedKeys;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};
