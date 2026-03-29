// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "SharedData/EPFSharedDataSubsystem.h"
#include "EPFAsyncSharedData.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncSharedGroupCreated, const FString&, SharedGroupId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEPFAsyncSharedDataSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEPFAsyncSharedDataUpdated);

// ── Create Shared Group ──────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Create Shared Group"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncCreateSharedGroup : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Create Shared Group"), Category = "PlayFab|Async|SharedData")
	static UEPFAsyncCreateSharedGroup* CreateSharedGroup(UObject* WorldContext, const FString& SharedGroupId);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncSharedGroupCreated OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;
private:
	FString GroupId; TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const FString& Id);
};

// ── Get Shared Data ──────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Get Shared Group Data"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGetSharedData : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get Shared Group Data"), Category = "PlayFab|Async|SharedData")
	static UEPFAsyncGetSharedData* GetSharedGroupData(UObject* WorldContext, const FString& SharedGroupId, const TArray<FString>& Keys);
	/** Fires on success — call GetCachedGroupData() on the SharedData subsystem to access data */
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncSharedDataSuccess OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;
private:
	FString GroupId; TArray<FString> Keys; TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};

// ── Update Shared Data ───────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Update Shared Group Data"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncUpdateSharedData : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Update Shared Group Data"), Category = "PlayFab|Async|SharedData")
	static UEPFAsyncUpdateSharedData* UpdateSharedGroupData(UObject* WorldContext, const FString& SharedGroupId, const TMap<FString, FString>& Data);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncSharedDataUpdated OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;
private:
	FString GroupId; TMap<FString, FString> Data; TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};

// ── Add Members ──────────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Add Shared Group Members"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncAddSharedMembers : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Add Shared Group Members"), Category = "PlayFab|Async|SharedData")
	static UEPFAsyncAddSharedMembers* AddMembers(UObject* WorldContext, const FString& SharedGroupId, const TArray<FString>& PlayFabIds);
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncSharedDataUpdated OnSuccess;
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncFailed OnFailure;
	virtual void Activate() override;
private:
	FString GroupId; TArray<FString> Ids; TWeakObjectPtr<UObject> WorldContext;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};
