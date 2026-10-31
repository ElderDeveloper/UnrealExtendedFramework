// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "BlueprintActions/UGC/ESteamUGCAsyncActions.h"

// Every node below keeps its subsystem in a member named Subsystem, so locals use UGCSubsystem.

// ---- USteamAsyncCreateWorkshopItem ----

USteamAsyncCreateWorkshopItem* USteamAsyncCreateWorkshopItem::CreateWorkshopItem(UObject* WorldContext, EESteamWorkshopFileType FileType, float Timeout)
{
	USteamAsyncCreateWorkshopItem* Action = NewObject<USteamAsyncCreateWorkshopItem>();
	Action->FileType = FileType;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncCreateWorkshopItem::ExecuteAction()
{
	UESteamUGCSubsystem* UGCSubsystem = RequireGameInstanceSubsystem<UESteamUGCSubsystem>();
	if (!UGCSubsystem)
	{
		return;
	}

	Subsystem = UGCSubsystem;
	UGCSubsystem->OnItemCreated.AddDynamic(this, &USteamAsyncCreateWorkshopItem::HandleItemCreated);

	bool bStarted = false;
	CallWithContext([&] { bStarted = UGCSubsystem->CreateItem(FileType); });
	if (!bStarted)
	{
		Complete(false, 0, false);
	}
}

void USteamAsyncCreateWorkshopItem::HandleItemCreated(bool bSuccess, int64 PublishedFileId, bool bNeedsLegalAgreement)
{
	// FOnSteamUGCItemCreated's PublishedFileId is the result (unknown before the call), so it cannot
	// correlate concurrent creates; the subsystem tags each result with the call context of its request.
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(bSuccess, PublishedFileId, bNeedsLegalAgreement);
}

void USteamAsyncCreateWorkshopItem::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(0, false);
}

void USteamAsyncCreateWorkshopItem::CancelWork()
{
	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnItemCreated.RemoveDynamic(this, &USteamAsyncCreateWorkshopItem::HandleItemCreated);
	}
}

void USteamAsyncCreateWorkshopItem::Complete(bool bSuccess, int64 PublishedFileId, bool bNeedsLegalAgreement)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(PublishedFileId, bNeedsLegalAgreement);
	}
	else
	{
		OnFailure.Broadcast(PublishedFileId, bNeedsLegalAgreement);
	}
}

// ---- USteamAsyncSubmitWorkshopItem ----

USteamAsyncSubmitWorkshopItem* USteamAsyncSubmitWorkshopItem::SubmitWorkshopItem(UObject* WorldContext, int64 UpdateHandle, const FString& ChangeNote, float Timeout)
{
	USteamAsyncSubmitWorkshopItem* Action = NewObject<USteamAsyncSubmitWorkshopItem>();
	Action->UpdateHandle = UpdateHandle;
	Action->ChangeNote = ChangeNote;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncSubmitWorkshopItem::ExecuteAction()
{
	UESteamUGCSubsystem* UGCSubsystem = RequireGameInstanceSubsystem<UESteamUGCSubsystem>();
	if (!UGCSubsystem)
	{
		return;
	}

	Subsystem = UGCSubsystem;
	UGCSubsystem->OnItemSubmitted.AddDynamic(this, &USteamAsyncSubmitWorkshopItem::HandleItemSubmitted);

	bool bStarted = false;
	CallWithContext([&] { bStarted = UGCSubsystem->SubmitItemUpdate(UpdateHandle, ChangeNote); });
	if (!bStarted)
	{
		Complete(false, false);
	}
}

void USteamAsyncSubmitWorkshopItem::HandleItemSubmitted(bool bSuccess, bool bNeedsLegalAgreement)
{
	// FOnSteamUGCItemSubmitted carries no update handle; the subsystem tags each result with the
	// call context of its request.
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(bSuccess, bNeedsLegalAgreement);
}

void USteamAsyncSubmitWorkshopItem::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(false);
}

void USteamAsyncSubmitWorkshopItem::CancelWork()
{
	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnItemSubmitted.RemoveDynamic(this, &USteamAsyncSubmitWorkshopItem::HandleItemSubmitted);
	}
}

void USteamAsyncSubmitWorkshopItem::Complete(bool bSuccess, bool bNeedsLegalAgreement)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(bNeedsLegalAgreement);
	}
	else
	{
		OnFailure.Broadcast(bNeedsLegalAgreement);
	}
}

// ---- USteamAsyncQueryWorkshopItems ----

USteamAsyncQueryWorkshopItems* USteamAsyncQueryWorkshopItems::QueryWorkshopItems(UObject* WorldContext, EESteamUGCQueryType QueryType, EESteamUGCMatchingType MatchingType, int32 Page, float Timeout)
{
	USteamAsyncQueryWorkshopItems* Action = NewObject<USteamAsyncQueryWorkshopItems>();
	Action->QueryType = QueryType;
	Action->MatchingType = MatchingType;
	Action->Page = Page;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncQueryWorkshopItems::ExecuteAction()
{
	UESteamUGCSubsystem* UGCSubsystem = RequireGameInstanceSubsystem<UESteamUGCSubsystem>();
	if (!UGCSubsystem)
	{
		return;
	}

	Subsystem = UGCSubsystem;
	UGCSubsystem->OnQueryCompleted.AddDynamic(this, &USteamAsyncQueryWorkshopItems::HandleQueryCompleted);

	bool bStarted = false;
	CallWithContext([&] { bStarted = UGCSubsystem->QueryAllItems(QueryType, MatchingType, Page); });
	if (!bStarted)
	{
		Complete(false, TArray<FESteamUGCDetails>(), 0);
	}
}

void USteamAsyncQueryWorkshopItems::HandleQueryCompleted(bool bSuccess, const TArray<FESteamUGCDetails>& Results, int32 TotalMatchingResults)
{
	// FOnSteamUGCQueryCompleted carries no query handle; the subsystem tags each result with the
	// call context of its request.
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(bSuccess, Results, TotalMatchingResults);
}

void USteamAsyncQueryWorkshopItems::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(TArray<FESteamUGCDetails>(), 0);
}

void USteamAsyncQueryWorkshopItems::CancelWork()
{
	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnQueryCompleted.RemoveDynamic(this, &USteamAsyncQueryWorkshopItems::HandleQueryCompleted);
	}
}

void USteamAsyncQueryWorkshopItems::Complete(bool bSuccess, const TArray<FESteamUGCDetails>& Results, int32 TotalResults)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(Results, TotalResults);
	}
	else
	{
		OnFailure.Broadcast(Results, TotalResults);
	}
}

// ---- USteamAsyncSubscribeWorkshopItem ----

USteamAsyncSubscribeWorkshopItem* USteamAsyncSubscribeWorkshopItem::SubscribeWorkshopItem(UObject* WorldContext, int64 PublishedFileId, float Timeout)
{
	USteamAsyncSubscribeWorkshopItem* Action = NewObject<USteamAsyncSubscribeWorkshopItem>();
	Action->PublishedFileId = PublishedFileId;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncSubscribeWorkshopItem::ExecuteAction()
{
	UESteamUGCSubsystem* UGCSubsystem = RequireGameInstanceSubsystem<UESteamUGCSubsystem>();
	if (!UGCSubsystem)
	{
		return;
	}

	Subsystem = UGCSubsystem;
	UGCSubsystem->OnSubscribed.AddDynamic(this, &USteamAsyncSubscribeWorkshopItem::HandleSubscribed);

	bool bStarted = false;
	CallWithContext([&] { bStarted = UGCSubsystem->SubscribeItem(PublishedFileId); });
	if (!bStarted)
	{
		Complete(false);
	}
}

void USteamAsyncSubscribeWorkshopItem::HandleSubscribed(bool bSuccess, int64 InPublishedFileId)
{
	// Other requests may be in flight on the shared subsystem delegate; only react to ours.
	if (!IsResultForThisCall() || InPublishedFileId != PublishedFileId)
	{
		return;
	}
	Complete(bSuccess);
}

void USteamAsyncSubscribeWorkshopItem::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(PublishedFileId);
}

void USteamAsyncSubscribeWorkshopItem::CancelWork()
{
	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnSubscribed.RemoveDynamic(this, &USteamAsyncSubscribeWorkshopItem::HandleSubscribed);
	}
}

void USteamAsyncSubscribeWorkshopItem::Complete(bool bSuccess)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(PublishedFileId);
	}
	else
	{
		OnFailure.Broadcast(PublishedFileId);
	}
}

// ---- USteamAsyncUnsubscribeWorkshopItem ----

USteamAsyncUnsubscribeWorkshopItem* USteamAsyncUnsubscribeWorkshopItem::UnsubscribeWorkshopItem(UObject* WorldContext, int64 PublishedFileId, float Timeout)
{
	USteamAsyncUnsubscribeWorkshopItem* Action = NewObject<USteamAsyncUnsubscribeWorkshopItem>();
	Action->PublishedFileId = PublishedFileId;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncUnsubscribeWorkshopItem::ExecuteAction()
{
	UESteamUGCSubsystem* UGCSubsystem = RequireGameInstanceSubsystem<UESteamUGCSubsystem>();
	if (!UGCSubsystem)
	{
		return;
	}

	Subsystem = UGCSubsystem;
	UGCSubsystem->OnUnsubscribed.AddDynamic(this, &USteamAsyncUnsubscribeWorkshopItem::HandleUnsubscribed);

	bool bStarted = false;
	CallWithContext([&] { bStarted = UGCSubsystem->UnsubscribeItem(PublishedFileId); });
	if (!bStarted)
	{
		Complete(false);
	}
}

void USteamAsyncUnsubscribeWorkshopItem::HandleUnsubscribed(bool bSuccess, int64 InPublishedFileId)
{
	if (!IsResultForThisCall() || InPublishedFileId != PublishedFileId)
	{
		return;
	}
	Complete(bSuccess);
}

void USteamAsyncUnsubscribeWorkshopItem::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(PublishedFileId);
}

void USteamAsyncUnsubscribeWorkshopItem::CancelWork()
{
	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnUnsubscribed.RemoveDynamic(this, &USteamAsyncUnsubscribeWorkshopItem::HandleUnsubscribed);
	}
}

void USteamAsyncUnsubscribeWorkshopItem::Complete(bool bSuccess)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(PublishedFileId);
	}
	else
	{
		OnFailure.Broadcast(PublishedFileId);
	}
}

// ---- USteamAsyncDownloadWorkshopItem ----

USteamAsyncDownloadWorkshopItem* USteamAsyncDownloadWorkshopItem::DownloadWorkshopItem(UObject* WorldContext, int64 PublishedFileId, bool bHighPriority, float Timeout)
{
	USteamAsyncDownloadWorkshopItem* Action = NewObject<USteamAsyncDownloadWorkshopItem>();
	Action->PublishedFileId = PublishedFileId;
	Action->bHighPriority = bHighPriority;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncDownloadWorkshopItem::ExecuteAction()
{
	UESteamUGCSubsystem* UGCSubsystem = RequireGameInstanceSubsystem<UESteamUGCSubsystem>();
	if (!UGCSubsystem)
	{
		return;
	}

	Subsystem = UGCSubsystem;
	UGCSubsystem->OnItemDownloaded.AddDynamic(this, &USteamAsyncDownloadWorkshopItem::HandleItemDownloaded);

	bool bStarted = false;
	CallWithContext([&] { bStarted = UGCSubsystem->DownloadItem(PublishedFileId, bHighPriority); });
	if (!bStarted)
	{
		Complete(false);
	}
}

void USteamAsyncDownloadWorkshopItem::HandleItemDownloaded(bool bSuccess, int64 InPublishedFileId)
{
	// The download callback fires for every item this client downloads; only react to ours.
	if (!IsResultForThisCall() || InPublishedFileId != PublishedFileId)
	{
		return;
	}
	Complete(bSuccess);
}

void USteamAsyncDownloadWorkshopItem::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(PublishedFileId);
}

void USteamAsyncDownloadWorkshopItem::CancelWork()
{
	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnItemDownloaded.RemoveDynamic(this, &USteamAsyncDownloadWorkshopItem::HandleItemDownloaded);
	}
}

void USteamAsyncDownloadWorkshopItem::Complete(bool bSuccess)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(PublishedFileId);
	}
	else
	{
		OnFailure.Broadcast(PublishedFileId);
	}
}

// ---- USteamAsyncSetWorkshopFavorite ----

USteamAsyncSetWorkshopFavorite* USteamAsyncSetWorkshopFavorite::SetWorkshopFavorite(UObject* WorldContext, int32 AppId, int64 PublishedFileId, bool bAddToFavorites, float Timeout)
{
	USteamAsyncSetWorkshopFavorite* Action = NewObject<USteamAsyncSetWorkshopFavorite>();
	Action->AppId = AppId;
	Action->PublishedFileId = PublishedFileId;
	Action->bAddToFavorites = bAddToFavorites;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncSetWorkshopFavorite::ExecuteAction()
{
	UESteamUGCSubsystem* UGCSubsystem = RequireGameInstanceSubsystem<UESteamUGCSubsystem>();
	if (!UGCSubsystem)
	{
		return;
	}

	Subsystem = UGCSubsystem;
	UGCSubsystem->OnItemFavoriteChanged.AddDynamic(this, &USteamAsyncSetWorkshopFavorite::HandleFavoriteChanged);

	bool bIssued = false;
	CallWithContext([&]
	{
		bIssued = bAddToFavorites
			? UGCSubsystem->AddItemToFavorites(AppId, PublishedFileId)
			: UGCSubsystem->RemoveItemFromFavorites(AppId, PublishedFileId);
	});
	if (!bIssued)
	{
		Complete(false);
	}
}

void USteamAsyncSetWorkshopFavorite::HandleFavoriteChanged(bool bSuccess, int64 InPublishedFileId, bool bWasAddRequest)
{
	// Favorites add/remove share one subsystem delegate; only react to our item and our direction.
	if (!IsResultForThisCall() || InPublishedFileId != PublishedFileId || bWasAddRequest != bAddToFavorites)
	{
		return;
	}
	Complete(bSuccess);
}

void USteamAsyncSetWorkshopFavorite::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(PublishedFileId, bAddToFavorites);
}

void USteamAsyncSetWorkshopFavorite::CancelWork()
{
	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnItemFavoriteChanged.RemoveDynamic(this, &USteamAsyncSetWorkshopFavorite::HandleFavoriteChanged);
	}
}

void USteamAsyncSetWorkshopFavorite::Complete(bool bSuccess)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(PublishedFileId, bAddToFavorites);
	}
	else
	{
		OnFailure.Broadcast(PublishedFileId, bAddToFavorites);
	}
}

// ---- USteamAsyncSetWorkshopItemVote ----

USteamAsyncSetWorkshopItemVote* USteamAsyncSetWorkshopItemVote::SetWorkshopItemVote(UObject* WorldContext, int64 PublishedFileId, bool bVoteUp, float Timeout)
{
	USteamAsyncSetWorkshopItemVote* Action = NewObject<USteamAsyncSetWorkshopItemVote>();
	Action->PublishedFileId = PublishedFileId;
	Action->bVoteUp = bVoteUp;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncSetWorkshopItemVote::ExecuteAction()
{
	UESteamUGCSubsystem* UGCSubsystem = RequireGameInstanceSubsystem<UESteamUGCSubsystem>();
	if (!UGCSubsystem)
	{
		return;
	}

	Subsystem = UGCSubsystem;
	UGCSubsystem->OnUserItemVoteSet.AddDynamic(this, &USteamAsyncSetWorkshopItemVote::HandleVoteSet);

	bool bStarted = false;
	CallWithContext([&] { bStarted = UGCSubsystem->SetUserItemVote(PublishedFileId, bVoteUp); });
	if (!bStarted)
	{
		Complete(false);
	}
}

void USteamAsyncSetWorkshopItemVote::HandleVoteSet(bool bSuccess, int64 InPublishedFileId, bool bInVoteUp)
{
	if (!IsResultForThisCall() || InPublishedFileId != PublishedFileId)
	{
		return;
	}
	Complete(bSuccess);
}

void USteamAsyncSetWorkshopItemVote::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(PublishedFileId, bVoteUp);
}

void USteamAsyncSetWorkshopItemVote::CancelWork()
{
	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnUserItemVoteSet.RemoveDynamic(this, &USteamAsyncSetWorkshopItemVote::HandleVoteSet);
	}
}

void USteamAsyncSetWorkshopItemVote::Complete(bool bSuccess)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(PublishedFileId, bVoteUp);
	}
	else
	{
		OnFailure.Broadcast(PublishedFileId, bVoteUp);
	}
}

// ---- USteamAsyncGetWorkshopItemVote ----

USteamAsyncGetWorkshopItemVote* USteamAsyncGetWorkshopItemVote::GetWorkshopItemVote(UObject* WorldContext, int64 PublishedFileId, float Timeout)
{
	USteamAsyncGetWorkshopItemVote* Action = NewObject<USteamAsyncGetWorkshopItemVote>();
	Action->PublishedFileId = PublishedFileId;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncGetWorkshopItemVote::ExecuteAction()
{
	UESteamUGCSubsystem* UGCSubsystem = RequireGameInstanceSubsystem<UESteamUGCSubsystem>();
	if (!UGCSubsystem)
	{
		return;
	}

	Subsystem = UGCSubsystem;
	UGCSubsystem->OnUserItemVote.AddDynamic(this, &USteamAsyncGetWorkshopItemVote::HandleVote);

	bool bStarted = false;
	CallWithContext([&] { bStarted = UGCSubsystem->GetUserItemVote(PublishedFileId); });
	if (!bStarted)
	{
		Complete(false, false, false, false);
	}
}

void USteamAsyncGetWorkshopItemVote::HandleVote(bool bSuccess, int64 InPublishedFileId, bool bVotedUp, bool bVotedDown, bool bVoteSkipped)
{
	if (!IsResultForThisCall() || InPublishedFileId != PublishedFileId)
	{
		return;
	}
	Complete(bSuccess, bVotedUp, bVotedDown, bVoteSkipped);
}

void USteamAsyncGetWorkshopItemVote::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(PublishedFileId, false, false, false);
}

void USteamAsyncGetWorkshopItemVote::CancelWork()
{
	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnUserItemVote.RemoveDynamic(this, &USteamAsyncGetWorkshopItemVote::HandleVote);
	}
}

void USteamAsyncGetWorkshopItemVote::Complete(bool bSuccess, bool bVotedUp, bool bVotedDown, bool bVoteSkipped)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(PublishedFileId, bVotedUp, bVotedDown, bVoteSkipped);
	}
	else
	{
		OnFailure.Broadcast(PublishedFileId, bVotedUp, bVotedDown, bVoteSkipped);
	}
}

// ---- USteamAsyncDeleteWorkshopItem ----

USteamAsyncDeleteWorkshopItem* USteamAsyncDeleteWorkshopItem::DeleteWorkshopItem(UObject* WorldContext, int64 PublishedFileId, float Timeout)
{
	USteamAsyncDeleteWorkshopItem* Action = NewObject<USteamAsyncDeleteWorkshopItem>();
	Action->PublishedFileId = PublishedFileId;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncDeleteWorkshopItem::ExecuteAction()
{
	UESteamUGCSubsystem* UGCSubsystem = RequireGameInstanceSubsystem<UESteamUGCSubsystem>();
	if (!UGCSubsystem)
	{
		return;
	}

	Subsystem = UGCSubsystem;
	UGCSubsystem->OnItemDeleted.AddDynamic(this, &USteamAsyncDeleteWorkshopItem::HandleItemDeleted);

	bool bStarted = false;
	CallWithContext([&] { bStarted = UGCSubsystem->DeleteItem(PublishedFileId); });
	if (!bStarted)
	{
		Complete(false);
	}
}

void USteamAsyncDeleteWorkshopItem::HandleItemDeleted(bool bSuccess, int64 InPublishedFileId)
{
	if (!IsResultForThisCall() || InPublishedFileId != PublishedFileId)
	{
		return;
	}
	Complete(bSuccess);
}

void USteamAsyncDeleteWorkshopItem::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(PublishedFileId);
}

void USteamAsyncDeleteWorkshopItem::CancelWork()
{
	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnItemDeleted.RemoveDynamic(this, &USteamAsyncDeleteWorkshopItem::HandleItemDeleted);
	}
}

void USteamAsyncDeleteWorkshopItem::Complete(bool bSuccess)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(PublishedFileId);
	}
	else
	{
		OnFailure.Broadcast(PublishedFileId);
	}
}

// ---- USteamAsyncSetWorkshopDependency ----

USteamAsyncSetWorkshopDependency* USteamAsyncSetWorkshopDependency::SetWorkshopDependency(UObject* WorldContext, int64 ParentPublishedFileId, int64 ChildPublishedFileId, bool bAdd, float Timeout)
{
	USteamAsyncSetWorkshopDependency* Action = NewObject<USteamAsyncSetWorkshopDependency>();
	Action->ParentPublishedFileId = ParentPublishedFileId;
	Action->ChildPublishedFileId = ChildPublishedFileId;
	Action->bAdd = bAdd;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncSetWorkshopDependency::ExecuteAction()
{
	UESteamUGCSubsystem* UGCSubsystem = RequireGameInstanceSubsystem<UESteamUGCSubsystem>();
	if (!UGCSubsystem)
	{
		return;
	}

	Subsystem = UGCSubsystem;
	if (bAdd)
	{
		UGCSubsystem->OnDependencyAdded.AddDynamic(this, &USteamAsyncSetWorkshopDependency::HandleDependencyResult);
	}
	else
	{
		UGCSubsystem->OnDependencyRemoved.AddDynamic(this, &USteamAsyncSetWorkshopDependency::HandleDependencyResult);
	}

	bool bIssued = false;
	CallWithContext([&]
	{
		bIssued = bAdd
			? UGCSubsystem->AddDependency(ParentPublishedFileId, ChildPublishedFileId)
			: UGCSubsystem->RemoveDependency(ParentPublishedFileId, ChildPublishedFileId);
	});
	if (!bIssued)
	{
		Complete(false);
	}
}

void USteamAsyncSetWorkshopDependency::HandleDependencyResult(bool bSuccess, int64 InParentPublishedFileId, int64 InChildPublishedFileId)
{
	if (!IsResultForThisCall() || InParentPublishedFileId != ParentPublishedFileId || InChildPublishedFileId != ChildPublishedFileId)
	{
		return;
	}
	Complete(bSuccess);
}

void USteamAsyncSetWorkshopDependency::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(ParentPublishedFileId, ChildPublishedFileId);
}

void USteamAsyncSetWorkshopDependency::CancelWork()
{
	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		if (bAdd)
		{
			UGCSubsystem->OnDependencyAdded.RemoveDynamic(this, &USteamAsyncSetWorkshopDependency::HandleDependencyResult);
		}
		else
		{
			UGCSubsystem->OnDependencyRemoved.RemoveDynamic(this, &USteamAsyncSetWorkshopDependency::HandleDependencyResult);
		}
	}
}

void USteamAsyncSetWorkshopDependency::Complete(bool bSuccess)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(ParentPublishedFileId, ChildPublishedFileId);
	}
	else
	{
		OnFailure.Broadcast(ParentPublishedFileId, ChildPublishedFileId);
	}
}

// ---- USteamAsyncSetWorkshopAppDependency ----

USteamAsyncSetWorkshopAppDependency* USteamAsyncSetWorkshopAppDependency::SetWorkshopAppDependency(UObject* WorldContext, int64 PublishedFileId, int32 AppId, bool bAdd, float Timeout)
{
	USteamAsyncSetWorkshopAppDependency* Action = NewObject<USteamAsyncSetWorkshopAppDependency>();
	Action->PublishedFileId = PublishedFileId;
	Action->AppId = AppId;
	Action->bAdd = bAdd;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncSetWorkshopAppDependency::ExecuteAction()
{
	UESteamUGCSubsystem* UGCSubsystem = RequireGameInstanceSubsystem<UESteamUGCSubsystem>();
	if (!UGCSubsystem)
	{
		return;
	}

	Subsystem = UGCSubsystem;
	if (bAdd)
	{
		UGCSubsystem->OnAppDependencyAdded.AddDynamic(this, &USteamAsyncSetWorkshopAppDependency::HandleAppDependencyResult);
	}
	else
	{
		UGCSubsystem->OnAppDependencyRemoved.AddDynamic(this, &USteamAsyncSetWorkshopAppDependency::HandleAppDependencyResult);
	}

	bool bIssued = false;
	CallWithContext([&]
	{
		bIssued = bAdd
			? UGCSubsystem->AddAppDependency(PublishedFileId, AppId)
			: UGCSubsystem->RemoveAppDependency(PublishedFileId, AppId);
	});
	if (!bIssued)
	{
		Complete(false);
	}
}

void USteamAsyncSetWorkshopAppDependency::HandleAppDependencyResult(bool bSuccess, int64 InPublishedFileId, int32 InAppId)
{
	if (!IsResultForThisCall() || InPublishedFileId != PublishedFileId || InAppId != AppId)
	{
		return;
	}
	Complete(bSuccess);
}

void USteamAsyncSetWorkshopAppDependency::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(PublishedFileId, AppId);
}

void USteamAsyncSetWorkshopAppDependency::CancelWork()
{
	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		if (bAdd)
		{
			UGCSubsystem->OnAppDependencyAdded.RemoveDynamic(this, &USteamAsyncSetWorkshopAppDependency::HandleAppDependencyResult);
		}
		else
		{
			UGCSubsystem->OnAppDependencyRemoved.RemoveDynamic(this, &USteamAsyncSetWorkshopAppDependency::HandleAppDependencyResult);
		}
	}
}

void USteamAsyncSetWorkshopAppDependency::Complete(bool bSuccess)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(PublishedFileId, AppId);
	}
	else
	{
		OnFailure.Broadcast(PublishedFileId, AppId);
	}
}

// ---- USteamAsyncGetWorkshopAppDependencies ----

USteamAsyncGetWorkshopAppDependencies* USteamAsyncGetWorkshopAppDependencies::GetWorkshopAppDependencies(UObject* WorldContext, int64 PublishedFileId, float Timeout)
{
	USteamAsyncGetWorkshopAppDependencies* Action = NewObject<USteamAsyncGetWorkshopAppDependencies>();
	Action->PublishedFileId = PublishedFileId;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncGetWorkshopAppDependencies::ExecuteAction()
{
	UESteamUGCSubsystem* UGCSubsystem = RequireGameInstanceSubsystem<UESteamUGCSubsystem>();
	if (!UGCSubsystem)
	{
		return;
	}

	Subsystem = UGCSubsystem;
	UGCSubsystem->OnAppDependenciesReceived.AddDynamic(this, &USteamAsyncGetWorkshopAppDependencies::HandleAppDependencies);

	bool bStarted = false;
	CallWithContext([&] { bStarted = UGCSubsystem->GetAppDependencies(PublishedFileId); });
	if (!bStarted)
	{
		Complete(false, TArray<int32>());
	}
}

void USteamAsyncGetWorkshopAppDependencies::HandleAppDependencies(bool bSuccess, int64 InPublishedFileId, const TArray<int32>& AppIds)
{
	if (!IsResultForThisCall() || InPublishedFileId != PublishedFileId)
	{
		return;
	}
	Complete(bSuccess, AppIds);
}

void USteamAsyncGetWorkshopAppDependencies::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(PublishedFileId, TArray<int32>());
}

void USteamAsyncGetWorkshopAppDependencies::CancelWork()
{
	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnAppDependenciesReceived.RemoveDynamic(this, &USteamAsyncGetWorkshopAppDependencies::HandleAppDependencies);
	}
}

void USteamAsyncGetWorkshopAppDependencies::Complete(bool bSuccess, const TArray<int32>& AppIds)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(PublishedFileId, AppIds);
	}
	else
	{
		OnFailure.Broadcast(PublishedFileId, AppIds);
	}
}

// ---- USteamAsyncStartPlaytimeTracking ----

USteamAsyncStartPlaytimeTracking* USteamAsyncStartPlaytimeTracking::StartPlaytimeTracking(UObject* WorldContext, const TArray<int64>& PublishedFileIds, float Timeout)
{
	USteamAsyncStartPlaytimeTracking* Action = NewObject<USteamAsyncStartPlaytimeTracking>();
	Action->PublishedFileIds = PublishedFileIds;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncStartPlaytimeTracking::ExecuteAction()
{
	UESteamUGCSubsystem* UGCSubsystem = RequireGameInstanceSubsystem<UESteamUGCSubsystem>();
	if (!UGCSubsystem)
	{
		return;
	}

	Subsystem = UGCSubsystem;
	UGCSubsystem->OnPlaytimeTrackingStarted.AddDynamic(this, &USteamAsyncStartPlaytimeTracking::HandleTrackingResult);

	bool bStarted = false;
	CallWithContext([&] { bStarted = UGCSubsystem->StartPlaytimeTracking(PublishedFileIds); });
	if (!bStarted)
	{
		Complete(false);
	}
}

void USteamAsyncStartPlaytimeTracking::HandleTrackingResult(bool bSuccess)
{
	// The result carries no ids; the subsystem tags it with the call context of its request.
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(bSuccess);
}

void USteamAsyncStartPlaytimeTracking::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast();
}

void USteamAsyncStartPlaytimeTracking::CancelWork()
{
	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnPlaytimeTrackingStarted.RemoveDynamic(this, &USteamAsyncStartPlaytimeTracking::HandleTrackingResult);
	}
}

void USteamAsyncStartPlaytimeTracking::Complete(bool bSuccess)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast();
	}
	else
	{
		OnFailure.Broadcast();
	}
}

// ---- USteamAsyncStopPlaytimeTracking ----

USteamAsyncStopPlaytimeTracking* USteamAsyncStopPlaytimeTracking::StopPlaytimeTracking(UObject* WorldContext, const TArray<int64>& PublishedFileIds, float Timeout)
{
	USteamAsyncStopPlaytimeTracking* Action = NewObject<USteamAsyncStopPlaytimeTracking>();
	Action->PublishedFileIds = PublishedFileIds;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncStopPlaytimeTracking::ExecuteAction()
{
	UESteamUGCSubsystem* UGCSubsystem = RequireGameInstanceSubsystem<UESteamUGCSubsystem>();
	if (!UGCSubsystem)
	{
		return;
	}

	Subsystem = UGCSubsystem;
	UGCSubsystem->OnPlaytimeTrackingStopped.AddDynamic(this, &USteamAsyncStopPlaytimeTracking::HandleTrackingResult);

	bool bStarted = false;
	CallWithContext([&] { bStarted = UGCSubsystem->StopPlaytimeTracking(PublishedFileIds); });
	if (!bStarted)
	{
		Complete(false);
	}
}

void USteamAsyncStopPlaytimeTracking::HandleTrackingResult(bool bSuccess)
{
	// StopPlaytimeTracking/ForAllItems share one subsystem delegate and the result carries no ids;
	// the subsystem tags each result with the call context of its request.
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(bSuccess);
}

void USteamAsyncStopPlaytimeTracking::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast();
}

void USteamAsyncStopPlaytimeTracking::CancelWork()
{
	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnPlaytimeTrackingStopped.RemoveDynamic(this, &USteamAsyncStopPlaytimeTracking::HandleTrackingResult);
	}
}

void USteamAsyncStopPlaytimeTracking::Complete(bool bSuccess)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast();
	}
	else
	{
		OnFailure.Broadcast();
	}
}

// ---- USteamAsyncStopPlaytimeTrackingForAllItems ----

USteamAsyncStopPlaytimeTrackingForAllItems* USteamAsyncStopPlaytimeTrackingForAllItems::StopPlaytimeTrackingForAllItems(UObject* WorldContext, float Timeout)
{
	USteamAsyncStopPlaytimeTrackingForAllItems* Action = NewObject<USteamAsyncStopPlaytimeTrackingForAllItems>();
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncStopPlaytimeTrackingForAllItems::ExecuteAction()
{
	UESteamUGCSubsystem* UGCSubsystem = RequireGameInstanceSubsystem<UESteamUGCSubsystem>();
	if (!UGCSubsystem)
	{
		return;
	}

	Subsystem = UGCSubsystem;
	UGCSubsystem->OnPlaytimeTrackingStopped.AddDynamic(this, &USteamAsyncStopPlaytimeTrackingForAllItems::HandleTrackingResult);

	bool bStarted = false;
	CallWithContext([&] { bStarted = UGCSubsystem->StopPlaytimeTrackingForAllItems(); });
	if (!bStarted)
	{
		Complete(false);
	}
}

void USteamAsyncStopPlaytimeTrackingForAllItems::HandleTrackingResult(bool bSuccess)
{
	// StopPlaytimeTracking/ForAllItems share one subsystem delegate and the result carries no ids;
	// the subsystem tags each result with the call context of its request.
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(bSuccess);
}

void USteamAsyncStopPlaytimeTrackingForAllItems::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast();
}

void USteamAsyncStopPlaytimeTrackingForAllItems::CancelWork()
{
	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnPlaytimeTrackingStopped.RemoveDynamic(this, &USteamAsyncStopPlaytimeTrackingForAllItems::HandleTrackingResult);
	}
}

void USteamAsyncStopPlaytimeTrackingForAllItems::Complete(bool bSuccess)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast();
	}
	else
	{
		OnFailure.Broadcast();
	}
}

// ---- USteamAsyncQueryUserWorkshopItems ----

USteamAsyncQueryUserWorkshopItems* USteamAsyncQueryUserWorkshopItems::QueryUserWorkshopItems(UObject* WorldContext, FESteamId User, EESteamUserUGCList ListType, EESteamUserUGCListSortOrder SortOrder, EESteamUGCMatchingType MatchingType, int32 Page, const FESteamUGCQueryConfig& Config, float Timeout)
{
	USteamAsyncQueryUserWorkshopItems* Action = NewObject<USteamAsyncQueryUserWorkshopItems>();
	Action->User = User;
	Action->ListType = ListType;
	Action->SortOrder = SortOrder;
	Action->MatchingType = MatchingType;
	Action->Page = Page;
	Action->Config = Config;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncQueryUserWorkshopItems::ExecuteAction()
{
	UESteamUGCSubsystem* UGCSubsystem = RequireGameInstanceSubsystem<UESteamUGCSubsystem>();
	if (!UGCSubsystem)
	{
		return;
	}

	Subsystem = UGCSubsystem;
	UGCSubsystem->OnQueryCompleted.AddDynamic(this, &USteamAsyncQueryUserWorkshopItems::HandleQueryCompleted);

	bool bStarted = false;
	CallWithContext([&] { bStarted = UGCSubsystem->QueryUserItems(User, ListType, SortOrder, MatchingType, Page, Config); });
	if (!bStarted)
	{
		Complete(false, TArray<FESteamUGCDetails>(), 0);
	}
}

void USteamAsyncQueryUserWorkshopItems::HandleQueryCompleted(bool bSuccess, const TArray<FESteamUGCDetails>& Results, int32 TotalMatchingResults)
{
	// FOnSteamUGCQueryCompleted carries no query handle; the subsystem tags each result with the
	// call context of its request.
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(bSuccess, Results, TotalMatchingResults);
}

void USteamAsyncQueryUserWorkshopItems::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(TArray<FESteamUGCDetails>(), 0);
}

void USteamAsyncQueryUserWorkshopItems::CancelWork()
{
	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnQueryCompleted.RemoveDynamic(this, &USteamAsyncQueryUserWorkshopItems::HandleQueryCompleted);
	}
}

void USteamAsyncQueryUserWorkshopItems::Complete(bool bSuccess, const TArray<FESteamUGCDetails>& Results, int32 TotalResults)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(Results, TotalResults);
	}
	else
	{
		OnFailure.Broadcast(Results, TotalResults);
	}
}

// ---- USteamAsyncQueryWorkshopItemsByIds ----

USteamAsyncQueryWorkshopItemsByIds* USteamAsyncQueryWorkshopItemsByIds::QueryWorkshopItemsByIds(UObject* WorldContext, const TArray<int64>& PublishedFileIds, float Timeout)
{
	USteamAsyncQueryWorkshopItemsByIds* Action = NewObject<USteamAsyncQueryWorkshopItemsByIds>();
	Action->PublishedFileIds = PublishedFileIds;
	Action->InitAsyncAction(WorldContext, Timeout);
	return Action;
}

void USteamAsyncQueryWorkshopItemsByIds::ExecuteAction()
{
	UESteamUGCSubsystem* UGCSubsystem = RequireGameInstanceSubsystem<UESteamUGCSubsystem>();
	if (!UGCSubsystem)
	{
		return;
	}

	Subsystem = UGCSubsystem;
	UGCSubsystem->OnQueryCompleted.AddDynamic(this, &USteamAsyncQueryWorkshopItemsByIds::HandleQueryCompleted);

	bool bStarted = false;
	CallWithContext([&] { bStarted = UGCSubsystem->QueryItemsByIds(PublishedFileIds); });
	if (!bStarted)
	{
		Complete(false, TArray<FESteamUGCDetails>(), 0);
	}
}

void USteamAsyncQueryWorkshopItemsByIds::HandleQueryCompleted(bool bSuccess, const TArray<FESteamUGCDetails>& Results, int32 TotalMatchingResults)
{
	// FOnSteamUGCQueryCompleted carries no query handle; the subsystem tags each result with the
	// call context of its request.
	if (!IsResultForThisCall())
	{
		return;
	}
	Complete(bSuccess, Results, TotalMatchingResults);
}

void USteamAsyncQueryWorkshopItemsByIds::BroadcastFailure(const FEFResult& /*Result*/)
{
	OnFailure.Broadcast(TArray<FESteamUGCDetails>(), 0);
}

void USteamAsyncQueryWorkshopItemsByIds::CancelWork()
{
	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnQueryCompleted.RemoveDynamic(this, &USteamAsyncQueryWorkshopItemsByIds::HandleQueryCompleted);
	}
}

void USteamAsyncQueryWorkshopItemsByIds::Complete(bool bSuccess, const TArray<FESteamUGCDetails>& Results, int32 TotalResults)
{
	CancelWork();
	if (!FinishAction())
	{
		return;
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(Results, TotalResults);
	}
	else
	{
		OnFailure.Broadcast(Results, TotalResults);
	}
}
