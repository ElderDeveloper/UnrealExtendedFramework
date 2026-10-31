// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/EPFAsyncTypes.h"
#include "EntityFiles/EPFEntityFilesSubsystem.h"
#include "EPFAsyncEntityFiles.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEPFAsyncEntityObjSuccess, const TArray<FEPFEntityObject>&, Objects);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEPFAsyncEntityObjUpdated);

// ── Get Objects ──────────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Get Entity Objects"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncGetEntityObjects : public UEPFAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Get Entity Objects"), Category = "PlayFab|Async|EntityFiles")
	static UEPFAsyncGetEntityObjects* GetObjects(UObject* WorldContext, const FString& EntityId, const FString& EntityType = TEXT("title_player_account"));
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncEntityObjSuccess OnSuccess;
protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
private:
	FString TargetEntityId; FString TargetEntityType;
	UFUNCTION() void HandleComplete(const FEPFResult& Result, const TArray<FEPFEntityObject>& Objects);
};

// ── Set Objects ──────────────────────────────────────────────────────────────
UCLASS(meta = (DisplayName = "PlayFab: Set Entity Objects"))
class UNREALEXTENDEDPLAYFAB_API UEPFAsyncSetEntityObjects : public UEPFAsyncActionBase
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "PlayFab: Set Entity Objects"), Category = "PlayFab|Async|EntityFiles")
	static UEPFAsyncSetEntityObjects* SetObjects(UObject* WorldContext, const FString& EntityId, const TMap<FString, FString>& Objects, const FString& EntityType = TEXT("title_player_account"));
	UPROPERTY(BlueprintAssignable) FOnEPFAsyncEntityObjUpdated OnSuccess;
protected:
	virtual void ExecuteAction() override;
	virtual void CancelWork() override;
private:
	FString TargetEntityId; FString TargetEntityType; TMap<FString, FString> ObjectsToSet;
	UFUNCTION() void HandleComplete(const FEPFResult& Result);
};
