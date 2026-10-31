// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ESaveAsyncActions.h"

#include "ESaveSubsystem.h"

UESaveAsyncSaveSlot* UESaveAsyncSaveSlot::SaveGameToSlot(UObject* WorldContextObject, const FString& SlotName, const FString& DisplayName, float TimeoutSeconds)
{
	UESaveAsyncSaveSlot* Action = NewObject<UESaveAsyncSaveSlot>();
	Action->RequestedSlotName = SlotName;
	Action->RequestedDisplayName = DisplayName;
	Action->InitAsyncAction(WorldContextObject, TimeoutSeconds);
	return Action;
}

void UESaveAsyncSaveSlot::ExecuteAction()
{
	UESaveSubsystem* Save = RequireGameInstanceSubsystem<UESaveSubsystem>();
	if (!Save)
	{
		return;
	}
	Save->SaveSlot(RequestedSlotName, MakeWeakCallback<const FEFResult&>([this](const FEFResult& Result)
	{
		if (FinishAction())
		{
			Result.IsSuccess() ? OnSaved.Broadcast() : OnFailure.Broadcast(Result);
		}
	}), RequestedDisplayName);
}

UESaveAsyncLoadSlot* UESaveAsyncLoadSlot::LoadGameFromSlot(UObject* WorldContextObject, const FString& SlotName, EESaveLoadMode Mode, float TimeoutSeconds)
{
	UESaveAsyncLoadSlot* Action = NewObject<UESaveAsyncLoadSlot>();
	Action->RequestedSlotName = SlotName;
	Action->RequestedMode = Mode;
	Action->InitAsyncAction(WorldContextObject, TimeoutSeconds);
	return Action;
}

void UESaveAsyncLoadSlot::ExecuteAction()
{
	UESaveSubsystem* Save = RequireGameInstanceSubsystem<UESaveSubsystem>();
	if (!Save)
	{
		return;
	}
	Save->LoadSlot(RequestedSlotName, RequestedMode, MakeWeakCallback<const FEFResult&, bool>([this](const FEFResult& Result, bool bRestoredFromBackup)
	{
		if (FinishAction())
		{
			Result.IsSuccess() ? OnLoaded.Broadcast(bRestoredFromBackup) : OnFailure.Broadcast(Result);
		}
	}));
}

UESaveAsyncSyncFromCloud* UESaveAsyncSyncFromCloud::SyncSlotFromCloud(UObject* WorldContextObject, const FString& SlotName, float TimeoutSeconds)
{
	UESaveAsyncSyncFromCloud* Action = NewObject<UESaveAsyncSyncFromCloud>();
	Action->RequestedSlotName = SlotName;
	Action->InitAsyncAction(WorldContextObject, TimeoutSeconds);
	return Action;
}

void UESaveAsyncSyncFromCloud::ExecuteAction()
{
	UESaveSubsystem* Save = RequireGameInstanceSubsystem<UESaveSubsystem>();
	if (!Save)
	{
		return;
	}
	Save->SyncSlotFromCloud(RequestedSlotName, MakeWeakCallback<const FEFResult&, bool>([this](const FEFResult& Result, bool bDownloaded)
	{
		if (FinishAction())
		{
			Result.IsSuccess() ? OnSynced.Broadcast(bDownloaded) : OnFailure.Broadcast(Result);
		}
	}));
}
