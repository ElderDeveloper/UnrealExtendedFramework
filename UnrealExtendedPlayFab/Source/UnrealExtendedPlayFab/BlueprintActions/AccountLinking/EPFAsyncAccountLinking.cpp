// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncAccountLinking.h"

// ── Link Steam ───────────────────────────────────────────────────────────────
UEPFAsyncLinkSteam* UEPFAsyncLinkSteam::LinkSteamAccount(UObject* WorldContext, const FString& SteamTicket, bool bForceLink)
{
	auto* A = NewObject<UEPFAsyncLinkSteam>(); A->Ticket = SteamTicket; A->bForce = bForceLink; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncLinkSteam::ExecuteAction()
{
	if (Ticket.IsEmpty()) { FailInvalidArgument(TEXT("Steam ticket cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFAccountLinkingSubsystem>();
	if (!S) return;
	S->OnAccountLinked.AddDynamic(this, &UEPFAsyncLinkSteam::HandleComplete);
	CallWithContext([&] { S->LinkSteamAccount(Ticket, bForce); });
}
void UEPFAsyncLinkSteam::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFAccountLinkingSubsystem>()) S->OnAccountLinked.RemoveDynamic(this, &UEPFAsyncLinkSteam::HandleComplete);
}
void UEPFAsyncLinkSteam::HandleComplete(const FEPFResult& Result, const FString& Platform)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Platform) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to link Steam account")));
}

// ── Link Custom ID ───────────────────────────────────────────────────────────
UEPFAsyncLinkCustomId* UEPFAsyncLinkCustomId::LinkCustomId(UObject* WorldContext, const FString& CustomId, bool bForceLink)
{
	auto* A = NewObject<UEPFAsyncLinkCustomId>(); A->Id = CustomId; A->bForce = bForceLink; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncLinkCustomId::ExecuteAction()
{
	if (Id.IsEmpty()) { FailInvalidArgument(TEXT("CustomId cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFAccountLinkingSubsystem>();
	if (!S) return;
	S->OnAccountLinked.AddDynamic(this, &UEPFAsyncLinkCustomId::HandleComplete);
	CallWithContext([&] { S->LinkCustomId(Id, bForce); });
}
void UEPFAsyncLinkCustomId::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFAccountLinkingSubsystem>()) S->OnAccountLinked.RemoveDynamic(this, &UEPFAsyncLinkCustomId::HandleComplete);
}
void UEPFAsyncLinkCustomId::HandleComplete(const FEPFResult& Result, const FString& Platform)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Platform) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to link Custom ID")));
}

// ── Unlink Steam ─────────────────────────────────────────────────────────────
UEPFAsyncUnlinkSteam* UEPFAsyncUnlinkSteam::UnlinkSteamAccount(UObject* WorldContext)
{
	auto* A = NewObject<UEPFAsyncUnlinkSteam>(); A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncUnlinkSteam::ExecuteAction()
{
	auto* S = RequireGameInstanceSubsystem<UEPFAccountLinkingSubsystem>();
	if (!S) return;
	S->OnAccountUnlinked.AddDynamic(this, &UEPFAsyncUnlinkSteam::HandleComplete);
	CallWithContext([&] { S->UnlinkSteamAccount(); });
}
void UEPFAsyncUnlinkSteam::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFAccountLinkingSubsystem>()) S->OnAccountUnlinked.RemoveDynamic(this, &UEPFAsyncUnlinkSteam::HandleComplete);
}
void UEPFAsyncUnlinkSteam::HandleComplete(const FEPFResult& Result, const FString& Platform)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Platform) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to unlink Steam account")));
}
