// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "TitleNews/EPFTitleNewsSubsystem.h"
#include "EPFAsyncTitleNews.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncTitleNewsReceived, const TArray<FEPFTitleNewsItem>&, NewsItems);

UCLASS(meta = (DisplayName = "PlayFab: Get Title News"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGetTitleNews : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get Title News"), Category = "PlayFab|Async|TitleNews")
	static UEPFAsyncGetTitleNews* GetTitleNews(UObject* WorldContext, int32 Count = 10);

	UPROPERTY(BlueprintAssignable) FOnEPFAsyncTitleNewsReceived OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;

private:
	int32 Count;
	TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const TArray<FEPFTitleNewsItem>& NewsItems);
};
