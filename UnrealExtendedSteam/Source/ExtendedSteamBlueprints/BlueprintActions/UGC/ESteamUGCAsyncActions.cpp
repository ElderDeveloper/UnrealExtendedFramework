// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "BlueprintActions/UGC/ESteamUGCAsyncActions.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace
{
	UESteamUGCSubsystem* GetUGCSubsystem(const UObject* WorldContext)
	{
		if (const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull) : nullptr)
		{
			if (const UGameInstance* GameInstance = World->GetGameInstance())
			{
				return GameInstance->GetSubsystem<UESteamUGCSubsystem>();
			}
		}
		return nullptr;
	}
}

// ---- USteamAsyncCreateWorkshopItem ----

USteamAsyncCreateWorkshopItem* USteamAsyncCreateWorkshopItem::CreateWorkshopItem(UObject* WorldContext, EESteamWorkshopFileType FileType, float Timeout)
{
	USteamAsyncCreateWorkshopItem* Action = NewObject<USteamAsyncCreateWorkshopItem>();
	Action->WorldContextObject = WorldContext;
	Action->Subsystem = GetUGCSubsystem(WorldContext);
	Action->FileType = FileType;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncCreateWorkshopItem::Activate()
{
	UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get();
	if (!UGCSubsystem)
	{
		Complete(false, 0, false);
		return;
	}

	UGCSubsystem->OnItemCreated.AddDynamic(this, &USteamAsyncCreateWorkshopItem::HandleItemCreated);

	if (!UGCSubsystem->CreateItem(FileType))
	{
		Complete(false, 0, false);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncCreateWorkshopItem::HandleItemCreated(bool bSuccess, int64 PublishedFileId, bool bNeedsLegalAgreement)
{
	// NOTE: FOnSteamUGCItemCreated's PublishedFileId is the result (unknown before the call), so it
	// cannot correlate concurrent creates. Relies on Timeout + one-in-flight subsystem limit.
	Complete(bSuccess, PublishedFileId, bNeedsLegalAgreement);
}

void USteamAsyncCreateWorkshopItem::OnTimeoutFailure()
{
	Complete(false, 0, false);
}

void USteamAsyncCreateWorkshopItem::Complete(bool bSuccess, int64 PublishedFileId, bool bNeedsLegalAgreement)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnItemCreated.RemoveDynamic(this, &USteamAsyncCreateWorkshopItem::HandleItemCreated);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(PublishedFileId, bNeedsLegalAgreement);
	}
	else
	{
		OnFailure.Broadcast(PublishedFileId, bNeedsLegalAgreement);
	}
	SetReadyToDestroy();
}

// ---- USteamAsyncSubmitWorkshopItem ----

USteamAsyncSubmitWorkshopItem* USteamAsyncSubmitWorkshopItem::SubmitWorkshopItem(UObject* WorldContext, int64 UpdateHandle, const FString& ChangeNote, float Timeout)
{
	USteamAsyncSubmitWorkshopItem* Action = NewObject<USteamAsyncSubmitWorkshopItem>();
	Action->WorldContextObject = WorldContext;
	Action->Subsystem = GetUGCSubsystem(WorldContext);
	Action->UpdateHandle = UpdateHandle;
	Action->ChangeNote = ChangeNote;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncSubmitWorkshopItem::Activate()
{
	UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get();
	if (!UGCSubsystem)
	{
		Complete(false, false);
		return;
	}

	UGCSubsystem->OnItemSubmitted.AddDynamic(this, &USteamAsyncSubmitWorkshopItem::HandleItemSubmitted);

	if (!UGCSubsystem->SubmitItemUpdate(UpdateHandle, ChangeNote))
	{
		Complete(false, false);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncSubmitWorkshopItem::HandleItemSubmitted(bool bSuccess, bool bNeedsLegalAgreement)
{
	// NOTE: FOnSteamUGCItemSubmitted carries no update handle; cannot discriminate.
	// Relies on Timeout + one-in-flight subsystem limit.
	Complete(bSuccess, bNeedsLegalAgreement);
}

void USteamAsyncSubmitWorkshopItem::OnTimeoutFailure()
{
	Complete(false, false);
}

void USteamAsyncSubmitWorkshopItem::Complete(bool bSuccess, bool bNeedsLegalAgreement)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnItemSubmitted.RemoveDynamic(this, &USteamAsyncSubmitWorkshopItem::HandleItemSubmitted);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(bNeedsLegalAgreement);
	}
	else
	{
		OnFailure.Broadcast(bNeedsLegalAgreement);
	}
	SetReadyToDestroy();
}

// ---- USteamAsyncQueryWorkshopItems ----

USteamAsyncQueryWorkshopItems* USteamAsyncQueryWorkshopItems::QueryWorkshopItems(UObject* WorldContext, EESteamUGCQueryType QueryType, EESteamUGCMatchingType MatchingType, int32 Page, float Timeout)
{
	USteamAsyncQueryWorkshopItems* Action = NewObject<USteamAsyncQueryWorkshopItems>();
	Action->WorldContextObject = WorldContext;
	Action->Subsystem = GetUGCSubsystem(WorldContext);
	Action->QueryType = QueryType;
	Action->MatchingType = MatchingType;
	Action->Page = Page;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncQueryWorkshopItems::Activate()
{
	UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get();
	if (!UGCSubsystem)
	{
		Complete(false, TArray<FESteamUGCDetails>(), 0);
		return;
	}

	UGCSubsystem->OnQueryCompleted.AddDynamic(this, &USteamAsyncQueryWorkshopItems::HandleQueryCompleted);

	if (!UGCSubsystem->QueryAllItems(QueryType, MatchingType, Page))
	{
		Complete(false, TArray<FESteamUGCDetails>(), 0);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncQueryWorkshopItems::HandleQueryCompleted(bool bSuccess, const TArray<FESteamUGCDetails>& Results, int32 TotalMatchingResults)
{
	// NOTE: FOnSteamUGCQueryCompleted carries no query handle; cannot discriminate.
	// Relies on Timeout + one-in-flight subsystem limit.
	Complete(bSuccess, Results, TotalMatchingResults);
}

void USteamAsyncQueryWorkshopItems::OnTimeoutFailure()
{
	Complete(false, TArray<FESteamUGCDetails>(), 0);
}

void USteamAsyncQueryWorkshopItems::Complete(bool bSuccess, const TArray<FESteamUGCDetails>& Results, int32 TotalResults)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnQueryCompleted.RemoveDynamic(this, &USteamAsyncQueryWorkshopItems::HandleQueryCompleted);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(Results, TotalResults);
	}
	else
	{
		OnFailure.Broadcast(Results, TotalResults);
	}
	SetReadyToDestroy();
}

// ---- USteamAsyncSubscribeWorkshopItem ----

USteamAsyncSubscribeWorkshopItem* USteamAsyncSubscribeWorkshopItem::SubscribeWorkshopItem(UObject* WorldContext, int64 PublishedFileId, float Timeout)
{
	USteamAsyncSubscribeWorkshopItem* Action = NewObject<USteamAsyncSubscribeWorkshopItem>();
	Action->WorldContextObject = WorldContext;
	Action->Subsystem = GetUGCSubsystem(WorldContext);
	Action->PublishedFileId = PublishedFileId;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncSubscribeWorkshopItem::Activate()
{
	UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get();
	if (!UGCSubsystem)
	{
		Complete(false);
		return;
	}

	UGCSubsystem->OnSubscribed.AddDynamic(this, &USteamAsyncSubscribeWorkshopItem::HandleSubscribed);

	if (!UGCSubsystem->SubscribeItem(PublishedFileId))
	{
		Complete(false);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncSubscribeWorkshopItem::HandleSubscribed(bool bSuccess, int64 InPublishedFileId)
{
	// Other requests may be in flight on the shared subsystem delegate; only react to ours.
	if (InPublishedFileId != PublishedFileId)
	{
		return;
	}
	Complete(bSuccess);
}

void USteamAsyncSubscribeWorkshopItem::OnTimeoutFailure()
{
	Complete(false);
}

void USteamAsyncSubscribeWorkshopItem::Complete(bool bSuccess)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnSubscribed.RemoveDynamic(this, &USteamAsyncSubscribeWorkshopItem::HandleSubscribed);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(PublishedFileId);
	}
	else
	{
		OnFailure.Broadcast(PublishedFileId);
	}
	SetReadyToDestroy();
}

// ---- USteamAsyncUnsubscribeWorkshopItem ----

USteamAsyncUnsubscribeWorkshopItem* USteamAsyncUnsubscribeWorkshopItem::UnsubscribeWorkshopItem(UObject* WorldContext, int64 PublishedFileId, float Timeout)
{
	USteamAsyncUnsubscribeWorkshopItem* Action = NewObject<USteamAsyncUnsubscribeWorkshopItem>();
	Action->WorldContextObject = WorldContext;
	Action->Subsystem = GetUGCSubsystem(WorldContext);
	Action->PublishedFileId = PublishedFileId;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncUnsubscribeWorkshopItem::Activate()
{
	UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get();
	if (!UGCSubsystem)
	{
		Complete(false);
		return;
	}

	UGCSubsystem->OnUnsubscribed.AddDynamic(this, &USteamAsyncUnsubscribeWorkshopItem::HandleUnsubscribed);

	if (!UGCSubsystem->UnsubscribeItem(PublishedFileId))
	{
		Complete(false);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncUnsubscribeWorkshopItem::HandleUnsubscribed(bool bSuccess, int64 InPublishedFileId)
{
	if (InPublishedFileId != PublishedFileId)
	{
		return;
	}
	Complete(bSuccess);
}

void USteamAsyncUnsubscribeWorkshopItem::OnTimeoutFailure()
{
	Complete(false);
}

void USteamAsyncUnsubscribeWorkshopItem::Complete(bool bSuccess)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnUnsubscribed.RemoveDynamic(this, &USteamAsyncUnsubscribeWorkshopItem::HandleUnsubscribed);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(PublishedFileId);
	}
	else
	{
		OnFailure.Broadcast(PublishedFileId);
	}
	SetReadyToDestroy();
}

// ---- USteamAsyncDownloadWorkshopItem ----

USteamAsyncDownloadWorkshopItem* USteamAsyncDownloadWorkshopItem::DownloadWorkshopItem(UObject* WorldContext, int64 PublishedFileId, bool bHighPriority, float Timeout)
{
	USteamAsyncDownloadWorkshopItem* Action = NewObject<USteamAsyncDownloadWorkshopItem>();
	Action->WorldContextObject = WorldContext;
	Action->Subsystem = GetUGCSubsystem(WorldContext);
	Action->PublishedFileId = PublishedFileId;
	Action->bHighPriority = bHighPriority;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncDownloadWorkshopItem::Activate()
{
	UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get();
	if (!UGCSubsystem)
	{
		Complete(false);
		return;
	}

	UGCSubsystem->OnItemDownloaded.AddDynamic(this, &USteamAsyncDownloadWorkshopItem::HandleItemDownloaded);

	if (!UGCSubsystem->DownloadItem(PublishedFileId, bHighPriority))
	{
		Complete(false);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncDownloadWorkshopItem::HandleItemDownloaded(bool bSuccess, int64 InPublishedFileId)
{
	// The download callback fires for every item this client downloads; only react to ours.
	if (InPublishedFileId != PublishedFileId)
	{
		return;
	}
	Complete(bSuccess);
}

void USteamAsyncDownloadWorkshopItem::OnTimeoutFailure()
{
	Complete(false);
}

void USteamAsyncDownloadWorkshopItem::Complete(bool bSuccess)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnItemDownloaded.RemoveDynamic(this, &USteamAsyncDownloadWorkshopItem::HandleItemDownloaded);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(PublishedFileId);
	}
	else
	{
		OnFailure.Broadcast(PublishedFileId);
	}
	SetReadyToDestroy();
}

// ---- USteamAsyncSetWorkshopFavorite ----

USteamAsyncSetWorkshopFavorite* USteamAsyncSetWorkshopFavorite::SetWorkshopFavorite(UObject* WorldContext, int32 AppId, int64 PublishedFileId, bool bAddToFavorites, float Timeout)
{
	USteamAsyncSetWorkshopFavorite* Action = NewObject<USteamAsyncSetWorkshopFavorite>();
	Action->WorldContextObject = WorldContext;
	Action->Subsystem = GetUGCSubsystem(WorldContext);
	Action->AppId = AppId;
	Action->PublishedFileId = PublishedFileId;
	Action->bAddToFavorites = bAddToFavorites;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncSetWorkshopFavorite::Activate()
{
	UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get();
	if (!UGCSubsystem)
	{
		Complete(false);
		return;
	}

	UGCSubsystem->OnItemFavoriteChanged.AddDynamic(this, &USteamAsyncSetWorkshopFavorite::HandleFavoriteChanged);

	const bool bIssued = bAddToFavorites
		? UGCSubsystem->AddItemToFavorites(AppId, PublishedFileId)
		: UGCSubsystem->RemoveItemFromFavorites(AppId, PublishedFileId);
	if (!bIssued)
	{
		Complete(false);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncSetWorkshopFavorite::HandleFavoriteChanged(bool bSuccess, int64 InPublishedFileId, bool bWasAddRequest)
{
	// Favorites add/remove share one subsystem delegate; only react to our item and our direction.
	if (InPublishedFileId != PublishedFileId || bWasAddRequest != bAddToFavorites)
	{
		return;
	}
	Complete(bSuccess);
}

void USteamAsyncSetWorkshopFavorite::OnTimeoutFailure()
{
	Complete(false);
}

void USteamAsyncSetWorkshopFavorite::Complete(bool bSuccess)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnItemFavoriteChanged.RemoveDynamic(this, &USteamAsyncSetWorkshopFavorite::HandleFavoriteChanged);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(PublishedFileId, bAddToFavorites);
	}
	else
	{
		OnFailure.Broadcast(PublishedFileId, bAddToFavorites);
	}
	SetReadyToDestroy();
}

// ---- USteamAsyncSetWorkshopItemVote ----

USteamAsyncSetWorkshopItemVote* USteamAsyncSetWorkshopItemVote::SetWorkshopItemVote(UObject* WorldContext, int64 PublishedFileId, bool bVoteUp, float Timeout)
{
	USteamAsyncSetWorkshopItemVote* Action = NewObject<USteamAsyncSetWorkshopItemVote>();
	Action->WorldContextObject = WorldContext;
	Action->Subsystem = GetUGCSubsystem(WorldContext);
	Action->PublishedFileId = PublishedFileId;
	Action->bVoteUp = bVoteUp;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncSetWorkshopItemVote::Activate()
{
	UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get();
	if (!UGCSubsystem)
	{
		Complete(false);
		return;
	}

	UGCSubsystem->OnUserItemVoteSet.AddDynamic(this, &USteamAsyncSetWorkshopItemVote::HandleVoteSet);

	if (!UGCSubsystem->SetUserItemVote(PublishedFileId, bVoteUp))
	{
		Complete(false);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncSetWorkshopItemVote::HandleVoteSet(bool bSuccess, int64 InPublishedFileId, bool bInVoteUp)
{
	if (InPublishedFileId != PublishedFileId)
	{
		return;
	}
	Complete(bSuccess);
}

void USteamAsyncSetWorkshopItemVote::OnTimeoutFailure()
{
	Complete(false);
}

void USteamAsyncSetWorkshopItemVote::Complete(bool bSuccess)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnUserItemVoteSet.RemoveDynamic(this, &USteamAsyncSetWorkshopItemVote::HandleVoteSet);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(PublishedFileId, bVoteUp);
	}
	else
	{
		OnFailure.Broadcast(PublishedFileId, bVoteUp);
	}
	SetReadyToDestroy();
}

// ---- USteamAsyncGetWorkshopItemVote ----

USteamAsyncGetWorkshopItemVote* USteamAsyncGetWorkshopItemVote::GetWorkshopItemVote(UObject* WorldContext, int64 PublishedFileId, float Timeout)
{
	USteamAsyncGetWorkshopItemVote* Action = NewObject<USteamAsyncGetWorkshopItemVote>();
	Action->WorldContextObject = WorldContext;
	Action->Subsystem = GetUGCSubsystem(WorldContext);
	Action->PublishedFileId = PublishedFileId;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncGetWorkshopItemVote::Activate()
{
	UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get();
	if (!UGCSubsystem)
	{
		Complete(false, false, false, false);
		return;
	}

	UGCSubsystem->OnUserItemVote.AddDynamic(this, &USteamAsyncGetWorkshopItemVote::HandleVote);

	if (!UGCSubsystem->GetUserItemVote(PublishedFileId))
	{
		Complete(false, false, false, false);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncGetWorkshopItemVote::HandleVote(bool bSuccess, int64 InPublishedFileId, bool bVotedUp, bool bVotedDown, bool bVoteSkipped)
{
	if (InPublishedFileId != PublishedFileId)
	{
		return;
	}
	Complete(bSuccess, bVotedUp, bVotedDown, bVoteSkipped);
}

void USteamAsyncGetWorkshopItemVote::OnTimeoutFailure()
{
	Complete(false, false, false, false);
}

void USteamAsyncGetWorkshopItemVote::Complete(bool bSuccess, bool bVotedUp, bool bVotedDown, bool bVoteSkipped)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnUserItemVote.RemoveDynamic(this, &USteamAsyncGetWorkshopItemVote::HandleVote);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(PublishedFileId, bVotedUp, bVotedDown, bVoteSkipped);
	}
	else
	{
		OnFailure.Broadcast(PublishedFileId, bVotedUp, bVotedDown, bVoteSkipped);
	}
	SetReadyToDestroy();
}

// ---- USteamAsyncDeleteWorkshopItem ----

USteamAsyncDeleteWorkshopItem* USteamAsyncDeleteWorkshopItem::DeleteWorkshopItem(UObject* WorldContext, int64 PublishedFileId, float Timeout)
{
	USteamAsyncDeleteWorkshopItem* Action = NewObject<USteamAsyncDeleteWorkshopItem>();
	Action->WorldContextObject = WorldContext;
	Action->Subsystem = GetUGCSubsystem(WorldContext);
	Action->PublishedFileId = PublishedFileId;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncDeleteWorkshopItem::Activate()
{
	UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get();
	if (!UGCSubsystem)
	{
		Complete(false);
		return;
	}

	UGCSubsystem->OnItemDeleted.AddDynamic(this, &USteamAsyncDeleteWorkshopItem::HandleItemDeleted);

	if (!UGCSubsystem->DeleteItem(PublishedFileId))
	{
		Complete(false);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncDeleteWorkshopItem::HandleItemDeleted(bool bSuccess, int64 InPublishedFileId)
{
	if (InPublishedFileId != PublishedFileId)
	{
		return;
	}
	Complete(bSuccess);
}

void USteamAsyncDeleteWorkshopItem::OnTimeoutFailure()
{
	Complete(false);
}

void USteamAsyncDeleteWorkshopItem::Complete(bool bSuccess)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnItemDeleted.RemoveDynamic(this, &USteamAsyncDeleteWorkshopItem::HandleItemDeleted);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(PublishedFileId);
	}
	else
	{
		OnFailure.Broadcast(PublishedFileId);
	}
	SetReadyToDestroy();
}

// ---- USteamAsyncSetWorkshopDependency ----

USteamAsyncSetWorkshopDependency* USteamAsyncSetWorkshopDependency::SetWorkshopDependency(UObject* WorldContext, int64 ParentPublishedFileId, int64 ChildPublishedFileId, bool bAdd, float Timeout)
{
	USteamAsyncSetWorkshopDependency* Action = NewObject<USteamAsyncSetWorkshopDependency>();
	Action->WorldContextObject = WorldContext;
	Action->Subsystem = GetUGCSubsystem(WorldContext);
	Action->ParentPublishedFileId = ParentPublishedFileId;
	Action->ChildPublishedFileId = ChildPublishedFileId;
	Action->bAdd = bAdd;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncSetWorkshopDependency::Activate()
{
	UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get();
	if (!UGCSubsystem)
	{
		Complete(false);
		return;
	}

	if (bAdd)
	{
		UGCSubsystem->OnDependencyAdded.AddDynamic(this, &USteamAsyncSetWorkshopDependency::HandleDependencyResult);
	}
	else
	{
		UGCSubsystem->OnDependencyRemoved.AddDynamic(this, &USteamAsyncSetWorkshopDependency::HandleDependencyResult);
	}

	const bool bIssued = bAdd
		? UGCSubsystem->AddDependency(ParentPublishedFileId, ChildPublishedFileId)
		: UGCSubsystem->RemoveDependency(ParentPublishedFileId, ChildPublishedFileId);
	if (!bIssued)
	{
		Complete(false);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncSetWorkshopDependency::HandleDependencyResult(bool bSuccess, int64 InParentPublishedFileId, int64 InChildPublishedFileId)
{
	if (InParentPublishedFileId != ParentPublishedFileId || InChildPublishedFileId != ChildPublishedFileId)
	{
		return;
	}
	Complete(bSuccess);
}

void USteamAsyncSetWorkshopDependency::OnTimeoutFailure()
{
	Complete(false);
}

void USteamAsyncSetWorkshopDependency::Complete(bool bSuccess)
{
	if (!BeginComplete())
	{
		return;
	}

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

	if (bSuccess)
	{
		OnSuccess.Broadcast(ParentPublishedFileId, ChildPublishedFileId);
	}
	else
	{
		OnFailure.Broadcast(ParentPublishedFileId, ChildPublishedFileId);
	}
	SetReadyToDestroy();
}

// ---- USteamAsyncSetWorkshopAppDependency ----

USteamAsyncSetWorkshopAppDependency* USteamAsyncSetWorkshopAppDependency::SetWorkshopAppDependency(UObject* WorldContext, int64 PublishedFileId, int32 AppId, bool bAdd, float Timeout)
{
	USteamAsyncSetWorkshopAppDependency* Action = NewObject<USteamAsyncSetWorkshopAppDependency>();
	Action->WorldContextObject = WorldContext;
	Action->Subsystem = GetUGCSubsystem(WorldContext);
	Action->PublishedFileId = PublishedFileId;
	Action->AppId = AppId;
	Action->bAdd = bAdd;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncSetWorkshopAppDependency::Activate()
{
	UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get();
	if (!UGCSubsystem)
	{
		Complete(false);
		return;
	}

	if (bAdd)
	{
		UGCSubsystem->OnAppDependencyAdded.AddDynamic(this, &USteamAsyncSetWorkshopAppDependency::HandleAppDependencyResult);
	}
	else
	{
		UGCSubsystem->OnAppDependencyRemoved.AddDynamic(this, &USteamAsyncSetWorkshopAppDependency::HandleAppDependencyResult);
	}

	const bool bIssued = bAdd
		? UGCSubsystem->AddAppDependency(PublishedFileId, AppId)
		: UGCSubsystem->RemoveAppDependency(PublishedFileId, AppId);
	if (!bIssued)
	{
		Complete(false);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncSetWorkshopAppDependency::HandleAppDependencyResult(bool bSuccess, int64 InPublishedFileId, int32 InAppId)
{
	if (InPublishedFileId != PublishedFileId || InAppId != AppId)
	{
		return;
	}
	Complete(bSuccess);
}

void USteamAsyncSetWorkshopAppDependency::OnTimeoutFailure()
{
	Complete(false);
}

void USteamAsyncSetWorkshopAppDependency::Complete(bool bSuccess)
{
	if (!BeginComplete())
	{
		return;
	}

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

	if (bSuccess)
	{
		OnSuccess.Broadcast(PublishedFileId, AppId);
	}
	else
	{
		OnFailure.Broadcast(PublishedFileId, AppId);
	}
	SetReadyToDestroy();
}

// ---- USteamAsyncGetWorkshopAppDependencies ----

USteamAsyncGetWorkshopAppDependencies* USteamAsyncGetWorkshopAppDependencies::GetWorkshopAppDependencies(UObject* WorldContext, int64 PublishedFileId, float Timeout)
{
	USteamAsyncGetWorkshopAppDependencies* Action = NewObject<USteamAsyncGetWorkshopAppDependencies>();
	Action->WorldContextObject = WorldContext;
	Action->Subsystem = GetUGCSubsystem(WorldContext);
	Action->PublishedFileId = PublishedFileId;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncGetWorkshopAppDependencies::Activate()
{
	UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get();
	if (!UGCSubsystem)
	{
		Complete(false, TArray<int32>());
		return;
	}

	UGCSubsystem->OnAppDependenciesReceived.AddDynamic(this, &USteamAsyncGetWorkshopAppDependencies::HandleAppDependencies);

	if (!UGCSubsystem->GetAppDependencies(PublishedFileId))
	{
		Complete(false, TArray<int32>());
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncGetWorkshopAppDependencies::HandleAppDependencies(bool bSuccess, int64 InPublishedFileId, const TArray<int32>& AppIds)
{
	if (InPublishedFileId != PublishedFileId)
	{
		return;
	}
	Complete(bSuccess, AppIds);
}

void USteamAsyncGetWorkshopAppDependencies::OnTimeoutFailure()
{
	Complete(false, TArray<int32>());
}

void USteamAsyncGetWorkshopAppDependencies::Complete(bool bSuccess, const TArray<int32>& AppIds)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnAppDependenciesReceived.RemoveDynamic(this, &USteamAsyncGetWorkshopAppDependencies::HandleAppDependencies);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(PublishedFileId, AppIds);
	}
	else
	{
		OnFailure.Broadcast(PublishedFileId, AppIds);
	}
	SetReadyToDestroy();
}

// ---- USteamAsyncStartPlaytimeTracking ----

USteamAsyncStartPlaytimeTracking* USteamAsyncStartPlaytimeTracking::StartPlaytimeTracking(UObject* WorldContext, const TArray<int64>& PublishedFileIds, float Timeout)
{
	USteamAsyncStartPlaytimeTracking* Action = NewObject<USteamAsyncStartPlaytimeTracking>();
	Action->WorldContextObject = WorldContext;
	Action->Subsystem = GetUGCSubsystem(WorldContext);
	Action->PublishedFileIds = PublishedFileIds;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncStartPlaytimeTracking::Activate()
{
	UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get();
	if (!UGCSubsystem)
	{
		Complete(false);
		return;
	}

	UGCSubsystem->OnPlaytimeTrackingStarted.AddDynamic(this, &USteamAsyncStartPlaytimeTracking::HandleTrackingResult);

	if (!UGCSubsystem->StartPlaytimeTracking(PublishedFileIds))
	{
		Complete(false);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncStartPlaytimeTracking::HandleTrackingResult(bool bSuccess)
{
	// The result carries no ids to discriminate; relies on Timeout + one-in-flight subsystem limit.
	Complete(bSuccess);
}

void USteamAsyncStartPlaytimeTracking::OnTimeoutFailure()
{
	Complete(false);
}

void USteamAsyncStartPlaytimeTracking::Complete(bool bSuccess)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnPlaytimeTrackingStarted.RemoveDynamic(this, &USteamAsyncStartPlaytimeTracking::HandleTrackingResult);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast();
	}
	else
	{
		OnFailure.Broadcast();
	}
	SetReadyToDestroy();
}

// ---- USteamAsyncStopPlaytimeTracking ----

USteamAsyncStopPlaytimeTracking* USteamAsyncStopPlaytimeTracking::StopPlaytimeTracking(UObject* WorldContext, const TArray<int64>& PublishedFileIds, float Timeout)
{
	USteamAsyncStopPlaytimeTracking* Action = NewObject<USteamAsyncStopPlaytimeTracking>();
	Action->WorldContextObject = WorldContext;
	Action->Subsystem = GetUGCSubsystem(WorldContext);
	Action->PublishedFileIds = PublishedFileIds;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncStopPlaytimeTracking::Activate()
{
	UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get();
	if (!UGCSubsystem)
	{
		Complete(false);
		return;
	}

	UGCSubsystem->OnPlaytimeTrackingStopped.AddDynamic(this, &USteamAsyncStopPlaytimeTracking::HandleTrackingResult);

	if (!UGCSubsystem->StopPlaytimeTracking(PublishedFileIds))
	{
		Complete(false);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncStopPlaytimeTracking::HandleTrackingResult(bool bSuccess)
{
	// StopPlaytimeTracking/ForAllItems share one subsystem delegate and the result carries no ids;
	// relies on Timeout + one-in-flight subsystem limit.
	Complete(bSuccess);
}

void USteamAsyncStopPlaytimeTracking::OnTimeoutFailure()
{
	Complete(false);
}

void USteamAsyncStopPlaytimeTracking::Complete(bool bSuccess)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnPlaytimeTrackingStopped.RemoveDynamic(this, &USteamAsyncStopPlaytimeTracking::HandleTrackingResult);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast();
	}
	else
	{
		OnFailure.Broadcast();
	}
	SetReadyToDestroy();
}

// ---- USteamAsyncStopPlaytimeTrackingForAllItems ----

USteamAsyncStopPlaytimeTrackingForAllItems* USteamAsyncStopPlaytimeTrackingForAllItems::StopPlaytimeTrackingForAllItems(UObject* WorldContext, float Timeout)
{
	USteamAsyncStopPlaytimeTrackingForAllItems* Action = NewObject<USteamAsyncStopPlaytimeTrackingForAllItems>();
	Action->WorldContextObject = WorldContext;
	Action->Subsystem = GetUGCSubsystem(WorldContext);
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncStopPlaytimeTrackingForAllItems::Activate()
{
	UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get();
	if (!UGCSubsystem)
	{
		Complete(false);
		return;
	}

	UGCSubsystem->OnPlaytimeTrackingStopped.AddDynamic(this, &USteamAsyncStopPlaytimeTrackingForAllItems::HandleTrackingResult);

	if (!UGCSubsystem->StopPlaytimeTrackingForAllItems())
	{
		Complete(false);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncStopPlaytimeTrackingForAllItems::HandleTrackingResult(bool bSuccess)
{
	// StopPlaytimeTracking/ForAllItems share one subsystem delegate and the result carries no ids;
	// relies on Timeout + one-in-flight subsystem limit.
	Complete(bSuccess);
}

void USteamAsyncStopPlaytimeTrackingForAllItems::OnTimeoutFailure()
{
	Complete(false);
}

void USteamAsyncStopPlaytimeTrackingForAllItems::Complete(bool bSuccess)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnPlaytimeTrackingStopped.RemoveDynamic(this, &USteamAsyncStopPlaytimeTrackingForAllItems::HandleTrackingResult);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast();
	}
	else
	{
		OnFailure.Broadcast();
	}
	SetReadyToDestroy();
}

// ---- USteamAsyncQueryUserWorkshopItems ----

USteamAsyncQueryUserWorkshopItems* USteamAsyncQueryUserWorkshopItems::QueryUserWorkshopItems(UObject* WorldContext, FESteamId User, EESteamUserUGCList ListType, EESteamUserUGCListSortOrder SortOrder, EESteamUGCMatchingType MatchingType, int32 Page, const FESteamUGCQueryConfig& Config, float Timeout)
{
	USteamAsyncQueryUserWorkshopItems* Action = NewObject<USteamAsyncQueryUserWorkshopItems>();
	Action->WorldContextObject = WorldContext;
	Action->Subsystem = GetUGCSubsystem(WorldContext);
	Action->User = User;
	Action->ListType = ListType;
	Action->SortOrder = SortOrder;
	Action->MatchingType = MatchingType;
	Action->Page = Page;
	Action->Config = Config;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncQueryUserWorkshopItems::Activate()
{
	UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get();
	if (!UGCSubsystem)
	{
		Complete(false, TArray<FESteamUGCDetails>(), 0);
		return;
	}

	UGCSubsystem->OnQueryCompleted.AddDynamic(this, &USteamAsyncQueryUserWorkshopItems::HandleQueryCompleted);

	if (!UGCSubsystem->QueryUserItems(User, ListType, SortOrder, MatchingType, Page, Config))
	{
		Complete(false, TArray<FESteamUGCDetails>(), 0);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncQueryUserWorkshopItems::HandleQueryCompleted(bool bSuccess, const TArray<FESteamUGCDetails>& Results, int32 TotalMatchingResults)
{
	// FOnSteamUGCQueryCompleted carries no query handle; cannot discriminate.
	// Relies on Timeout + one-in-flight subsystem limit.
	Complete(bSuccess, Results, TotalMatchingResults);
}

void USteamAsyncQueryUserWorkshopItems::OnTimeoutFailure()
{
	Complete(false, TArray<FESteamUGCDetails>(), 0);
}

void USteamAsyncQueryUserWorkshopItems::Complete(bool bSuccess, const TArray<FESteamUGCDetails>& Results, int32 TotalResults)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnQueryCompleted.RemoveDynamic(this, &USteamAsyncQueryUserWorkshopItems::HandleQueryCompleted);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(Results, TotalResults);
	}
	else
	{
		OnFailure.Broadcast(Results, TotalResults);
	}
	SetReadyToDestroy();
}

// ---- USteamAsyncQueryWorkshopItemsByIds ----

USteamAsyncQueryWorkshopItemsByIds* USteamAsyncQueryWorkshopItemsByIds::QueryWorkshopItemsByIds(UObject* WorldContext, const TArray<int64>& PublishedFileIds, float Timeout)
{
	USteamAsyncQueryWorkshopItemsByIds* Action = NewObject<USteamAsyncQueryWorkshopItemsByIds>();
	Action->WorldContextObject = WorldContext;
	Action->Subsystem = GetUGCSubsystem(WorldContext);
	Action->PublishedFileIds = PublishedFileIds;
	Action->Timeout = Timeout;
	Action->RegisterWithGameInstance(WorldContext);
	return Action;
}

void USteamAsyncQueryWorkshopItemsByIds::Activate()
{
	UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get();
	if (!UGCSubsystem)
	{
		Complete(false, TArray<FESteamUGCDetails>(), 0);
		return;
	}

	UGCSubsystem->OnQueryCompleted.AddDynamic(this, &USteamAsyncQueryWorkshopItemsByIds::HandleQueryCompleted);

	if (!UGCSubsystem->QueryItemsByIds(PublishedFileIds))
	{
		Complete(false, TArray<FESteamUGCDetails>(), 0);
		return;
	}

	// Fail the node if the subsystem delegate never fires.
	ArmTimeout(Timeout);
}

void USteamAsyncQueryWorkshopItemsByIds::HandleQueryCompleted(bool bSuccess, const TArray<FESteamUGCDetails>& Results, int32 TotalMatchingResults)
{
	// FOnSteamUGCQueryCompleted carries no query handle; cannot discriminate.
	// Relies on Timeout + one-in-flight subsystem limit.
	Complete(bSuccess, Results, TotalMatchingResults);
}

void USteamAsyncQueryWorkshopItemsByIds::OnTimeoutFailure()
{
	Complete(false, TArray<FESteamUGCDetails>(), 0);
}

void USteamAsyncQueryWorkshopItemsByIds::Complete(bool bSuccess, const TArray<FESteamUGCDetails>& Results, int32 TotalResults)
{
	if (!BeginComplete())
	{
		return;
	}

	if (UESteamUGCSubsystem* UGCSubsystem = Subsystem.Get())
	{
		UGCSubsystem->OnQueryCompleted.RemoveDynamic(this, &USteamAsyncQueryWorkshopItemsByIds::HandleQueryCompleted);
	}

	if (bSuccess)
	{
		OnSuccess.Broadcast(Results, TotalResults);
	}
	else
	{
		OnFailure.Broadcast(Results, TotalResults);
	}
	SetReadyToDestroy();
}
