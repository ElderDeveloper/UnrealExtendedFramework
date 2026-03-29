// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncContentDelivery.h"
#include "Engine/GameInstance.h"

// ── Get Content URL ──────────────────────────────────────────────────────────
UEPFAsyncGetContentUrl* UEPFAsyncGetContentUrl::GetContentDownloadUrl(UObject* WorldContext, const FString& Key, bool bThruCDN)
{
	auto* A = NewObject<UEPFAsyncGetContentUrl>(); A->Key = Key; A->bCDN = bThruCDN; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncGetContentUrl::Activate()
{
	if (Key.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Content key cannot be empty"))); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFContentDeliverySubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Content delivery subsystem not available"))); SetReadyToDestroy(); return; }
	S->OnContentUrlReceived.AddDynamic(this, &UEPFAsyncGetContentUrl::HandleComplete);
	S->GetContentDownloadUrl(Key, bCDN);
}
void UEPFAsyncGetContentUrl::HandleComplete(const FEPFResult& Result, const FString& Url)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFContentDeliverySubsystem>(WorldContext.Get())) S->OnContentUrlReceived.RemoveDynamic(this, &UEPFAsyncGetContentUrl::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Url) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get content URL")));
	SetReadyToDestroy();
}

// ── Download Content ─────────────────────────────────────────────────────────
UEPFAsyncDownloadContent* UEPFAsyncDownloadContent::DownloadContentAsString(UObject* WorldContext, const FString& Key)
{
	auto* A = NewObject<UEPFAsyncDownloadContent>(); A->Key = Key; A->WorldContext = WorldContext; A->RegisterWithGameInstance(WorldContext); return A;
}
void UEPFAsyncDownloadContent::Activate()
{
	if (Key.IsEmpty()) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Content key cannot be empty"))); SetReadyToDestroy(); return; }
	auto* S = GetEPFSubsystemFromContext<UEPFContentDeliverySubsystem>(WorldContext.Get());
	if (!S) { BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(TEXT("Content delivery subsystem not available"))); SetReadyToDestroy(); return; }
	S->OnContentDownloaded.AddDynamic(this, &UEPFAsyncDownloadContent::HandleComplete);
	S->DownloadContentAsString(Key);
}
void UEPFAsyncDownloadContent::HandleComplete(const FEPFResult& Result, const FString& Content)
{
	if (auto* S = GetEPFSubsystemFromContext<UEPFContentDeliverySubsystem>(WorldContext.Get())) S->OnContentDownloaded.RemoveDynamic(this, &UEPFAsyncDownloadContent::HandleComplete);
	Result.bSuccess ? OnSuccess.Broadcast(Content) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to download content")));
	SetReadyToDestroy();
}
