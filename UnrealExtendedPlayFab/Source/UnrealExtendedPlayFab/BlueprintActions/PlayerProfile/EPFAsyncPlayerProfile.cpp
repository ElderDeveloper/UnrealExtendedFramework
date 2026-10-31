// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncPlayerProfile.h"

// ── Get Profile ──────────────────────────────────────────────────────────────
UEPFAsyncGetProfile* UEPFAsyncGetProfile::GetPlayerProfile(UObject* WorldContext, const FString& PlayFabId)
{
	auto* A = NewObject<UEPFAsyncGetProfile>(); A->TargetId = PlayFabId; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncGetProfile::ExecuteAction()
{
	auto* S = RequireGameInstanceSubsystem<UEPFPlayerProfileSubsystem>();
	if (!S) return;
	S->OnProfileReceived.AddDynamic(this, &UEPFAsyncGetProfile::HandleComplete);
	CallWithContext([&] { S->GetPlayerProfile(TargetId); });
}
void UEPFAsyncGetProfile::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFPlayerProfileSubsystem>()) S->OnProfileReceived.RemoveDynamic(this, &UEPFAsyncGetProfile::HandleComplete);
}
void UEPFAsyncGetProfile::HandleComplete(const FEPFResult& Result, const FEPFPlayerProfile& Profile)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Profile) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get profile")));
}

// ── Get Account Info ─────────────────────────────────────────────────────────
UEPFAsyncGetAccountInfo* UEPFAsyncGetAccountInfo::GetAccountInfo(UObject* WorldContext, const FString& PlayFabId)
{
	auto* A = NewObject<UEPFAsyncGetAccountInfo>(); A->TargetId = PlayFabId; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncGetAccountInfo::ExecuteAction()
{
	auto* S = RequireGameInstanceSubsystem<UEPFPlayerProfileSubsystem>();
	if (!S) return;
	S->OnAccountInfoReceived.AddDynamic(this, &UEPFAsyncGetAccountInfo::HandleComplete);
	CallWithContext([&] { S->GetAccountInfo(TargetId); });
}
void UEPFAsyncGetAccountInfo::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFPlayerProfileSubsystem>()) S->OnAccountInfoReceived.RemoveDynamic(this, &UEPFAsyncGetAccountInfo::HandleComplete);
}
void UEPFAsyncGetAccountInfo::HandleComplete(const FEPFResult& Result, const FEPFAccountInfo& AccountInfo)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(AccountInfo) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get account info")));
}

// ── Get Combined Info ────────────────────────────────────────────────────────
UEPFAsyncGetCombinedInfo* UEPFAsyncGetCombinedInfo::GetCombinedInfo(UObject* WorldContext, bool bGetStats, bool bGetPlayerData, bool bGetCurrency)
{
	auto* A = NewObject<UEPFAsyncGetCombinedInfo>(); A->bStats = bGetStats; A->bData = bGetPlayerData; A->bCurrency = bGetCurrency; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncGetCombinedInfo::ExecuteAction()
{
	auto* S = RequireGameInstanceSubsystem<UEPFPlayerProfileSubsystem>();
	if (!S) return;
	S->OnCombinedInfoReceived.AddDynamic(this, &UEPFAsyncGetCombinedInfo::HandleComplete);
	CallWithContext([&] { S->GetPlayerCombinedInfo(bStats, bData, bCurrency); });
}
void UEPFAsyncGetCombinedInfo::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFPlayerProfileSubsystem>()) S->OnCombinedInfoReceived.RemoveDynamic(this, &UEPFAsyncGetCombinedInfo::HandleComplete);
}
void UEPFAsyncGetCombinedInfo::HandleComplete(const FEPFResult& Result, const FEPFPlayerProfile& Profile)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Profile) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get combined info")));
}

// ── Update Avatar ────────────────────────────────────────────────────────────
UEPFAsyncUpdateAvatar* UEPFAsyncUpdateAvatar::UpdateAvatarUrl(UObject* WorldContext, const FString& AvatarUrl)
{
	auto* A = NewObject<UEPFAsyncUpdateAvatar>(); A->Url = AvatarUrl; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncUpdateAvatar::ExecuteAction()
{
	if (Url.IsEmpty()) { FailInvalidArgument(TEXT("Avatar URL cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFPlayerProfileSubsystem>();
	if (!S) return;
	S->OnAvatarUpdated.AddDynamic(this, &UEPFAsyncUpdateAvatar::HandleComplete);
	CallWithContext([&] { S->UpdateAvatarUrl(Url); });
}
void UEPFAsyncUpdateAvatar::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFPlayerProfileSubsystem>()) S->OnAvatarUpdated.RemoveDynamic(this, &UEPFAsyncUpdateAvatar::HandleComplete);
}
void UEPFAsyncUpdateAvatar::HandleComplete(const FEPFResult& Result)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast() : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to update avatar")));
}
