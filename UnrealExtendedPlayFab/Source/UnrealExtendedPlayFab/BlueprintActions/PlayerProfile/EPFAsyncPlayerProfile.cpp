// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncPlayerProfile.h"
#include "Engine/GameInstance.h"

// ── Get Profile ──────────────────────────────────────────────────────────────
UEPFAsyncGetProfile* UEPFAsyncGetProfile::GetPlayerProfile(UObject* WorldContext, const FString& PlayFabId)
{
	auto* A = NewObject<UEPFAsyncGetProfile>(); A->TargetId = PlayFabId; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncGetProfile::Activate()
{
	auto* S = GetEPFSubsystemFromContext<UEPFPlayerProfileSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, TEXT("Profile subsystem not available")); SetReadyToDestroy(); return; }
	S->OnProfileReceived.AddDynamic(this, &UEPFAsyncGetProfile::HandleComplete);
	S->GetPlayerProfile(TargetId);
}
void UEPFAsyncGetProfile::HandleComplete(const FEPFResult& Result, const FEPFPlayerProfile& Profile)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFPlayerProfileSubsystem>(WorldContext.Get())) S->OnProfileReceived.RemoveDynamic(this, &UEPFAsyncGetProfile::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Profile) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get profile")));
	SetReadyToDestroy();
}

// ── Get Account Info ─────────────────────────────────────────────────────────
UEPFAsyncGetAccountInfo* UEPFAsyncGetAccountInfo::GetAccountInfo(UObject* WorldContext, const FString& PlayFabId)
{
	auto* A = NewObject<UEPFAsyncGetAccountInfo>(); A->TargetId = PlayFabId; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncGetAccountInfo::Activate()
{
	auto* S = GetEPFSubsystemFromContext<UEPFPlayerProfileSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, TEXT("Profile subsystem not available")); SetReadyToDestroy(); return; }
	S->OnAccountInfoReceived.AddDynamic(this, &UEPFAsyncGetAccountInfo::HandleComplete);
	S->GetAccountInfo(TargetId);
}
void UEPFAsyncGetAccountInfo::HandleComplete(const FEPFResult& Result, const FEPFAccountInfo& AccountInfo)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFPlayerProfileSubsystem>(WorldContext.Get())) S->OnAccountInfoReceived.RemoveDynamic(this, &UEPFAsyncGetAccountInfo::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(AccountInfo) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get account info")));
	SetReadyToDestroy();
}

// ── Get Combined Info ────────────────────────────────────────────────────────
UEPFAsyncGetCombinedInfo* UEPFAsyncGetCombinedInfo::GetCombinedInfo(UObject* WorldContext, bool bGetStats, bool bGetPlayerData, bool bGetCurrency)
{
	auto* A = NewObject<UEPFAsyncGetCombinedInfo>(); A->bStats = bGetStats; A->bData = bGetPlayerData; A->bCurrency = bGetCurrency; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncGetCombinedInfo::Activate()
{
	auto* S = GetEPFSubsystemFromContext<UEPFPlayerProfileSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, TEXT("Profile subsystem not available")); SetReadyToDestroy(); return; }
	S->OnCombinedInfoReceived.AddDynamic(this, &UEPFAsyncGetCombinedInfo::HandleComplete);
	S->GetPlayerCombinedInfo(bStats, bData, bCurrency);
}
void UEPFAsyncGetCombinedInfo::HandleComplete(const FEPFResult& Result, const FEPFPlayerProfile& Profile)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFPlayerProfileSubsystem>(WorldContext.Get())) S->OnCombinedInfoReceived.RemoveDynamic(this, &UEPFAsyncGetCombinedInfo::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Profile) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get combined info")));
	SetReadyToDestroy();
}

// ── Update Avatar ────────────────────────────────────────────────────────────
UEPFAsyncUpdateAvatar* UEPFAsyncUpdateAvatar::UpdateAvatarUrl(UObject* WorldContext, const FString& AvatarUrl)
{
	auto* A = NewObject<UEPFAsyncUpdateAvatar>(); A->Url = AvatarUrl; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncUpdateAvatar::Activate()
{
	if (Url.IsEmpty()) { BroadcastEPFFailure(OnFailure, TEXT("Avatar URL cannot be empty")); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFPlayerProfileSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, TEXT("Profile subsystem not available")); SetReadyToDestroy(); return; }
	S->OnAvatarUpdated.AddDynamic(this, &UEPFAsyncUpdateAvatar::HandleComplete);
	S->UpdateAvatarUrl(Url);
}
void UEPFAsyncUpdateAvatar::HandleComplete(const FEPFResult& Result)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFPlayerProfileSubsystem>(WorldContext.Get())) S->OnAvatarUpdated.RemoveDynamic(this, &UEPFAsyncUpdateAvatar::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to update avatar")));
	SetReadyToDestroy();
}
