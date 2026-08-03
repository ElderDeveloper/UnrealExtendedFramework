// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianWorkspaceData.h"

/** Injectable monotonic clock used by toast/undo tests without sleeping. */
class IExtendedAtlassianInteractionClock
{
public:
	virtual ~IExtendedAtlassianInteractionClock() = default;
	virtual double NowSeconds() const = 0;
};

/** Current toast presentation. */
struct FExtendedAtlassianToast
{
	uint64 Key = 0;
	FText Message;
	double ExpiresAtSeconds = 0.0;
	bool bOffersUndo = false;

	bool IsSet() const { return Key != 0; }
	void Reset() { *this = FExtendedAtlassianToast(); }
};

/** Central navigation, selection, async-generation, and mutation state for the Backlot tab. */
class FExtendedAtlassianWorkspaceController final
	: public TSharedFromThis<FExtendedAtlassianWorkspaceController>
{
public:
	explicit FExtendedAtlassianWorkspaceController(
		TSharedRef<IExtendedAtlassianWorkspaceData> InData,
		TSharedPtr<IExtendedAtlassianInteractionClock> InClock = nullptr);
	~FExtendedAtlassianWorkspaceController();

	DECLARE_MULTICAST_DELEGATE(FOnChanged);
	FOnChanged& OnChanged() { return Changed; }

	void Refresh();
	void Navigate(EExtendedAtlassianWorkspaceRoute NewRoute);
	void OpenIssue(const FString& IssueKey);
	void SelectPage(const FString& PageId);
	void SelectIssue(const FString& IssueKey);
	void SelectPin(const FString& PinId);
	void SelectNotification(const FString& NotificationId);
	void SetGlobalSearch(const FString& Search);
	void SetPageSearch(const FString& Search);
	void ToggleDocumentNode(const FString& NodeId);
	void SelectIssueView(const FString& ViewId);
	void CycleStatusFilter();
	void CycleAssigneeFilter();
	void SetAssigneeFilter(const FString& AccountId);
	void ToggleEpicFilter(const FString& EpicId);
	void ResetIssueFilters();
	void SetInboxTab(const FString& Tab);
	void SetCompact(bool bInCompact);
	void SetRailOpen(bool bInRailOpen);
	void ToggleCompact();
	void ToggleRail();
	bool CanExecuteMutation(
		EExtendedAtlassianWorkspaceMutation Type,
		FText* OutReason = nullptr) const;
	/** Starts a mutation and returns false when capability checks reject it locally. */
	bool ExecuteMutation(const FExtendedAtlassianWorkspaceMutation& Mutation);
	void ExecuteDestructiveMutation(
		const FExtendedAtlassianWorkspaceMutation& Mutation,
		const FText& UndoMessage);
	bool UndoLastDestructiveMutation();
	void TickInteractionState();
	void DismissToast();
	void ShowToast(const FText& Message, double DurationSeconds = 2.6);

	EExtendedAtlassianWorkspaceRoute GetRoute() const { return Route; }
	const FExtendedAtlassianWorkspaceSnapshot& GetSnapshot() const { return Snapshot; }
	const FString& GetSelectedPageId() const { return SelectedPageId; }
	const FString& GetSelectedIssueKey() const { return SelectedIssueKey; }
	const FString& GetSelectedPinId() const { return SelectedPinId; }
	const FString& GetSelectedNotificationId() const { return SelectedNotificationId; }
	const FString& GetGlobalSearch() const { return GlobalSearch; }
	const FString& GetPageSearch() const { return PageSearch; }
	const FString& GetSelectedIssueViewId() const { return SelectedIssueViewId; }
	const FString& GetStatusFilter() const { return StatusFilter; }
	const FString& GetAssigneeFilter() const { return AssigneeFilter; }
	const FString& GetEpicFilter() const { return EpicFilter; }
	const FString& GetInboxTab() const { return InboxTab; }
	const FExtendedAtlassianError& GetLastMutationError() const { return LastMutationError; }
	const FExtendedAtlassianError& GetLastMutationWarning() const { return LastMutationWarning; }
	const FExtendedAtlassianToast& GetToast() const { return Toast; }
	bool IsCompact() const { return bCompact; }
	bool IsRailOpen() const { return bRailOpen; }
	bool IsFixtureProvider() const { return Data->IsFixtureProvider(); }
	bool IsMutating() const { return PendingMutations.Num() > 0 || PendingDestructive.IsSet(); }
	bool HasTimedInteractionState() const
	{
		return Toast.IsSet() || PendingDestructive.IsSet();
	}
	int32 GetUnreadCount() const;
	/** Stable, secret-free state used by automation and CI state diffs. */
	FString ExportNormalizedState() const;

private:
	void HandleLoad(
		const FExtendedAtlassianWorkspaceRequest& Request,
		const FExtendedAtlassianWorkspaceSnapshot& Loaded);
	void HandleMutation(
		uint64 MutationId,
		bool bSuccess,
		const FString& ResultId,
		const FExtendedAtlassianError& Error);
	void BeginProviderMutation(
		FExtendedAtlassianWorkspaceMutation Mutation,
		const FExtendedAtlassianWorkspaceSnapshot& RollbackSnapshot,
		bool bApplyOptimistically);
	/** Adds stable catalog ids beside presentation labels before a provider call. */
	void EnrichMutationWithStableIds(FExtendedAtlassianWorkspaceMutation& Mutation) const;
	void CommitPendingDestructiveMutation();
	void ApplyOptimisticMutation(const FExtendedAtlassianWorkspaceMutation& Mutation);
	void LoadUserPreferences();
	void SaveUserPreferences() const;
	void EnsureSelections();
	void BroadcastChanged();

	struct FPendingDestructiveMutation
	{
		FExtendedAtlassianWorkspaceMutation Mutation;
		FExtendedAtlassianWorkspaceSnapshot Before;
		FString BeforeSelectedPageId;
		FString BeforeSelectedIssueKey;
		FString BeforeSelectedPinId;
		FString BeforeSelectedNotificationId;
		double CommitAtSeconds = 0.0;

		bool IsSet() const { return Mutation.ClientMutationId != 0; }
		void Reset() { *this = FPendingDestructiveMutation(); }
	};

	struct FMutationRollbackState
	{
		FExtendedAtlassianWorkspaceSnapshot Snapshot;
		EExtendedAtlassianWorkspaceRoute Route = EExtendedAtlassianWorkspaceRoute::Docs;
		FString SelectedPageId;
		FString SelectedIssueKey;
		FString SelectedPinId;
		FString SelectedNotificationId;
	};

	TSharedRef<IExtendedAtlassianWorkspaceData> Data;
	TSharedPtr<IExtendedAtlassianInteractionClock> Clock;
	FExtendedAtlassianWorkspaceSnapshot Snapshot;
	FOnChanged Changed;
	EExtendedAtlassianWorkspaceRoute Route = EExtendedAtlassianWorkspaceRoute::Docs;
	FString SelectedPageId;
	FString SelectedIssueKey;
	FString SelectedPinId = TEXT("M_WetStone_Master");
	FString SelectedNotificationId = TEXT("fixture-inbox-0");
	FString GlobalSearch;
	FString PageSearch;
	FString SelectedIssueViewId = TEXT("sprint");
	FString StatusFilter = TEXT("any");
	FString AssigneeFilter = TEXT("anyone");
	FString EpicFilter;
	FString InboxTab = TEXT("All");
	FExtendedAtlassianError LastMutationError;
	FExtendedAtlassianError LastMutationWarning;
	TSet<uint64> PendingMutations;
	TSet<uint64> PendingOpenResultMutations;
	TMap<uint64, FMutationRollbackState> MutationRollbacks;
	FPendingDestructiveMutation PendingDestructive;
	FExtendedAtlassianToast Toast;
	uint64 RequestGeneration = 0;
	uint64 NextMutationId = 1;
	uint64 NextToastKey = 1;
	bool bCompact = false;
	bool bRailOpen = true;
	bool bPreferencesLoaded = false;
};
