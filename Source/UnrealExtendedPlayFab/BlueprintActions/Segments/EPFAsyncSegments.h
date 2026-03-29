// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "Segments/EPFSegmentsSubsystem.h"
#include "EPFAsyncSegments.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncSegmentsSuccess, const TArray<FEPFPlayerSegment>&, Segments);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncTagsSuccess, const TArray<FString>&, Tags);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEPFAsyncTagModifiedSuccess);

// ── Get Segments ─────────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Get Player Segments"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGetSegments : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get Player Segments"), Category = "PlayFab|Async|Segments")
	static UEPFAsyncGetSegments* GetPlayerSegments(UObject* WorldContext);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncSegmentsSuccess OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;
private:
	TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const TArray<FEPFPlayerSegment>& Segments);
};

// ── Get Tags ─────────────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Get Player Tags"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGetPlayerTags : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get Player Tags"), Category = "PlayFab|Async|Segments")
	static UEPFAsyncGetPlayerTags* GetPlayerTags(UObject* WorldContext, const FString& Namespace = TEXT(""));
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncTagsSuccess OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;
private:
	FString Namespace; TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const TArray<FString>& Tags);
};

// ── Add Tag ──────────────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Add Player Tag"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncAddPlayerTag : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Add Player Tag"), Category = "PlayFab|Async|Segments")
	static UEPFAsyncAddPlayerTag* AddPlayerTag(UObject* WorldContext, const FString& TagName, const FString& Namespace = TEXT(""));
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncTagModifiedSuccess OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;
private:
	FString TagName; FString Namespace; TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};

// ── Remove Tag ───────────────────────────────────────────────────────────────

UCLASS(meta = (DisplayName = "PlayFab: Remove Player Tag"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncRemovePlayerTag : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Remove Player Tag"), Category = "PlayFab|Async|Segments")
	static UEPFAsyncRemovePlayerTag* RemovePlayerTag(UObject* WorldContext, const FString& TagName, const FString& Namespace = TEXT(""));
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncTagModifiedSuccess OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;
private:
	FString TagName; FString Namespace; TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};
