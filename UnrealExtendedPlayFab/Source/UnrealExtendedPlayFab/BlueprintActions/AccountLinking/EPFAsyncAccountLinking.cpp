// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncAccountLinking.h"
#include "Engine/GameInstance.h"

// ── Link Steam ───────────────────────────────────────────────────────────────
UEPFAsyncLinkSteam* UEPFAsyncLinkSteam::LinkSteamAccount(UObject* WorldContext, const FString& SteamTicket, bool bForceLink)
{
	auto* A = NewObject<UEPFAsyncLinkSteam>(); A->Ticket = SteamTicket; A->bForce = bForceLink; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncLinkSteam::Activate()
{
	if (Ticket.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Steam ticket cannot be empty"))); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFAccountLinkingSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Account linking subsystem not available"))); SetReadyToDestroy(); return; }
	S->OnAccountLinked.AddDynamic(this, &UEPFAsyncLinkSteam::HandleComplete);
	S->LinkSteamAccount(Ticket, bForce);
}
void UEPFAsyncLinkSteam::HandleComplete(const FEPFResult& Result, const FString& Platform)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFAccountLinkingSubsystem>(WorldContext.Get())) S->OnAccountLinked.RemoveDynamic(this, &UEPFAsyncLinkSteam::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Platform) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to link Steam account")));
	SetReadyToDestroy();
}

// ── Link Custom ID ───────────────────────────────────────────────────────────
UEPFAsyncLinkCustomId* UEPFAsyncLinkCustomId::LinkCustomId(UObject* WorldContext, const FString& CustomId, bool bForceLink)
{
	auto* A = NewObject<UEPFAsyncLinkCustomId>(); A->Id = CustomId; A->bForce = bForceLink; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncLinkCustomId::Activate()
{
	if (Id.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("CustomId cannot be empty"))); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFAccountLinkingSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Account linking subsystem not available"))); SetReadyToDestroy(); return; }
	S->OnAccountLinked.AddDynamic(this, &UEPFAsyncLinkCustomId::HandleComplete);
	S->LinkCustomId(Id, bForce);
}
void UEPFAsyncLinkCustomId::HandleComplete(const FEPFResult& Result, const FString& Platform)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFAccountLinkingSubsystem>(WorldContext.Get())) S->OnAccountLinked.RemoveDynamic(this, &UEPFAsyncLinkCustomId::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Platform) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to link Custom ID")));
	SetReadyToDestroy();
}

// ── Unlink Steam ─────────────────────────────────────────────────────────────
UEPFAsyncUnlinkSteam* UEPFAsyncUnlinkSteam::UnlinkSteamAccount(UObject* WorldContext)
{
	auto* A = NewObject<UEPFAsyncUnlinkSteam>(); A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncUnlinkSteam::Activate()
{
	auto* S = GetEPFSubsystemFromContext<UEPFAccountLinkingSubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Account linking subsystem not available"))); SetReadyToDestroy(); return; }
	S->OnAccountUnlinked.AddDynamic(this, &UEPFAsyncUnlinkSteam::HandleComplete);
	S->UnlinkSteamAccount();
}
void UEPFAsyncUnlinkSteam::HandleComplete(const FEPFResult& Result, const FString& Platform)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFAccountLinkingSubsystem>(WorldContext.Get())) S->OnAccountUnlinked.RemoveDynamic(this, &UEPFAsyncUnlinkSteam::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Platform) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to unlink Steam account")));
	SetReadyToDestroy();
}
