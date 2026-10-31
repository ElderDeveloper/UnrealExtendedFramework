// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EPFAsyncContentDelivery.h"

// ── Get Content URL ──────────────────────────────────────────────────────────
UEPFAsyncGetContentUrl* UEPFAsyncGetContentUrl::GetContentDownloadUrl(UObject* WorldContext, const FString& Key, bool bThruCDN)
{
	auto* A = NewObject<UEPFAsyncGetContentUrl>(); A->ContentKey = Key; A->bCDN = bThruCDN; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncGetContentUrl::ExecuteAction()
{
	if (ContentKey.IsEmpty()) { FailInvalidArgument(TEXT("Content key cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFContentDeliverySubsystem>();
	if (!S) return;
	S->OnContentUrlReceived.AddDynamic(this, &UEPFAsyncGetContentUrl::HandleComplete);
	CallWithContext([&] { S->GetContentDownloadUrl(ContentKey, bCDN); });
}
void UEPFAsyncGetContentUrl::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFContentDeliverySubsystem>()) S->OnContentUrlReceived.RemoveDynamic(this, &UEPFAsyncGetContentUrl::HandleComplete);
}
void UEPFAsyncGetContentUrl::HandleComplete(const FEPFResult& Result, const FString& Url)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Url) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to get content URL")));
}

// ── Download Content ─────────────────────────────────────────────────────────
UEPFAsyncDownloadContent* UEPFAsyncDownloadContent::DownloadContentAsString(UObject* WorldContext, const FString& Key)
{
	auto* A = NewObject<UEPFAsyncDownloadContent>(); A->ContentKey = Key; A->InitAsyncAction(WorldContext); return A;
}
void UEPFAsyncDownloadContent::ExecuteAction()
{
	if (ContentKey.IsEmpty()) { FailInvalidArgument(TEXT("Content key cannot be empty")); return; }
	auto* S = RequireGameInstanceSubsystem<UEPFContentDeliverySubsystem>();
	if (!S) return;
	S->OnContentDownloaded.AddDynamic(this, &UEPFAsyncDownloadContent::HandleComplete);
	CallWithContext([&] { S->DownloadContentAsString(ContentKey); });
}
void UEPFAsyncDownloadContent::CancelWork()
{
	if (auto* S = GetGameInstanceSubsystem<UEPFContentDeliverySubsystem>()) S->OnContentDownloaded.RemoveDynamic(this, &UEPFAsyncDownloadContent::HandleComplete);
}
void UEPFAsyncDownloadContent::HandleComplete(const FEPFResult& Result, const FString& Content)
{
	if (!IsResultForThisCall()) return;
	CancelWork();
	if (!FinishAction()) return;
	Result.bSuccess ? OnSuccess.Broadcast(Content) : BroadcastEPFFailure(OnFailure, MakeEPFAsyncError(Result, TEXT("Failed to download content")));
}
