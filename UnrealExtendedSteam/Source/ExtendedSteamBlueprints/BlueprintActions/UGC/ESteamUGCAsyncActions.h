// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BlueprintActions/ESteamAsyncActionBase.h"
#include "UGC/ESteamUGCSubsystem.h"
#include "ESteamUGCAsyncActions.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSteamAsyncCreateWorkshopItemPin, int64, PublishedFileId, bool, bNeedsLegalAgreement);

/** Creates a new Workshop item and completes with its published file id. */
UCLASS()
class USteamAsyncCreateWorkshopItem : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Creates a new Workshop item with no content attached yet. When bNeedsLegalAgreement
	 * is set the user must accept the Workshop legal agreement before the item is visible.
	 * @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|UGC",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Create Workshop Item"))
	static USteamAsyncCreateWorkshopItem* CreateWorkshopItem(UObject* WorldContext, EESteamWorkshopFileType FileType, float Timeout = 10.0f);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncCreateWorkshopItemPin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncCreateWorkshopItemPin OnFailure;

private:
	UFUNCTION()
	void HandleItemCreated(bool bSuccess, int64 PublishedFileId, bool bNeedsLegalAgreement);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, int64 PublishedFileId, bool bNeedsLegalAgreement);

	TWeakObjectPtr<UESteamUGCSubsystem> Subsystem;
	EESteamWorkshopFileType FileType = EESteamWorkshopFileType::Community;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamAsyncSubmitWorkshopItemPin, bool, bNeedsLegalAgreement);

/** Submits an item update previously prepared via the UGC subsystem's StartItemUpdate/SetItem* calls. */
UCLASS()
class USteamAsyncSubmitWorkshopItem : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|UGC",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Submit Workshop Item"))
	static USteamAsyncSubmitWorkshopItem* SubmitWorkshopItem(UObject* WorldContext, int64 UpdateHandle, const FString& ChangeNote, float Timeout = 10.0f);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncSubmitWorkshopItemPin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncSubmitWorkshopItemPin OnFailure;

private:
	UFUNCTION()
	void HandleItemSubmitted(bool bSuccess, bool bNeedsLegalAgreement);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, bool bNeedsLegalAgreement);

	TWeakObjectPtr<UESteamUGCSubsystem> Subsystem;
	int64 UpdateHandle = 0;
	FString ChangeNote;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSteamAsyncQueryWorkshopItemsPin, const TArray<FESteamUGCDetails>&, Results, int32, TotalResults);

/** Queries all Workshop items of this app (one page of up to 50 results). */
UCLASS()
class USteamAsyncQueryWorkshopItems : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Queries all Workshop items of this app, sorted by QueryType. Page starts at 1.
	 * @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|UGC",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Query Workshop Items"))
	static USteamAsyncQueryWorkshopItems* QueryWorkshopItems(UObject* WorldContext, EESteamUGCQueryType QueryType, EESteamUGCMatchingType MatchingType, int32 Page = 1, float Timeout = 10.0f);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncQueryWorkshopItemsPin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncQueryWorkshopItemsPin OnFailure;

private:
	UFUNCTION()
	void HandleQueryCompleted(bool bSuccess, const TArray<FESteamUGCDetails>& Results, int32 TotalMatchingResults);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, const TArray<FESteamUGCDetails>& Results, int32 TotalResults);

	TWeakObjectPtr<UESteamUGCSubsystem> Subsystem;
	EESteamUGCQueryType QueryType = EESteamUGCQueryType::RankedByPublicationDate;
	EESteamUGCMatchingType MatchingType = EESteamUGCMatchingType::Items;
	int32 Page = 1;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamAsyncWorkshopSubscriptionPin, int64, PublishedFileId);

/** Subscribes to a Workshop item; it will be downloaded and installed ASAP. */
UCLASS()
class USteamAsyncSubscribeWorkshopItem : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|UGC",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Subscribe Workshop Item"))
	static USteamAsyncSubscribeWorkshopItem* SubscribeWorkshopItem(UObject* WorldContext, int64 PublishedFileId, float Timeout = 10.0f);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncWorkshopSubscriptionPin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncWorkshopSubscriptionPin OnFailure;

private:
	UFUNCTION()
	void HandleSubscribed(bool bSuccess, int64 InPublishedFileId);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess);

	TWeakObjectPtr<UESteamUGCSubsystem> Subsystem;
	int64 PublishedFileId = 0;
};

/** Unsubscribes from a Workshop item; it is uninstalled after the game quits. */
UCLASS()
class USteamAsyncUnsubscribeWorkshopItem : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|UGC",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Unsubscribe Workshop Item"))
	static USteamAsyncUnsubscribeWorkshopItem* UnsubscribeWorkshopItem(UObject* WorldContext, int64 PublishedFileId, float Timeout = 10.0f);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncWorkshopSubscriptionPin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncWorkshopSubscriptionPin OnFailure;

private:
	UFUNCTION()
	void HandleUnsubscribed(bool bSuccess, int64 InPublishedFileId);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess);

	TWeakObjectPtr<UESteamUGCSubsystem> Subsystem;
	int64 PublishedFileId = 0;
};

/** Downloads (or updates) a Workshop item and completes when its files are safe to use. */
UCLASS()
class USteamAsyncDownloadWorkshopItem : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * bHighPriority suspends all other Workshop downloads in favor of this one.
	 * @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|UGC",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Download Workshop Item"))
	static USteamAsyncDownloadWorkshopItem* DownloadWorkshopItem(UObject* WorldContext, int64 PublishedFileId, bool bHighPriority, float Timeout = 10.0f);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncWorkshopSubscriptionPin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncWorkshopSubscriptionPin OnFailure;

private:
	UFUNCTION()
	void HandleItemDownloaded(bool bSuccess, int64 InPublishedFileId);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess);

	TWeakObjectPtr<UESteamUGCSubsystem> Subsystem;
	int64 PublishedFileId = 0;
	bool bHighPriority = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSteamAsyncFavoritePin, int64, PublishedFileId, bool, bWasAddRequest);

/** Adds or removes a Workshop item from the current user's favorites. */
UCLASS()
class USteamAsyncSetWorkshopFavorite : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Adds (bAddToFavorites) or removes an item from the current user's favorites. AppId 0 uses the
	 * running app.
	 * @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|UGC",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Set Workshop Favorite"))
	static USteamAsyncSetWorkshopFavorite* SetWorkshopFavorite(UObject* WorldContext, int32 AppId, int64 PublishedFileId, bool bAddToFavorites, float Timeout = 10.0f);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncFavoritePin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncFavoritePin OnFailure;

private:
	UFUNCTION()
	void HandleFavoriteChanged(bool bSuccess, int64 InPublishedFileId, bool bWasAddRequest);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess);

	TWeakObjectPtr<UESteamUGCSubsystem> Subsystem;
	int32 AppId = 0;
	int64 PublishedFileId = 0;
	bool bAddToFavorites = true;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSteamAsyncVoteSetPin, int64, PublishedFileId, bool, bVoteUp);

/** Casts the current user's up/down vote on a Workshop item. */
UCLASS()
class USteamAsyncSetWorkshopItemVote : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|UGC",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Set Workshop Item Vote"))
	static USteamAsyncSetWorkshopItemVote* SetWorkshopItemVote(UObject* WorldContext, int64 PublishedFileId, bool bVoteUp, float Timeout = 10.0f);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncVoteSetPin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncVoteSetPin OnFailure;

private:
	UFUNCTION()
	void HandleVoteSet(bool bSuccess, int64 InPublishedFileId, bool bInVoteUp);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess);

	TWeakObjectPtr<UESteamUGCSubsystem> Subsystem;
	int64 PublishedFileId = 0;
	bool bVoteUp = true;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FSteamAsyncVoteGetPin, int64, PublishedFileId, bool, bVotedUp, bool, bVotedDown, bool, bVoteSkipped);

/** Reads the current user's existing vote on a Workshop item. */
UCLASS()
class USteamAsyncGetWorkshopItemVote : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|UGC",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Get Workshop Item Vote"))
	static USteamAsyncGetWorkshopItemVote* GetWorkshopItemVote(UObject* WorldContext, int64 PublishedFileId, float Timeout = 10.0f);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncVoteGetPin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncVoteGetPin OnFailure;

private:
	UFUNCTION()
	void HandleVote(bool bSuccess, int64 InPublishedFileId, bool bVotedUp, bool bVotedDown, bool bVoteSkipped);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, bool bVotedUp, bool bVotedDown, bool bVoteSkipped);

	TWeakObjectPtr<UESteamUGCSubsystem> Subsystem;
	int64 PublishedFileId = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSteamAsyncDeleteItemPin, int64, PublishedFileId);

/** Deletes a Workshop item the current user owns without prompting. */
UCLASS()
class USteamAsyncDeleteWorkshopItem : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|UGC",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Delete Workshop Item"))
	static USteamAsyncDeleteWorkshopItem* DeleteWorkshopItem(UObject* WorldContext, int64 PublishedFileId, float Timeout = 10.0f);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncDeleteItemPin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncDeleteItemPin OnFailure;

private:
	UFUNCTION()
	void HandleItemDeleted(bool bSuccess, int64 InPublishedFileId);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess);

	TWeakObjectPtr<UESteamUGCSubsystem> Subsystem;
	int64 PublishedFileId = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSteamAsyncDependencyPin, int64, ParentPublishedFileId, int64, ChildPublishedFileId);

/** Adds or removes a parent/child dependency between two Workshop items. */
UCLASS()
class USteamAsyncSetWorkshopDependency : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Adds (bAdd) or removes a child item as a dependency of a parent (collection).
	 * @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|UGC",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Set Workshop Dependency"))
	static USteamAsyncSetWorkshopDependency* SetWorkshopDependency(UObject* WorldContext, int64 ParentPublishedFileId, int64 ChildPublishedFileId, bool bAdd, float Timeout = 10.0f);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncDependencyPin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncDependencyPin OnFailure;

private:
	UFUNCTION()
	void HandleDependencyResult(bool bSuccess, int64 InParentPublishedFileId, int64 InChildPublishedFileId);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess);

	TWeakObjectPtr<UESteamUGCSubsystem> Subsystem;
	int64 ParentPublishedFileId = 0;
	int64 ChildPublishedFileId = 0;
	bool bAdd = true;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSteamAsyncAppDependencyPin, int64, PublishedFileId, int32, AppId);

/** Adds or removes an app (usually DLC) dependency of a Workshop item. */
UCLASS()
class USteamAsyncSetWorkshopAppDependency : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Adds (bAdd) or removes an app id as a required dependency of an item.
	 * @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|UGC",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Set Workshop App Dependency"))
	static USteamAsyncSetWorkshopAppDependency* SetWorkshopAppDependency(UObject* WorldContext, int64 PublishedFileId, int32 AppId, bool bAdd, float Timeout = 10.0f);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncAppDependencyPin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncAppDependencyPin OnFailure;

private:
	UFUNCTION()
	void HandleAppDependencyResult(bool bSuccess, int64 InPublishedFileId, int32 InAppId);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess);

	TWeakObjectPtr<UESteamUGCSubsystem> Subsystem;
	int64 PublishedFileId = 0;
	int32 AppId = 0;
	bool bAdd = true;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSteamAsyncAppDependenciesPin, int64, PublishedFileId, const TArray<int32>&, AppIds);

/** Requests the app dependencies of a Workshop item. */
UCLASS()
class USteamAsyncGetWorkshopAppDependencies : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|UGC",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Get Workshop App Dependencies"))
	static USteamAsyncGetWorkshopAppDependencies* GetWorkshopAppDependencies(UObject* WorldContext, int64 PublishedFileId, float Timeout = 10.0f);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncAppDependenciesPin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncAppDependenciesPin OnFailure;

private:
	UFUNCTION()
	void HandleAppDependencies(bool bSuccess, int64 InPublishedFileId, const TArray<int32>& AppIds);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, const TArray<int32>& AppIds);

	TWeakObjectPtr<UESteamUGCSubsystem> Subsystem;
	int64 PublishedFileId = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSteamAsyncPlaytimePin);

/** Starts tracking playtime for a set of Workshop items. */
UCLASS()
class USteamAsyncStartPlaytimeTracking : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|UGC",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Start Playtime Tracking"))
	static USteamAsyncStartPlaytimeTracking* StartPlaytimeTracking(UObject* WorldContext, const TArray<int64>& PublishedFileIds, float Timeout = 10.0f);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncPlaytimePin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncPlaytimePin OnFailure;

private:
	UFUNCTION()
	void HandleTrackingResult(bool bSuccess);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess);

	TWeakObjectPtr<UESteamUGCSubsystem> Subsystem;
	TArray<int64> PublishedFileIds;
};

/** Stops tracking playtime for a set of Workshop items. */
UCLASS()
class USteamAsyncStopPlaytimeTracking : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|UGC",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Stop Playtime Tracking"))
	static USteamAsyncStopPlaytimeTracking* StopPlaytimeTracking(UObject* WorldContext, const TArray<int64>& PublishedFileIds, float Timeout = 10.0f);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncPlaytimePin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncPlaytimePin OnFailure;

private:
	UFUNCTION()
	void HandleTrackingResult(bool bSuccess);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess);

	TWeakObjectPtr<UESteamUGCSubsystem> Subsystem;
	TArray<int64> PublishedFileIds;
};

/** Stops tracking playtime for all Workshop items. */
UCLASS()
class USteamAsyncStopPlaytimeTrackingForAllItems : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|UGC",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Stop Playtime Tracking For All Items"))
	static USteamAsyncStopPlaytimeTrackingForAllItems* StopPlaytimeTrackingForAllItems(UObject* WorldContext, float Timeout = 10.0f);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncPlaytimePin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncPlaytimePin OnFailure;

private:
	UFUNCTION()
	void HandleTrackingResult(bool bSuccess);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess);

	TWeakObjectPtr<UESteamUGCSubsystem> Subsystem;
};

/** Queries one of a user's UGC lists (published, subscribed, favorited...) with filtering options. */
UCLASS()
class USteamAsyncQueryUserWorkshopItems : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/**
	 * Queries one of a user's UGC lists. Page starts at 1.
	 * @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap).
	 */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|UGC",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Query User Workshop Items"))
	static USteamAsyncQueryUserWorkshopItems* QueryUserWorkshopItems(UObject* WorldContext, FESteamId User, EESteamUserUGCList ListType, EESteamUserUGCListSortOrder SortOrder, EESteamUGCMatchingType MatchingType, int32 Page, const FESteamUGCQueryConfig& Config, float Timeout = 10.0f);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncQueryWorkshopItemsPin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncQueryWorkshopItemsPin OnFailure;

private:
	UFUNCTION()
	void HandleQueryCompleted(bool bSuccess, const TArray<FESteamUGCDetails>& Results, int32 TotalMatchingResults);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, const TArray<FESteamUGCDetails>& Results, int32 TotalResults);

	TWeakObjectPtr<UESteamUGCSubsystem> Subsystem;
	FESteamId User;
	EESteamUserUGCList ListType = EESteamUserUGCList::Published;
	EESteamUserUGCListSortOrder SortOrder = EESteamUserUGCListSortOrder::CreationOrderDesc;
	EESteamUGCMatchingType MatchingType = EESteamUGCMatchingType::Items;
	int32 Page = 1;
	FESteamUGCQueryConfig Config;
};

/** Queries the details of specific Workshop items by id. */
UCLASS()
class USteamAsyncQueryWorkshopItemsByIds : public USteamAsyncActionBase
{
	GENERATED_BODY()

public:
	/** @param Timeout Seconds before the node fails with OnFailure if no result arrives (<= 0 uses a safety cap). */
	UFUNCTION(BlueprintCallable, Category = "Steam|Async|UGC",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContext", DisplayName = "Steam: Query Workshop Items By Ids"))
	static USteamAsyncQueryWorkshopItemsByIds* QueryWorkshopItemsByIds(UObject* WorldContext, const TArray<int64>& PublishedFileIds, float Timeout = 10.0f);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncQueryWorkshopItemsPin OnSuccess;

	UPROPERTY(BlueprintAssignable)
	FSteamAsyncQueryWorkshopItemsPin OnFailure;

private:
	UFUNCTION()
	void HandleQueryCompleted(bool bSuccess, const TArray<FESteamUGCDetails>& Results, int32 TotalMatchingResults);

	virtual void OnTimeoutFailure() override;

	void Complete(bool bSuccess, const TArray<FESteamUGCDetails>& Results, int32 TotalResults);

	TWeakObjectPtr<UESteamUGCSubsystem> Subsystem;
	TArray<int64> PublishedFileIds;
};
