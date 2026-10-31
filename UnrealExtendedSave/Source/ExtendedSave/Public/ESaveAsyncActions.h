// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/EFAsyncActionBase.h"
#include "ESaveTypes.h"
#include "ESaveAsyncActions.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FESaveSucceededPin);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FESaveLoadedPin, bool, bRestoredFromBackup);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FESaveCloudSyncedPin, bool, bDownloaded);

/** Base for the save nodes: failure pin, result source. */
UCLASS(Abstract)
class EXTENDEDSAVE_API UESaveAsyncActionBase : public UEFAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FEFAsyncFailurePin OnFailure;

protected:
	virtual void BroadcastFailure(const FEFResult& Result) override { OnFailure.Broadcast(Result); }
	virtual FName GetResultSource() const override { return TEXT("Save"); }
};

UCLASS()
class EXTENDEDSAVE_API UESaveAsyncSaveSlot : public UESaveAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable)
	FESaveSucceededPin OnSaved;

	/** Captures the world and writes it to the slot. Display Name is shown in slot lists; empty keeps the slot's previous name. */
	UFUNCTION(BlueprintCallable, Category = "Extended Save", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Save Game To Slot (Extended)"))
	static UESaveAsyncSaveSlot* SaveGameToSlot(UObject* WorldContextObject, const FString& SlotName, const FString& DisplayName, float TimeoutSeconds = 0.f);

protected:
	virtual void ExecuteAction() override;

private:
	FString RequestedSlotName;
	FString RequestedDisplayName;
};

UCLASS()
class EXTENDEDSAVE_API UESaveAsyncLoadSlot : public UESaveAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Restored From Backup: the newest copy could not be read and the previous save was loaded. */
	UPROPERTY(BlueprintAssignable)
	FESaveLoadedPin OnLoaded;

	/** Reads the slot and restores it (Apply To Current World) or opens its level (Open Saved Level). */
	UFUNCTION(BlueprintCallable, Category = "Extended Save", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Load Game From Slot (Extended)"))
	static UESaveAsyncLoadSlot* LoadGameFromSlot(UObject* WorldContextObject, const FString& SlotName, EESaveLoadMode Mode = EESaveLoadMode::ApplyToCurrentWorld, float TimeoutSeconds = 0.f);

protected:
	virtual void ExecuteAction() override;

private:
	FString RequestedSlotName;
	EESaveLoadMode RequestedMode = EESaveLoadMode::ApplyToCurrentWorld;
};

UCLASS()
class EXTENDEDSAVE_API UESaveAsyncSyncFromCloud : public UESaveAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Downloaded: the local slot was replaced by the cloud copy. Load the slot to use it. */
	UPROPERTY(BlueprintAssignable)
	FESaveCloudSyncedPin OnSynced;

	UFUNCTION(BlueprintCallable, Category = "Extended Save|Cloud", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Sync Save Slot From Cloud"))
	static UESaveAsyncSyncFromCloud* SyncSlotFromCloud(UObject* WorldContextObject, const FString& SlotName, float TimeoutSeconds = 60.f);

protected:
	virtual void ExecuteAction() override;

private:
	FString RequestedSlotName;
};
