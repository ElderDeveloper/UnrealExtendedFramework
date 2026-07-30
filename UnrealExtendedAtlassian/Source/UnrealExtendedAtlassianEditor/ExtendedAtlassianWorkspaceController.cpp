// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianWorkspaceController.h"

#include "ExtendedAtlassianModelUtils.h"
#include "Misc/ConfigCacheIni.h"

namespace ExtendedAtlassianWorkspaceControllerPrivate
{
	class FSystemInteractionClock final : public IExtendedAtlassianInteractionClock
	{
	public:
		virtual double NowSeconds() const override
		{
			return FPlatformTime::Seconds();
		}
	};

	FString MutationField(
		const FExtendedAtlassianWorkspaceMutation& Mutation,
		const TCHAR* Name)
	{
		if (const FString* Value = Mutation.Fields.Find(Name))
		{
			return *Value;
		}
		return FString();
	}

	EExtendedAtlassianPinKind PinKindFromField(const FString& Kind)
	{
		if (Kind == TEXT("LEVEL"))
		{
			return EExtendedAtlassianPinKind::Level;
		}
		if (Kind == TEXT("BLUEPRINT"))
		{
			return EExtendedAtlassianPinKind::Blueprint;
		}
		if (Kind == TEXT("PAGE"))
		{
			return EExtendedAtlassianPinKind::Page;
		}
		return EExtendedAtlassianPinKind::Material;
	}

	bool HasVisibleContent(const FExtendedAtlassianWorkspaceSnapshot& Snapshot)
	{
		return !Snapshot.Pages.IsEmpty()
			|| !Snapshot.DocumentTree.IsEmpty()
			|| !Snapshot.Issues.IsEmpty()
			|| !Snapshot.BoardColumns.IsEmpty()
			|| !Snapshot.Pins.IsEmpty()
			|| !Snapshot.Notifications.IsEmpty();
	}

	bool IsFailureState(EExtendedAtlassianLoadState State)
	{
		return State == EExtendedAtlassianLoadState::Offline
			|| State == EExtendedAtlassianLoadState::Error
			|| State == EExtendedAtlassianLoadState::PermissionDenied;
	}
}

FExtendedAtlassianWorkspaceController::FExtendedAtlassianWorkspaceController(
	TSharedRef<IExtendedAtlassianWorkspaceData> InData,
	TSharedPtr<IExtendedAtlassianInteractionClock> InClock)
	: Data(MoveTemp(InData))
	, Clock(InClock.IsValid()
		? MoveTemp(InClock)
		: MakeShared<ExtendedAtlassianWorkspaceControllerPrivate::FSystemInteractionClock>())
{
}

FExtendedAtlassianWorkspaceController::~FExtendedAtlassianWorkspaceController()
{
	SaveUserPreferences();
	if (RequestGeneration > 0)
	{
		Data->CancelGeneration(RequestGeneration);
	}

	// A destructive operation that is still in its Undo window was never confirmed remotely.
	PendingDestructive.Reset();
}

void FExtendedAtlassianWorkspaceController::Refresh()
{
	if (RequestGeneration > 0)
	{
		Data->CancelGeneration(RequestGeneration);
	}

	++RequestGeneration;
	if (ExtendedAtlassianWorkspaceControllerPrivate::HasVisibleContent(Snapshot))
	{
		Snapshot.bRefreshing = true;
	}
	else
	{
		Snapshot.State = EExtendedAtlassianLoadState::Loading;
		Snapshot.bRefreshing = false;
	}
	BroadcastChanged();

	FExtendedAtlassianWorkspaceRequest Request;
	Request.Route = Route;
	Request.SelectedPageId = SelectedPageId;
	Request.SelectedIssueKey = SelectedIssueKey;
	Request.SelectedPinId = SelectedPinId;
	Request.SelectedNotificationId = SelectedNotificationId;
	Request.GlobalSearch = GlobalSearch;
	Request.Generation = RequestGeneration;

	Data->Load(
		Request,
		FExtendedAtlassianWorkspaceLoadDelegate::CreateSP(
			AsShared(),
			&FExtendedAtlassianWorkspaceController::HandleLoad));
}

void FExtendedAtlassianWorkspaceController::Navigate(EExtendedAtlassianWorkspaceRoute NewRoute)
{
	if (Route == NewRoute)
	{
		return;
	}

	Route = NewRoute;
	SaveUserPreferences();
	BroadcastChanged();
}

void FExtendedAtlassianWorkspaceController::OpenIssue(const FString& IssueKey)
{
	SelectedIssueKey = IssueKey;
	Route = EExtendedAtlassianWorkspaceRoute::IssueDetail;
	SaveUserPreferences();
	BroadcastChanged();
}

void FExtendedAtlassianWorkspaceController::SelectPage(const FString& PageId)
{
	if (SelectedPageId == PageId)
	{
		if (!Data->IsFixtureProvider() && !IsMutating())
		{
			Refresh();
		}
		return;
	}

	SelectedPageId = PageId;
	SaveUserPreferences();
	if (!Data->IsFixtureProvider() && !IsMutating())
	{
		Refresh();
	}
	else
	{
		BroadcastChanged();
	}
}

void FExtendedAtlassianWorkspaceController::SelectIssue(const FString& IssueKey)
{
	SelectedIssueKey = IssueKey;
	SaveUserPreferences();
	BroadcastChanged();
}

void FExtendedAtlassianWorkspaceController::SelectPin(const FString& PinId)
{
	SelectedPinId = PinId;
	SaveUserPreferences();
	BroadcastChanged();
}

void FExtendedAtlassianWorkspaceController::SelectNotification(const FString& NotificationId)
{
	SelectedNotificationId = NotificationId;
	SaveUserPreferences();

	FExtendedAtlassianWorkspaceMutation Mutation;
	Mutation.Type = EExtendedAtlassianWorkspaceMutation::MarkNotificationRead;
	Mutation.TargetId = NotificationId;
	ExecuteMutation(Mutation);
}

void FExtendedAtlassianWorkspaceController::SetGlobalSearch(const FString& Search)
{
	if (GlobalSearch == Search)
	{
		return;
	}

	GlobalSearch = Search;
	BroadcastChanged();
}

void FExtendedAtlassianWorkspaceController::SetPageSearch(const FString& Search)
{
	if (PageSearch == Search)
	{
		return;
	}
	PageSearch = Search;
	BroadcastChanged();
}

void FExtendedAtlassianWorkspaceController::ToggleDocumentNode(const FString& NodeId)
{
	for (FExtendedAtlassianDocumentTreeNode& Node : Snapshot.DocumentTree)
	{
		const bool bHasChildren = Snapshot.DocumentTree.ContainsByPredicate(
			[&Node](const FExtendedAtlassianDocumentTreeNode& Candidate)
			{
				return Candidate.ParentId == Node.Id;
			});
		if (Node.Id == NodeId && (Node.bSection || bHasChildren))
		{
			Node.bExpanded = !Node.bExpanded;
			BroadcastChanged();
			break;
		}
	}
}

void FExtendedAtlassianWorkspaceController::SelectIssueView(const FString& ViewId)
{
	SelectedIssueViewId = ViewId;
	StatusFilter = TEXT("any");
	AssigneeFilter = TEXT("anyone");
	EpicFilter.Reset();
	BroadcastChanged();
}

void FExtendedAtlassianWorkspaceController::CycleStatusFilter()
{
	StatusFilter =
		StatusFilter == TEXT("any")
			? TEXT("not Done")
			: (StatusFilter == TEXT("not Done") ? TEXT("Done") : TEXT("any"));
	BroadcastChanged();
}

void FExtendedAtlassianWorkspaceController::CycleAssigneeFilter()
{
	const TArray<FExtendedAtlassianUser>& People = Snapshot.People;
	if (People.IsEmpty())
	{
		AssigneeFilter.Reset();
		BroadcastChanged();
		return;
	}

	if (AssigneeFilter.IsEmpty() || AssigneeFilter == TEXT("anyone"))
	{
		AssigneeFilter = People[0].AccountId;
	}
	else
	{
		const int32 Current = People.IndexOfByPredicate(
			[this](const FExtendedAtlassianUser& User)
			{
				return User.AccountId == AssigneeFilter;
			});
		AssigneeFilter =
			Current == INDEX_NONE || Current + 1 >= People.Num()
				? FString(TEXT("anyone"))
				: People[Current + 1].AccountId;
	}
	BroadcastChanged();
}

void FExtendedAtlassianWorkspaceController::ToggleEpicFilter(const FString& EpicId)
{
	EpicFilter = EpicFilter == EpicId ? FString() : EpicId;
	BroadcastChanged();
}

void FExtendedAtlassianWorkspaceController::ResetIssueFilters()
{
	SelectedIssueViewId = Snapshot.IssueViews.IsEmpty()
		? FString()
		: Snapshot.IssueViews[0].Id;
	StatusFilter = TEXT("any");
	AssigneeFilter = TEXT("anyone");
	EpicFilter.Reset();
	GlobalSearch.Reset();
	BroadcastChanged();
}

void FExtendedAtlassianWorkspaceController::SetInboxTab(const FString& Tab)
{
	if (InboxTab != Tab)
	{
		InboxTab = Tab;
		BroadcastChanged();
	}
}

void FExtendedAtlassianWorkspaceController::SetCompact(bool bInCompact)
{
	if (bCompact == bInCompact)
	{
		return;
	}

	bCompact = bInCompact;
	SaveUserPreferences();
	BroadcastChanged();
}

void FExtendedAtlassianWorkspaceController::SetRailOpen(bool bInRailOpen)
{
	if (bRailOpen == bInRailOpen)
	{
		return;
	}

	bRailOpen = bInRailOpen;
	SaveUserPreferences();
	BroadcastChanged();
}

void FExtendedAtlassianWorkspaceController::ToggleCompact()
{
	SetCompact(!bCompact);
}

void FExtendedAtlassianWorkspaceController::ToggleRail()
{
	SetRailOpen(!bRailOpen);
}

bool FExtendedAtlassianWorkspaceController::CanExecuteMutation(
	EExtendedAtlassianWorkspaceMutation Type,
	FText* OutReason) const
{
	const FExtendedAtlassianCapabilities& Capabilities = Snapshot.Capabilities;
	bool bAllowed = true;
	const TCHAR* RequiredPermission = TEXT("");
	switch (Type)
	{
	case EExtendedAtlassianWorkspaceMutation::CreateIssue:
	case EExtendedAtlassianWorkspaceMutation::CreateCaptureIssue:
		bAllowed = Capabilities.bCanCreateIssues;
		RequiredPermission = TEXT("Create Issues");
		break;
	case EExtendedAtlassianWorkspaceMutation::UpdateIssue:
		bAllowed = Capabilities.bCanEditIssues;
		RequiredPermission = TEXT("Edit Issues");
		break;
	case EExtendedAtlassianWorkspaceMutation::DeleteIssue:
		bAllowed = Capabilities.bCanDeleteIssues;
		RequiredPermission = TEXT("Delete Issues");
		break;
	case EExtendedAtlassianWorkspaceMutation::TransitionIssue:
		bAllowed = Capabilities.bCanTransitionIssues;
		RequiredPermission = TEXT("Transition Issues");
		break;
	case EExtendedAtlassianWorkspaceMutation::RankIssue:
		bAllowed = Capabilities.bCanRankIssues;
		RequiredPermission = TEXT("Jira Software ranking");
		break;
	case EExtendedAtlassianWorkspaceMutation::MoveIssue:
		bAllowed =
			Capabilities.bCanTransitionIssues
			&& Capabilities.bCanRankIssues;
		RequiredPermission = TEXT("Transition Issues and Jira Software ranking");
		break;
	case EExtendedAtlassianWorkspaceMutation::CreateIssueComment:
	case EExtendedAtlassianWorkspaceMutation::UpdateIssueComment:
	case EExtendedAtlassianWorkspaceMutation::DeleteIssueComment:
	case EExtendedAtlassianWorkspaceMutation::ResolveIssueComment:
	case EExtendedAtlassianWorkspaceMutation::ReopenIssueComment:
	case EExtendedAtlassianWorkspaceMutation::CreatePageComment:
	case EExtendedAtlassianWorkspaceMutation::UpdatePageComment:
	case EExtendedAtlassianWorkspaceMutation::DeletePageComment:
	case EExtendedAtlassianWorkspaceMutation::ResolvePageComment:
	case EExtendedAtlassianWorkspaceMutation::ReopenPageComment:
		bAllowed = Capabilities.bCanComment;
		RequiredPermission = TEXT("Add Comments");
		break;
	case EExtendedAtlassianWorkspaceMutation::CreatePage:
	case EExtendedAtlassianWorkspaceMutation::UpdatePage:
	case EExtendedAtlassianWorkspaceMutation::MovePage:
	case EExtendedAtlassianWorkspaceMutation::DuplicatePage:
	case EExtendedAtlassianWorkspaceMutation::CreateSection:
	case EExtendedAtlassianWorkspaceMutation::RenameSection:
	case EExtendedAtlassianWorkspaceMutation::ReorderPage:
	case EExtendedAtlassianWorkspaceMutation::TogglePageTask:
		bAllowed = Capabilities.bCanEditPages;
		RequiredPermission = TEXT("Confluence page edit");
		break;
	case EExtendedAtlassianWorkspaceMutation::DeletePage:
	case EExtendedAtlassianWorkspaceMutation::DeleteSection:
		bAllowed = Capabilities.bCanDeletePages;
		RequiredPermission = TEXT("Confluence page delete");
		break;
	case EExtendedAtlassianWorkspaceMutation::CreatePin:
	case EExtendedAtlassianWorkspaceMutation::UpdatePin:
	case EExtendedAtlassianWorkspaceMutation::DeletePin:
	case EExtendedAtlassianWorkspaceMutation::CreatePinReply:
	case EExtendedAtlassianWorkspaceMutation::UpdatePinReply:
	case EExtendedAtlassianWorkspaceMutation::DeletePinReply:
	case EExtendedAtlassianWorkspaceMutation::ResolvePinReply:
		bAllowed = Capabilities.bCanUseSharedMetadata;
		RequiredPermission = TEXT("Backlot shared metadata page edit");
		break;
	default:
		break;
	}

	if (!bAllowed && OutReason)
	{
		*OutReason = FText::Format(
			NSLOCTEXT(
				"ExtendedAtlassian",
				"MissingOperationPermission",
				"Unavailable: this action requires {0} permission."),
			FText::FromString(RequiredPermission));
	}
	return bAllowed;
}

void FExtendedAtlassianWorkspaceController::ExecuteMutation(
	const FExtendedAtlassianWorkspaceMutation& Mutation)
{
	FText PermissionReason;
	if (!CanExecuteMutation(Mutation.Type, &PermissionReason))
	{
		LastMutationError.HttpStatus = 403;
		LastMutationError.Code = TEXT("Forbidden");
		LastMutationError.Message = PermissionReason.ToString();
		LastMutationError.bRetryable = false;
		ShowToast(PermissionReason);
		return;
	}
	FExtendedAtlassianWorkspaceMutation Pending = Mutation;
	EnrichMutationWithStableIds(Pending);
	Pending.ClientMutationId = NextMutationId++;
	BeginProviderMutation(Pending, Snapshot, true);
}

void FExtendedAtlassianWorkspaceController::EnrichMutationWithStableIds(
	FExtendedAtlassianWorkspaceMutation& Mutation) const
{
	auto AddCatalogId = [&Mutation](
		const TCHAR* LabelField,
		const TCHAR* IdField,
		const auto& Options)
	{
		if (Mutation.Fields.Contains(IdField))
		{
			return;
		}
		const FString Label = Mutation.Fields.FindRef(LabelField);
		if (Label.IsEmpty())
		{
			return;
		}
		if (const auto* Option = Options.FindByPredicate(
			[&Label](const auto& Candidate)
			{
				return Candidate.Name.Equals(Label, ESearchCase::IgnoreCase)
					|| Candidate.Id.Equals(Label, ESearchCase::CaseSensitive);
			}))
		{
			Mutation.Fields.Add(IdField, Option->Id);
		}
	};

	AddCatalogId(TEXT("type"), TEXT("typeId"), Snapshot.IssueTypes);
	AddCatalogId(TEXT("priority"), TEXT("priorityId"), Snapshot.Priorities);
	AddCatalogId(TEXT("epic"), TEXT("epicId"), Snapshot.Epics);

	if (!Mutation.Fields.Contains(TEXT("statusId")))
	{
		const FString StatusName = Mutation.Fields.FindRef(TEXT("status"));
		if (!StatusName.IsEmpty())
		{
			if (const FExtendedAtlassianIssue* Issue =
				Snapshot.Issues.FindByPredicate(
					[&StatusName](const FExtendedAtlassianIssue& Candidate)
					{
						return Candidate.StatusName.Equals(
							StatusName,
							ESearchCase::IgnoreCase);
					}))
			{
				if (!Issue->StatusId.IsEmpty())
				{
					Mutation.Fields.Add(TEXT("statusId"), Issue->StatusId);
				}
			}
		}
	}

	// Assignee values are normalized to Atlassian account ids. Labels/initials remain
	// presentation-only and are never sent to a live provider when an id is known.
	if (const FString* Assignee = Mutation.Fields.Find(TEXT("assignee")))
	{
		if (const FExtendedAtlassianUser* User = Snapshot.People.FindByPredicate(
			[Assignee](const FExtendedAtlassianUser& Candidate)
			{
				return Candidate.AccountId.Equals(
						*Assignee,
						ESearchCase::CaseSensitive)
					|| Candidate.DisplayName.Equals(
						*Assignee,
						ESearchCase::IgnoreCase)
					|| Candidate.Initials.Equals(
						*Assignee,
						ESearchCase::IgnoreCase);
			}))
		{
			Mutation.Fields.Add(TEXT("assignee"), User->AccountId);
		}
	}
}

void FExtendedAtlassianWorkspaceController::ExecuteDestructiveMutation(
	const FExtendedAtlassianWorkspaceMutation& Mutation,
	const FText& UndoMessage)
{
	FText PermissionReason;
	if (!CanExecuteMutation(Mutation.Type, &PermissionReason))
	{
		LastMutationError.HttpStatus = 403;
		LastMutationError.Code = TEXT("Forbidden");
		LastMutationError.Message = PermissionReason.ToString();
		LastMutationError.bRetryable = false;
		ShowToast(PermissionReason);
		return;
	}
	if (PendingDestructive.IsSet())
	{
		CommitPendingDestructiveMutation();
	}

	PendingDestructive.Mutation = Mutation;
	PendingDestructive.Mutation.ClientMutationId = NextMutationId++;
	PendingDestructive.Before = Snapshot;
	PendingDestructive.BeforeSelectedPageId = SelectedPageId;
	PendingDestructive.BeforeSelectedIssueKey = SelectedIssueKey;
	PendingDestructive.BeforeSelectedPinId = SelectedPinId;
	PendingDestructive.BeforeSelectedNotificationId = SelectedNotificationId;
	PendingDestructive.CommitAtSeconds = Clock->NowSeconds() + 7.0;
	ApplyOptimisticMutation(PendingDestructive.Mutation);
	EnsureSelections();

	Toast.Key = NextToastKey++;
	Toast.Message = UndoMessage;
	Toast.ExpiresAtSeconds = PendingDestructive.CommitAtSeconds;
	Toast.bOffersUndo = true;
	BroadcastChanged();
}

bool FExtendedAtlassianWorkspaceController::UndoLastDestructiveMutation()
{
	if (!PendingDestructive.IsSet())
	{
		return false;
	}

	Snapshot = MoveTemp(PendingDestructive.Before);
	SelectedPageId = PendingDestructive.BeforeSelectedPageId;
	SelectedIssueKey = PendingDestructive.BeforeSelectedIssueKey;
	SelectedPinId = PendingDestructive.BeforeSelectedPinId;
	SelectedNotificationId = PendingDestructive.BeforeSelectedNotificationId;
	PendingDestructive.Reset();
	ShowToast(NSLOCTEXT("ExtendedAtlassian", "UndoRestored", "Restored"), 2.6);
	EnsureSelections();
	BroadcastChanged();
	return true;
}

void FExtendedAtlassianWorkspaceController::TickInteractionState()
{
	const double Now = Clock->NowSeconds();
	bool bChanged = false;
	if (PendingDestructive.IsSet() && Now >= PendingDestructive.CommitAtSeconds)
	{
		CommitPendingDestructiveMutation();
		bChanged = true;
	}
	if (Toast.IsSet() && Now >= Toast.ExpiresAtSeconds)
	{
		Toast.Reset();
		bChanged = true;
	}
	if (bChanged)
	{
		BroadcastChanged();
	}
}

void FExtendedAtlassianWorkspaceController::DismissToast()
{
	if (Toast.IsSet())
	{
		Toast.Reset();
		BroadcastChanged();
	}
}

void FExtendedAtlassianWorkspaceController::ShowToast(
	const FText& Message,
	double DurationSeconds)
{
	Toast.Key = NextToastKey++;
	Toast.Message = Message;
	Toast.ExpiresAtSeconds = Clock->NowSeconds() + FMath::Max(0.0, DurationSeconds);
	Toast.bOffersUndo = false;
	BroadcastChanged();
}

void FExtendedAtlassianWorkspaceController::BeginProviderMutation(
	FExtendedAtlassianWorkspaceMutation Mutation,
	const FExtendedAtlassianWorkspaceSnapshot& RollbackSnapshot,
	bool bApplyOptimistically)
{
	PendingMutations.Add(Mutation.ClientMutationId);
	MutationRollbacks.Add(Mutation.ClientMutationId, RollbackSnapshot);
	LastMutationError.Reset();
	LastMutationWarning.Reset();
	if (bApplyOptimistically)
	{
		ApplyOptimisticMutation(Mutation);
		EnsureSelections();
	}
	BroadcastChanged();

	Data->Mutate(
		Mutation,
		FExtendedAtlassianWorkspaceMutationDelegate::CreateSP(
			AsShared(),
			&FExtendedAtlassianWorkspaceController::HandleMutation));
}

void FExtendedAtlassianWorkspaceController::CommitPendingDestructiveMutation()
{
	if (!PendingDestructive.IsSet())
	{
		return;
	}

	FExtendedAtlassianWorkspaceMutation Mutation = PendingDestructive.Mutation;
	FExtendedAtlassianWorkspaceSnapshot Rollback = MoveTemp(PendingDestructive.Before);
	PendingDestructive.Reset();
	if (Toast.bOffersUndo)
	{
		Toast.Reset();
	}
	BeginProviderMutation(MoveTemp(Mutation), Rollback, false);
}

int32 FExtendedAtlassianWorkspaceController::GetUnreadCount() const
{
	int32 Count = 0;
	for (const FExtendedAtlassianNotification& Notification : Snapshot.Notifications)
	{
		Count += !Notification.bRead && !Notification.bArchived ? 1 : 0;
	}
	return Count;
}

FString FExtendedAtlassianWorkspaceController::ExportNormalizedState() const
{
	auto Normalize = [](FString Value)
	{
		Value.ReplaceInline(TEXT("\r"), TEXT("\\r"));
		Value.ReplaceInline(TEXT("\n"), TEXT("\\n"));
		Value.ReplaceInline(TEXT("|"), TEXT("\\p"));
		return Value;
	};

	TArray<FString> Parts;
	Parts.Reserve(
		18
		+ Snapshot.Pages.Num()
		+ Snapshot.Issues.Num()
		+ Snapshot.Pins.Num()
		+ Snapshot.Notifications.Num());
	Parts.Add(FString::Printf(TEXT("state=%d"), static_cast<int32>(Snapshot.State)));
	Parts.Add(FString::Printf(TEXT("route=%d"), static_cast<int32>(Route)));
	Parts.Add(FString::Printf(TEXT("compact=%d"), bCompact ? 1 : 0));
	Parts.Add(FString::Printf(TEXT("rail=%d"), bRailOpen ? 1 : 0));
	Parts.Add(FString::Printf(TEXT("refreshing=%d"), Snapshot.bRefreshing ? 1 : 0));
	Parts.Add(FString::Printf(TEXT("stale=%d"), Snapshot.bStale ? 1 : 0));
	Parts.Add(TEXT("page=") + Normalize(SelectedPageId));
	Parts.Add(TEXT("issue=") + Normalize(SelectedIssueKey));
	Parts.Add(TEXT("pin=") + Normalize(SelectedPinId));
	Parts.Add(TEXT("notification=") + Normalize(SelectedNotificationId));
	Parts.Add(TEXT("globalSearch=") + Normalize(GlobalSearch));
	Parts.Add(TEXT("pageSearch=") + Normalize(PageSearch));
	Parts.Add(TEXT("issueView=") + Normalize(SelectedIssueViewId));
	Parts.Add(TEXT("statusFilter=") + Normalize(StatusFilter));
	Parts.Add(TEXT("assigneeFilter=") + Normalize(AssigneeFilter));
	Parts.Add(TEXT("epicFilter=") + Normalize(EpicFilter));
	Parts.Add(TEXT("inboxTab=") + Normalize(InboxTab));
	Parts.Add(FString::Printf(TEXT("unread=%d"), GetUnreadCount()));
	for (const FExtendedAtlassianPage& Page : Snapshot.Pages)
	{
		Parts.Add(FString::Printf(
			TEXT("pageItem=%s,%s,%d,%u"),
			*Normalize(Page.Id),
			*Normalize(Page.Title),
			Page.Version,
			GetTypeHash(Page.Markdown)));
	}
	for (const FExtendedAtlassianIssue& Issue : Snapshot.Issues)
	{
		Parts.Add(FString::Printf(
			TEXT("issueItem=%s,%s,%s,%s,%.3f"),
			*Normalize(Issue.Key),
			*Normalize(Issue.Summary),
			*Normalize(Issue.StatusName),
			*Normalize(Issue.StatusCategoryKey),
			Issue.Estimate));
	}
	for (const FExtendedAtlassianPin& Pin : Snapshot.Pins)
	{
		int32 ResolvedThreads = 0;
		for (const FExtendedAtlassianPinThread& Thread : Pin.Threads)
		{
			ResolvedThreads += Thread.bResolved ? 1 : 0;
		}
		Parts.Add(FString::Printf(
			TEXT("pinItem=%s,%s,%d,%d"),
			*Normalize(Pin.Id),
			*Normalize(Pin.DisplayName),
			static_cast<int32>(Pin.Target.Kind),
			ResolvedThreads));
	}
	for (const FExtendedAtlassianNotification& Notification :
		Snapshot.Notifications)
	{
		Parts.Add(FString::Printf(
			TEXT("notificationItem=%s,%d,%d,%s"),
			*Normalize(Notification.Id),
			Notification.bRead ? 1 : 0,
			Notification.bArchived ? 1 : 0,
			*Normalize(Notification.Target)));
	}
	return FString::Join(Parts, TEXT("|"));
}

void FExtendedAtlassianWorkspaceController::HandleLoad(
	const FExtendedAtlassianWorkspaceRequest& Request,
	const FExtendedAtlassianWorkspaceSnapshot& Loaded)
{
	if (Request.Generation != RequestGeneration)
	{
		return;
	}

	TMap<FString, bool> DocumentExpansion;
	for (const FExtendedAtlassianDocumentTreeNode& Node : Snapshot.DocumentTree)
	{
		DocumentExpansion.Add(Node.Id, Node.bExpanded);
	}

	if (ExtendedAtlassianWorkspaceControllerPrivate::IsFailureState(Loaded.State)
		&& ExtendedAtlassianWorkspaceControllerPrivate::HasVisibleContent(Snapshot))
	{
		// A background failure must not erase a usable workspace or any local selection/draft that
		// points into it. Keep the last reconciled data and expose the failure as a stale banner.
		Snapshot.Error = Loaded.Error;
		Snapshot.bRefreshing = false;
		Snapshot.bStale = true;
	}
	else
	{
		Snapshot = Loaded;
		for (FExtendedAtlassianDocumentTreeNode& Node : Snapshot.DocumentTree)
		{
			if (const bool* PreviousExpansion = DocumentExpansion.Find(Node.Id))
			{
				Node.bExpanded = *PreviousExpansion;
			}
		}
		Snapshot.bRefreshing = false;
		Snapshot.bStale =
			Snapshot.bStale
			|| ExtendedAtlassianWorkspaceControllerPrivate::IsFailureState(Snapshot.State);
	}
	bool bLoadedPreferencesThisResponse = false;
	if (!bPreferencesLoaded)
	{
		if (Data->IsFixtureProvider())
		{
			bPreferencesLoaded = true;
		}
		else
		{
			LoadUserPreferences();
			bLoadedPreferencesThisResponse = true;
		}
	}
	if (PendingDestructive.IsSet())
	{
		PendingDestructive.Before = Snapshot;
		ApplyOptimisticMutation(PendingDestructive.Mutation);
	}
	EnsureSelections();
	if (bLoadedPreferencesThisResponse && !SelectedPageId.IsEmpty())
	{
		const FExtendedAtlassianPage* SelectedPage =
			Snapshot.Pages.FindByPredicate(
				[this](const FExtendedAtlassianPage& Page)
				{
					return Page.Id == SelectedPageId;
				});
		if (SelectedPage && SelectedPage->Version <= 0)
		{
			// Preferences are account-scoped and cannot be loaded until the first live
			// response identifies the account. Fetch the restored selection once now.
			Refresh();
			return;
		}
	}
	BroadcastChanged();
}

void FExtendedAtlassianWorkspaceController::LoadUserPreferences()
{
	if (bPreferencesLoaded || !GConfig)
	{
		return;
	}
	bPreferencesLoaded = true;
	const FString AccountId = Snapshot.CurrentUser.AccountId.IsEmpty()
		? TEXT("default")
		: Snapshot.CurrentUser.AccountId;
	const FString Section = TEXT("ExtendedAtlassian.Backlot.") + AccountId;

	int32 SavedRoute = static_cast<int32>(Route);
	if (Route == EExtendedAtlassianWorkspaceRoute::Docs
		&& GConfig->GetInt(*Section, TEXT("Route"), SavedRoute, GEditorPerProjectIni))
	{
		Route = static_cast<EExtendedAtlassianWorkspaceRoute>(
			FMath::Clamp(
				SavedRoute,
				static_cast<int32>(EExtendedAtlassianWorkspaceRoute::Docs),
				static_cast<int32>(EExtendedAtlassianWorkspaceRoute::Inbox)));
	}
	GConfig->GetString(*Section, TEXT("SelectedPage"), SelectedPageId, GEditorPerProjectIni);
	GConfig->GetString(*Section, TEXT("SelectedIssue"), SelectedIssueKey, GEditorPerProjectIni);
	GConfig->GetString(*Section, TEXT("SelectedPin"), SelectedPinId, GEditorPerProjectIni);
	GConfig->GetString(
		*Section,
		TEXT("SelectedNotification"),
		SelectedNotificationId,
		GEditorPerProjectIni);
	GConfig->GetBool(*Section, TEXT("Compact"), bCompact, GEditorPerProjectIni);
	GConfig->GetBool(*Section, TEXT("RailOpen"), bRailOpen, GEditorPerProjectIni);
}

void FExtendedAtlassianWorkspaceController::SaveUserPreferences() const
{
	if (!bPreferencesLoaded || !GConfig || Data->IsFixtureProvider())
	{
		return;
	}
	const FString AccountId = Snapshot.CurrentUser.AccountId.IsEmpty()
		? TEXT("default")
		: Snapshot.CurrentUser.AccountId;
	const FString Section = TEXT("ExtendedAtlassian.Backlot.") + AccountId;
	GConfig->SetInt(*Section, TEXT("Route"), static_cast<int32>(Route), GEditorPerProjectIni);
	GConfig->SetString(*Section, TEXT("SelectedPage"), *SelectedPageId, GEditorPerProjectIni);
	GConfig->SetString(*Section, TEXT("SelectedIssue"), *SelectedIssueKey, GEditorPerProjectIni);
	GConfig->SetString(*Section, TEXT("SelectedPin"), *SelectedPinId, GEditorPerProjectIni);
	GConfig->SetString(
		*Section,
		TEXT("SelectedNotification"),
		*SelectedNotificationId,
		GEditorPerProjectIni);
	GConfig->SetBool(*Section, TEXT("Compact"), bCompact, GEditorPerProjectIni);
	GConfig->SetBool(*Section, TEXT("RailOpen"), bRailOpen, GEditorPerProjectIni);
	GConfig->Flush(false, GEditorPerProjectIni);
}

void FExtendedAtlassianWorkspaceController::HandleMutation(
	uint64 MutationId,
	bool bSuccess,
	const FExtendedAtlassianError& Error)
{
	PendingMutations.Remove(MutationId);
	FExtendedAtlassianWorkspaceSnapshot Rollback;
	const bool bHadRollback = MutationRollbacks.RemoveAndCopyValue(MutationId, Rollback);
	if (!bSuccess)
	{
		if (bHadRollback)
		{
			Snapshot = MoveTemp(Rollback);
		}
		LastMutationError = Error;
		Toast.Key = NextToastKey++;
		Toast.Message = FText::FromString(Error.ToString());
		Toast.ExpiresAtSeconds = Clock->NowSeconds() + 2.6;
		Toast.bOffersUndo = false;
		EnsureSelections();
		BroadcastChanged();
		return;
	}
	LastMutationWarning = Error;
	if (Error.IsSet())
	{
		Toast.Key = NextToastKey++;
		Toast.Message = FText::FromString(Error.Message);
		Toast.ExpiresAtSeconds = Clock->NowSeconds() + 2.6;
		Toast.bOffersUndo = false;
	}

	// Fixture mutations complete synchronously; live providers may need a reconciliatory refresh.
	Refresh();
}

void FExtendedAtlassianWorkspaceController::ApplyOptimisticMutation(
	const FExtendedAtlassianWorkspaceMutation& Mutation)
{
	using namespace ExtendedAtlassianWorkspaceControllerPrivate;

	if (Mutation.Type == EExtendedAtlassianWorkspaceMutation::MoveIssue)
	{
		FExtendedAtlassianWorkspaceMutation Transition = Mutation;
		Transition.Type =
			EExtendedAtlassianWorkspaceMutation::TransitionIssue;
		ApplyOptimisticMutation(Transition);
		FExtendedAtlassianWorkspaceMutation Rank = Mutation;
		Rank.Type = EExtendedAtlassianWorkspaceMutation::RankIssue;
		ApplyOptimisticMutation(Rank);
		return;
	}

	if (ExtendedAtlassianModelUtils::ApplyDocumentMutation(Snapshot, Mutation))
	{
		return;
	}

	switch (Mutation.Type)
	{
	case EExtendedAtlassianWorkspaceMutation::CreateIssue:
	case EExtendedAtlassianWorkspaceMutation::CreateCaptureIssue:
		{
			FExtendedAtlassianIssue Issue;
			Issue.Key = Mutation.TargetId.IsEmpty()
				? MutationField(Mutation, TEXT("key"))
				: Mutation.TargetId;
			Issue.Id = Issue.Key;
			Issue.Summary = MutationField(Mutation, TEXT("summary"));
			if (Issue.Summary.TrimStartAndEnd().IsEmpty())
			{
				Issue.Summary = Mutation.Type == EExtendedAtlassianWorkspaceMutation::CreateCaptureIssue
					? TEXT("Untitled capture from the viewport")
					: TEXT("Untitled card");
			}
			Issue.Description = MutationField(Mutation, TEXT("description"));
			Issue.StatusName = MutationField(Mutation, TEXT("status"));
			Issue.IssueTypeName = MutationField(Mutation, TEXT("type"));
			Issue.PriorityName = MutationField(Mutation, TEXT("priority"));
			Issue.AssigneeAccountId = MutationField(Mutation, TEXT("assignee"));
			Issue.EpicName = MutationField(Mutation, TEXT("epic"));
			Issue.Estimate = FCString::Atod(*MutationField(Mutation, TEXT("points")));
			Issue.RelativeUpdated = TEXT("now");
			Snapshot.Issues.Insert(MoveTemp(Issue), 0);
		}
		break;

	case EExtendedAtlassianWorkspaceMutation::UpdateIssue:
	case EExtendedAtlassianWorkspaceMutation::TransitionIssue:
		for (FExtendedAtlassianIssue& Issue : Snapshot.Issues)
		{
			if (Issue.Key != Mutation.TargetId)
			{
				continue;
			}
			const FString PreviousStatus = Issue.StatusName;
			const FString PreviousPriority = Issue.PriorityName;
			if (Mutation.Fields.Contains(TEXT("summary")))
			{
				Issue.Summary = MutationField(Mutation, TEXT("summary"));
			}
			if (Mutation.Fields.Contains(TEXT("description")))
			{
				Issue.Description = MutationField(Mutation, TEXT("description"));
			}
			if (Mutation.Fields.Contains(TEXT("type")))
			{
				Issue.IssueTypeName = MutationField(Mutation, TEXT("type"));
			}
			if (Mutation.Fields.Contains(TEXT("status")))
			{
				Issue.StatusName = MutationField(Mutation, TEXT("status"));
			}
			if (Mutation.Fields.Contains(TEXT("priority")))
			{
				Issue.PriorityName = MutationField(Mutation, TEXT("priority"));
			}
			if (Mutation.Fields.Contains(TEXT("assignee")))
			{
				Issue.AssigneeAccountId = MutationField(Mutation, TEXT("assignee"));
				if (const FExtendedAtlassianUser* Assignee =
					Snapshot.People.FindByPredicate(
						[&Issue](const FExtendedAtlassianUser& User)
						{
							return User.AccountId == Issue.AssigneeAccountId
								|| User.Initials == Issue.AssigneeAccountId;
						}))
				{
					Issue.AssigneeDisplayName = Assignee->DisplayName;
				}
				else
				{
					Issue.AssigneeDisplayName.Reset();
				}
			}
			if (Mutation.Fields.Contains(TEXT("epic")))
			{
				Issue.EpicName = MutationField(Mutation, TEXT("epic"));
				Issue.ParentSummary = Issue.EpicName;
			}
			if (Mutation.Fields.Contains(TEXT("points")))
			{
				Issue.Estimate = FCString::Atod(*MutationField(Mutation, TEXT("points")));
			}
			Issue.RelativeUpdated = TEXT("now");
			FExtendedAtlassianActivity Activity;
			Activity.Id = FString::Printf(
				TEXT("%s:optimistic:%llu"),
				*Issue.Key,
				static_cast<unsigned long long>(Mutation.ClientMutationId));
			Activity.IssueKey = Issue.Key;
			Activity.ActorAccountId = Snapshot.CurrentUser.AccountId;
			Activity.ActorDisplayName = Snapshot.CurrentUser.DisplayName;
			Activity.Created = FDateTime::UtcNow();
			Activity.RelativeTime = TEXT("now");
			if (Mutation.Fields.Contains(TEXT("status")))
			{
				Activity.Verb = TEXT("status");
				Activity.Detail = FString::Printf(
					TEXT("%s moved this from %s to %s."),
					*Snapshot.CurrentUser.DisplayName,
					*PreviousStatus,
					*Issue.StatusName);
			}
			else if (Mutation.Fields.Contains(TEXT("priority")))
			{
				Activity.Verb = TEXT("priority");
				Activity.Detail = FString::Printf(
					TEXT("%s changed priority from %s to %s."),
					*Snapshot.CurrentUser.DisplayName,
					*PreviousPriority,
					*Issue.PriorityName);
			}
			else if (Mutation.Fields.Contains(TEXT("points")))
			{
				Activity.Verb = TEXT("estimate");
				Activity.Detail = FString::Printf(
					TEXT("%s estimated this at %.0f points."),
					*Snapshot.CurrentUser.DisplayName,
					Issue.Estimate);
			}
			else
			{
				Activity.Verb = TEXT("edit");
				Activity.Detail =
					Snapshot.CurrentUser.DisplayName
					+ TEXT(" updated this issue.");
			}
			Snapshot.Activity.Insert(MoveTemp(Activity), 0);
			break;
		}
		break;

	case EExtendedAtlassianWorkspaceMutation::DeleteIssue:
		Snapshot.Issues.RemoveAll(
			[&Mutation](const FExtendedAtlassianIssue& Issue)
			{
				return Issue.Key == Mutation.TargetId;
			});
		break;

	case EExtendedAtlassianWorkspaceMutation::CreateIssueComment:
	case EExtendedAtlassianWorkspaceMutation::CreatePageComment:
		{
			const FString Target = MutationField(Mutation, TEXT("target"));
			FExtendedAtlassianCommentCollection* Collection =
				Snapshot.CommentCollections.FindByPredicate(
					[&Target](const FExtendedAtlassianCommentCollection& Candidate)
					{
						return Candidate.TargetId == Target;
					});
			if (!Collection)
			{
				FExtendedAtlassianCommentCollection NewCollection;
				NewCollection.TargetId = Target;
				Collection = &Snapshot.CommentCollections.Add_GetRef(MoveTemp(NewCollection));
			}
			const FString Body = MutationField(Mutation, TEXT("body")).TrimStartAndEnd();
			if (Body.IsEmpty())
			{
				break;
			}
			FExtendedAtlassianComment Comment;
			Comment.Id = Mutation.TargetId.IsEmpty()
				? FString::Printf(
					TEXT("comment-%llu"),
					static_cast<unsigned long long>(Mutation.ClientMutationId))
				: Mutation.TargetId;
			Comment.ParentId = Mutation.ParentId;
			Comment.AuthorAccountId = Snapshot.CurrentUser.AccountId;
			Comment.AuthorDisplayName = Snapshot.CurrentUser.DisplayName;
			Comment.Body = Body;
			Comment.RelativeTime = TEXT("now");
			Comment.bCanEdit = true;
			Comment.bCanDelete = true;
			if (Target.StartsWith(TEXT("issue:")))
			{
				FExtendedAtlassianActivity Activity;
				Activity.Id = FString::Printf(
					TEXT("%s:comment:%llu"),
					*Target,
					static_cast<unsigned long long>(
						Mutation.ClientMutationId));
				Activity.IssueKey = Target.Mid(6);
				Activity.ActorAccountId = Snapshot.CurrentUser.AccountId;
				Activity.ActorDisplayName = Snapshot.CurrentUser.DisplayName;
				Activity.Verb = TEXT("comment");
				Activity.Detail =
					Snapshot.CurrentUser.DisplayName
					+ TEXT(" commented on this issue.");
				Activity.Created = FDateTime::UtcNow();
				Activity.RelativeTime = TEXT("now");
				Snapshot.Activity.Insert(MoveTemp(Activity), 0);
			}
			if (!Mutation.ParentId.IsEmpty())
			{
				if (FExtendedAtlassianComment* Parent =
					ExtendedAtlassianModelUtils::FindComment(
						Collection->Comments,
						Mutation.ParentId))
				{
					Parent->Replies.Add(MoveTemp(Comment));
					break;
				}
			}
			Collection->Comments.Add(MoveTemp(Comment));
		}
		break;

	case EExtendedAtlassianWorkspaceMutation::UpdateIssueComment:
	case EExtendedAtlassianWorkspaceMutation::UpdatePageComment:
	case EExtendedAtlassianWorkspaceMutation::ResolveIssueComment:
	case EExtendedAtlassianWorkspaceMutation::ReopenIssueComment:
	case EExtendedAtlassianWorkspaceMutation::ResolvePageComment:
	case EExtendedAtlassianWorkspaceMutation::ReopenPageComment:
		for (FExtendedAtlassianCommentCollection& Collection : Snapshot.CommentCollections)
		{
			if (FExtendedAtlassianComment* Comment =
				ExtendedAtlassianModelUtils::FindComment(
					Collection.Comments,
					Mutation.TargetId))
			{
				if (Mutation.Fields.Contains(TEXT("body")))
				{
					Comment->Body = MutationField(Mutation, TEXT("body"));
					Comment->RelativeTime = TEXT("now");
				}
				if (Mutation.Type == EExtendedAtlassianWorkspaceMutation::ResolveIssueComment
					|| Mutation.Type == EExtendedAtlassianWorkspaceMutation::ResolvePageComment)
				{
					Comment->bResolved = true;
				}
				else if (Mutation.Type == EExtendedAtlassianWorkspaceMutation::ReopenIssueComment
					|| Mutation.Type == EExtendedAtlassianWorkspaceMutation::ReopenPageComment)
				{
					Comment->bResolved = false;
				}
				break;
			}
		}
		break;

	case EExtendedAtlassianWorkspaceMutation::DeleteIssueComment:
	case EExtendedAtlassianWorkspaceMutation::DeletePageComment:
		for (FExtendedAtlassianCommentCollection& Collection : Snapshot.CommentCollections)
		{
			if (ExtendedAtlassianModelUtils::RemoveComment(
				Collection.Comments,
				Mutation.TargetId))
			{
				break;
			}
		}
		break;

	case EExtendedAtlassianWorkspaceMutation::RankIssue:
		if (!Mutation.OrderedIds.IsEmpty())
		{
			Snapshot.Issues.StableSort(
				[&Mutation](
					const FExtendedAtlassianIssue& Left,
					const FExtendedAtlassianIssue& Right)
				{
					const int32 LeftIndex = Mutation.OrderedIds.IndexOfByKey(Left.Key);
					const int32 RightIndex = Mutation.OrderedIds.IndexOfByKey(Right.Key);
					return (LeftIndex == INDEX_NONE ? MAX_int32 : LeftIndex)
						< (RightIndex == INDEX_NONE ? MAX_int32 : RightIndex);
				});
		}
		break;

	case EExtendedAtlassianWorkspaceMutation::CreatePage:
		{
			FExtendedAtlassianPage Page;
			Page.Id = Mutation.TargetId;
			Page.ParentId = Mutation.ParentId;
			Page.Title = MutationField(Mutation, TEXT("title")).TrimStartAndEnd();
			if (Page.Title.IsEmpty())
			{
				Page.Title = TEXT("Untitled page");
			}
			Page.Version = 1;
			Page.EditedByLabel = Snapshot.CurrentUser.Initials;
			Page.EditedAtLabel = TEXT("JUST NOW");
			Snapshot.Pages.Add(MoveTemp(Page));
		}
		break;

	case EExtendedAtlassianWorkspaceMutation::UpdatePage:
	case EExtendedAtlassianWorkspaceMutation::MovePage:
	case EExtendedAtlassianWorkspaceMutation::ReorderPage:
		for (FExtendedAtlassianPage& Page : Snapshot.Pages)
		{
			if (Page.Id != Mutation.TargetId)
			{
				continue;
			}
			if (Mutation.Fields.Contains(TEXT("title")))
			{
				Page.Title = MutationField(Mutation, TEXT("title"));
			}
			if (Mutation.Fields.Contains(TEXT("body")))
			{
				Page.Body = MutationField(Mutation, TEXT("body"));
				Page.Markdown = Page.Body;
				++Page.Version;
			}
			if (!Mutation.ParentId.IsEmpty())
			{
				Page.ParentId = Mutation.ParentId;
			}
			Page.EditedByLabel = Snapshot.CurrentUser.Initials;
			Page.EditedAtLabel = TEXT("JUST NOW");
			break;
		}
		break;

	case EExtendedAtlassianWorkspaceMutation::DuplicatePage:
		for (const FExtendedAtlassianPage& Page : Snapshot.Pages)
		{
			if (Page.Id == Mutation.TargetId)
			{
				FExtendedAtlassianPage Copy = Page;
				Copy.Id = MutationField(Mutation, TEXT("newId"));
				Copy.Title = MutationField(Mutation, TEXT("title"));
				if (Copy.Title.IsEmpty())
				{
					Copy.Title = Page.Title + TEXT(" copy");
				}
				Copy.Version = 1;
				Copy.EditedByLabel = Snapshot.CurrentUser.Initials;
				Copy.EditedAtLabel = TEXT("JUST NOW");
				Snapshot.Pages.Add(MoveTemp(Copy));
				break;
			}
		}
		break;

	case EExtendedAtlassianWorkspaceMutation::DeletePage:
		Snapshot.Pages.RemoveAll(
			[&Mutation](const FExtendedAtlassianPage& Page)
			{
				return Page.Id == Mutation.TargetId;
			});
		break;

	case EExtendedAtlassianWorkspaceMutation::CreatePin:
		if (!MutationField(Mutation, TEXT("name")).TrimStartAndEnd().IsEmpty())
		{
			FExtendedAtlassianPin Pin;
			Pin.Id = Mutation.TargetId;
			Pin.DisplayName = MutationField(Mutation, TEXT("name"));
			Pin.Target.Kind = PinKindFromField(MutationField(Mutation, TEXT("kind")));
			Pin.Target.StableId = MutationField(Mutation, TEXT("stableId"));
			Pin.Target.DisplayName = Pin.DisplayName;
			Pin.Target.SecondaryId = MutationField(
				Mutation,
				TEXT("secondaryId"));
			Pin.Color = MutationField(Mutation, TEXT("color"));
			Pin.Version = 1;
			Snapshot.Pins.Insert(MoveTemp(Pin), 0);
		}
		break;

	case EExtendedAtlassianWorkspaceMutation::UpdatePin:
		for (FExtendedAtlassianPin& Pin : Snapshot.Pins)
		{
			if (Pin.Id == Mutation.TargetId)
			{
				Pin.DisplayName = MutationField(Mutation, TEXT("name"));
				Pin.Target.DisplayName = Pin.DisplayName;
				++Pin.Version;
				break;
			}
		}
		break;

	case EExtendedAtlassianWorkspaceMutation::DeletePin:
		Snapshot.Pins.RemoveAll(
			[&Mutation](const FExtendedAtlassianPin& Pin)
			{
				return Pin.Id == Mutation.TargetId;
			});
		break;

	case EExtendedAtlassianWorkspaceMutation::CreatePinReply:
		for (FExtendedAtlassianPin& Pin : Snapshot.Pins)
		{
			if (Pin.Id != Mutation.ParentId)
			{
				continue;
			}
			const FString Body = MutationField(Mutation, TEXT("body")).TrimStartAndEnd();
			if (Body.IsEmpty())
			{
				break;
			}
			FExtendedAtlassianPinThread Thread;
			Thread.Id = Mutation.TargetId.IsEmpty()
				? FString::Printf(
					TEXT("%s:thread:%llu"),
					*Pin.Id,
					static_cast<unsigned long long>(Mutation.ClientMutationId))
				: Mutation.TargetId;
			Thread.AuthorAccountId = Snapshot.CurrentUser.AccountId;
			Thread.AuthorDisplayName = Snapshot.CurrentUser.DisplayName;
			Thread.Body = Body;
			Thread.RelativeTime = TEXT("now");
			Pin.Threads.Add(MoveTemp(Thread));
			++Pin.Version;
			break;
		}
		break;

	case EExtendedAtlassianWorkspaceMutation::UpdatePinReply:
	case EExtendedAtlassianWorkspaceMutation::ResolvePinReply:
		for (FExtendedAtlassianPin& Pin : Snapshot.Pins)
		{
			for (FExtendedAtlassianPinThread& Thread : Pin.Threads)
			{
				if (Thread.Id != Mutation.TargetId)
				{
					continue;
				}
				if (Mutation.Fields.Contains(TEXT("body")))
				{
					Thread.Body = MutationField(Mutation, TEXT("body"));
				}
				if (Mutation.Type == EExtendedAtlassianWorkspaceMutation::ResolvePinReply)
				{
					Thread.bResolved =
						Mutation.Fields.Contains(TEXT("resolved"))
							? MutationField(
								Mutation,
								TEXT("resolved")).ToBool()
							: !Thread.bResolved;
				}
				Thread.RelativeTime = TEXT("now");
				++Pin.Version;
				break;
			}
		}
		break;

	case EExtendedAtlassianWorkspaceMutation::DeletePinReply:
		for (FExtendedAtlassianPin& Pin : Snapshot.Pins)
		{
			const int32 Removed = Pin.Threads.RemoveAll(
				[&Mutation](const FExtendedAtlassianPinThread& Thread)
				{
					return Thread.Id == Mutation.TargetId;
				});
			if (Removed > 0)
			{
				++Pin.Version;
				break;
			}
		}
		break;

	case EExtendedAtlassianWorkspaceMutation::MarkNotificationRead:
		for (FExtendedAtlassianNotification& Notification : Snapshot.Notifications)
		{
			if (Notification.Id == Mutation.TargetId)
			{
				Notification.bRead = true;
				break;
			}
		}
		break;

	case EExtendedAtlassianWorkspaceMutation::MarkAllNotificationsRead:
		for (FExtendedAtlassianNotification& Notification : Snapshot.Notifications)
		{
			Notification.bRead = true;
		}
		break;

	case EExtendedAtlassianWorkspaceMutation::DismissNotification:
		Snapshot.Notifications.RemoveAll(
			[&Mutation](const FExtendedAtlassianNotification& Notification)
			{
				return Notification.Id == Mutation.TargetId;
			});
		break;

	case EExtendedAtlassianWorkspaceMutation::ArchiveNotifications:
		for (FExtendedAtlassianNotification& Notification : Snapshot.Notifications)
		{
			Notification.bArchived = Notification.bArchived || Notification.bRead;
		}
		break;

	case EExtendedAtlassianWorkspaceMutation::MuteNotification:
		for (FExtendedAtlassianNotification& Notification : Snapshot.Notifications)
		{
			if (Notification.Id == Mutation.TargetId)
			{
				Notification.bRead = true;
				Notification.bMuted = true;
				break;
			}
		}
		break;

	default:
		break;
	}

	ExtendedAtlassianModelUtils::RefreshCommentPresentation(Snapshot);
	EnsureSelections();
}

void FExtendedAtlassianWorkspaceController::EnsureSelections()
{
	if (!Snapshot.Pages.ContainsByPredicate(
		[this](const FExtendedAtlassianPage& Page) { return Page.Id == SelectedPageId; }))
	{
		SelectedPageId = Snapshot.Pages.IsEmpty() ? FString() : Snapshot.Pages[0].Id;
	}

	if (!Snapshot.Issues.ContainsByPredicate(
		[this](const FExtendedAtlassianIssue& Issue) { return Issue.Key == SelectedIssueKey; }))
	{
		SelectedIssueKey = Snapshot.Issues.IsEmpty() ? FString() : Snapshot.Issues[0].Key;
	}

	if (!Snapshot.Pins.ContainsByPredicate(
		[this](const FExtendedAtlassianPin& Pin) { return Pin.Id == SelectedPinId; }))
	{
		SelectedPinId = Snapshot.Pins.IsEmpty() ? FString() : Snapshot.Pins[0].Id;
	}

	if (!Snapshot.Notifications.ContainsByPredicate(
		[this](const FExtendedAtlassianNotification& Notification)
		{
			return Notification.Id == SelectedNotificationId;
		}))
	{
		SelectedNotificationId =
			Snapshot.Notifications.IsEmpty() ? FString() : Snapshot.Notifications[0].Id;
	}
}

void FExtendedAtlassianWorkspaceController::BroadcastChanged()
{
	Changed.Broadcast();
}
