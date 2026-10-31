// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncPlayerData.h"
#include "PlayerData/EPFPlayerDataSubsystem.h"

// ── Get ──────────────────────────────────────────────────────────────────────

UEPFAsyncGetPlayerData* UEPFAsyncGetPlayerData::GetPlayerData(UObject* WorldContext, const TArray<FString>& Keys)
{
	auto* Action = NewObject<UEPFAsyncGetPlayerData>();
	Action->RequestedKeys = Keys;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncGetPlayerData::ExecuteAction()
{
	auto* Sub = RequireGameInstanceSubsystem<UEPFPlayerDataSubsystem>();
	if (!Sub) return;
	Sub->OnPlayerDataReceived.AddDynamic(this, &UEPFAsyncGetPlayerData::HandleComplete);
	CallWithContext([&]
	{
		if (RequestedKeys.Num() > 0) { Sub->GetPlayerData(RequestedKeys); }
		else { Sub->GetAllPlayerData(); }
	});
}

void UEPFAsyncGetPlayerData::CancelWork()
{
	if (auto* Sub = GetGameInstanceSubsystem<UEPFPlayerDataSubsystem>())
		Sub->OnPlayerDataReceived.RemoveDynamic(this, &UEPFAsyncGetPlayerData::HandleComplete);
}

void UEPFAsyncGetPlayerData::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get player data")));
}

// ── Set ──────────────────────────────────────────────────────────────────────

UEPFAsyncSetPlayerData* UEPFAsyncSetPlayerData::SetPlayerData(UObject* WorldContext, const TMap<FString, FString>& Data)
{
	auto* Action = NewObject<UEPFAsyncSetPlayerData>();
	Action->UpdatedData = Data;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncSetPlayerData::ExecuteAction()
{
	if (UpdatedData.Num() == 0) { FailInvalidArgument(TEXT("Data map cannot be empty")); return; }
	auto* Sub = RequireGameInstanceSubsystem<UEPFPlayerDataSubsystem>();
	if (!Sub) return;
	Sub->OnPlayerDataUpdated.AddDynamic(this, &UEPFAsyncSetPlayerData::HandleComplete);
	CallWithContext([&] { Sub->SetPlayerData(UpdatedData); });
}

void UEPFAsyncSetPlayerData::CancelWork()
{
	if (auto* Sub = GetGameInstanceSubsystem<UEPFPlayerDataSubsystem>())
		Sub->OnPlayerDataUpdated.RemoveDynamic(this, &UEPFAsyncSetPlayerData::HandleComplete);
}

void UEPFAsyncSetPlayerData::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to set player data")));
}

// ── Delete ───────────────────────────────────────────────────────────────────

UEPFAsyncDeletePlayerData* UEPFAsyncDeletePlayerData::DeletePlayerData(UObject* WorldContext, const FString& Key)
{
	auto* Action = NewObject<UEPFAsyncDeletePlayerData>();
	Action->DataKey = Key;
	Action->InitAsyncAction(WorldContext);
	return Action;
}

void UEPFAsyncDeletePlayerData::ExecuteAction()
{
	if (DataKey.IsEmpty()) { FailInvalidArgument(TEXT("Key cannot be empty")); return; }
	auto* Sub = RequireGameInstanceSubsystem<UEPFPlayerDataSubsystem>();
	if (!Sub) return;
	Sub->OnPlayerDataUpdated.AddDynamic(this, &UEPFAsyncDeletePlayerData::HandleComplete);
	CallWithContext([&] { Sub->DeletePlayerData(DataKey); });
}

void UEPFAsyncDeletePlayerData::CancelWork()
{
	if (auto* Sub = GetGameInstanceSubsystem<UEPFPlayerDataSubsystem>())
		Sub->OnPlayerDataUpdated.RemoveDynamic(this, &UEPFAsyncDeletePlayerData::HandleComplete);
}

void UEPFAsyncDeletePlayerData::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to delete player data")));
}
