// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncPlayerData.h"
#include "PlayerData/EPFPlayerDataSubsystem.h"
#include "Engine/GameInstance.h"

// ── Get ──────────────────────────────────────────────────────────────────────

UEPFAsyncGetPlayerData* UEPFAsyncGetPlayerData::GetPlayerData(UObject* WorldContext, const TArray<FString>& Keys)
{
	auto* Action = NewObject<UEPFAsyncGetPlayerData>();
	Action->Keys = Keys;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncGetPlayerData::Activate()
{
	auto* Sub = GetEPFSubsystemFromContext<UEPFPlayerDataSubsystem>(WorldContext.Get());
	if (!Sub) { BroadcastEPFFailure(OnFailure, TEXT("PlayerData subsystem not available")); SetReadyToDestroy(); return; }
	Sub->OnPlayerDataReceived.AddDynamic(this, &UEPFAsyncGetPlayerData::HandleComplete);
	Keys.Num() > 0 ? Sub->GetPlayerData(Keys) : Sub->GetAllPlayerData();
}

void UEPFAsyncGetPlayerData::HandleComplete(const FEPFResult& Result)
{
	if (auto* Sub = GetEPFSubsystemFromContext<UEPFPlayerDataSubsystem>(WorldContext.Get()))
		Sub->OnPlayerDataReceived.RemoveDynamic(this, &UEPFAsyncGetPlayerData::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get player data")));
	SetReadyToDestroy();
}

// ── Set ──────────────────────────────────────────────────────────────────────

UEPFAsyncSetPlayerData* UEPFAsyncSetPlayerData::SetPlayerData(UObject* WorldContext, const TMap<FString, FString>& Data)
{
	auto* Action = NewObject<UEPFAsyncSetPlayerData>();
	Action->Data = Data;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncSetPlayerData::Activate()
{
	if (Data.Num() == 0) { BroadcastEPFFailure(OnFailure, TEXT("Data map cannot be empty")); SetReadyToDestroy(); return; }
	auto* Sub = GetEPFSubsystemFromContext<UEPFPlayerDataSubsystem>(WorldContext.Get());
	if (!Sub) { BroadcastEPFFailure(OnFailure, TEXT("PlayerData subsystem not available")); SetReadyToDestroy(); return; }
	Sub->OnPlayerDataUpdated.AddDynamic(this, &UEPFAsyncSetPlayerData::HandleComplete);
	Sub->SetPlayerData(Data);
}

void UEPFAsyncSetPlayerData::HandleComplete(const FEPFResult& Result)
{
	if (auto* Sub = GetEPFSubsystemFromContext<UEPFPlayerDataSubsystem>(WorldContext.Get()))
		Sub->OnPlayerDataUpdated.RemoveDynamic(this, &UEPFAsyncSetPlayerData::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to set player data")));
	SetReadyToDestroy();
}

// ── Delete ───────────────────────────────────────────────────────────────────

UEPFAsyncDeletePlayerData* UEPFAsyncDeletePlayerData::DeletePlayerData(UObject* WorldContext, const FString& Key)
{
	auto* Action = NewObject<UEPFAsyncDeletePlayerData>();
	Action->Key = Key;
	Action->WorldContext = WorldContext;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void UEPFAsyncDeletePlayerData::Activate()
{
	if (Key.IsEmpty()) { BroadcastEPFFailure(OnFailure, TEXT("Key cannot be empty")); SetReadyToDestroy(); return; }
	auto* Sub = GetEPFSubsystemFromContext<UEPFPlayerDataSubsystem>(WorldContext.Get());
	if (!Sub) { BroadcastEPFFailure(OnFailure, TEXT("PlayerData subsystem not available")); SetReadyToDestroy(); return; }
	Sub->OnPlayerDataUpdated.AddDynamic(this, &UEPFAsyncDeletePlayerData::HandleComplete);
	Sub->DeletePlayerData(Key);
}

void UEPFAsyncDeletePlayerData::HandleComplete(const FEPFResult& Result)
{
	if (auto* Sub = GetEPFSubsystemFromContext<UEPFPlayerDataSubsystem>(WorldContext.Get()))
		Sub->OnPlayerDataUpdated.RemoveDynamic(this, &UEPFAsyncDeletePlayerData::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to delete player data")));
	SetReadyToDestroy();
}
