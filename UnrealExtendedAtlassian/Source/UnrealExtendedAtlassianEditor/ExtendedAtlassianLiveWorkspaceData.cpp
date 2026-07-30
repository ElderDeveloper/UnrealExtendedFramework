// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianLiveWorkspaceData.h"

#include "ExtendedAtlassianBacklotStore.h"
#include "ExtendedAtlassianClient.h"
#include "ExtendedAtlassianConfluence.h"
#include "ExtendedAtlassianConfluenceComments.h"
#include "ExtendedAtlassianInboxState.h"
#include "ExtendedAtlassianJira.h"
#include "ExtendedAtlassianJiraSoftware.h"
#include "ExtendedAtlassianSettings.h"
#include "UnrealExtendedAtlassian.h"

struct FExtendedAtlassianLiveWorkspaceData::FPendingLoad
{
	FExtendedAtlassianWorkspaceRequest Request;
	FExtendedAtlassianWorkspaceLoadDelegate Completion;
	FExtendedAtlassianWorkspaceSnapshot Snapshot;
	FExtendedAtlassianError FirstError;
	TArray<FExtendedAtlassianIssue> SprintIssues;
	TArray<FExtendedAtlassianIssueCommentMetadata> IssueCommentMetadata;
	bool bHasSprintIssues = false;
	int32 OutstandingBranches = 0;
	int32 SuccessfulBranches = 0;
};

namespace ExtendedAtlassianLiveWorkspacePrivate
{
	const TCHAR* const PresentationColumnNames[] = {
		TEXT("Triage"),
		TEXT("In progress"),
		TEXT("In review"),
		TEXT("Done")
	};

	const TCHAR* const PresentationColumnColors[] = {
		TEXT("#a2a9b4"),
		TEXT("#58a6ff"),
		TEXT("#b6a9ff"),
		TEXT("#57cc8a")
	};

	void AddUniqueInsensitive(TArray<FString>& Values, const FString& Value)
	{
		if (!Value.IsEmpty()
			&& !Values.ContainsByPredicate(
				[&Value](const FString& Existing)
				{
					return Existing.Equals(Value, ESearchCase::IgnoreCase);
				}))
		{
			Values.Add(Value);
		}
	}

	void BuildDocumentTree(
		const TArray<FExtendedAtlassianPage>& Pages,
		TArray<FExtendedAtlassianDocumentTreeNode>& OutTree)
	{
		OutTree.Reset();

		TMap<FString, int32> PageIndexById;
		for (int32 PageIndex = 0; PageIndex < Pages.Num(); ++PageIndex)
		{
			if (!Pages[PageIndex].Id.IsEmpty())
			{
				PageIndexById.FindOrAdd(Pages[PageIndex].Id) = PageIndex;
			}
		}

		TArray<int32> RootIndexes;
		TMap<FString, TArray<int32>> ChildIndexesByParent;
		for (int32 PageIndex = 0; PageIndex < Pages.Num(); ++PageIndex)
		{
			const FExtendedAtlassianPage& Page = Pages[PageIndex];
			const int32* ParentIndex = PageIndexById.Find(Page.ParentId);
			if (!Page.ParentId.IsEmpty()
				&& ParentIndex
				&& *ParentIndex != PageIndex)
			{
				ChildIndexesByParent.FindOrAdd(Page.ParentId).Add(PageIndex);
			}
			else
			{
				RootIndexes.Add(PageIndex);
			}
		}

		TSet<int32> AddedIndexes;
		TFunction<void(int32, int32, const FString&)> AppendPage;
		AppendPage =
			[&Pages, &ChildIndexesByParent, &AddedIndexes, &OutTree, &AppendPage](
				int32 PageIndex,
				int32 Depth,
				const FString& ParentId)
			{
				if (!Pages.IsValidIndex(PageIndex) || AddedIndexes.Contains(PageIndex))
				{
					return;
				}
				AddedIndexes.Add(PageIndex);

				const FExtendedAtlassianPage& Page = Pages[PageIndex];
				FExtendedAtlassianDocumentTreeNode Node;
				Node.Id = Page.Id;
				Node.Label = Page.Title;
				Node.ParentId = ParentId;
				Node.Depth = Depth;
				// Keep the space root open so its direct children are discoverable. Nested
				// parent pages start collapsed and are expanded explicitly by the user.
				Node.bExpanded = Depth == 0;
				OutTree.Add(MoveTemp(Node));

				if (const TArray<int32>* Children = ChildIndexesByParent.Find(Page.Id))
				{
					for (const int32 ChildIndex : *Children)
					{
						AppendPage(ChildIndex, Depth + 1, Page.Id);
					}
				}
			};

		for (const int32 RootIndex : RootIndexes)
		{
			AppendPage(RootIndex, 0, FString());
		}

		// Broken or cyclic parent references must not make pages disappear. Promote one
		// unvisited page to a root; the recursion still keeps any valid descendants below it.
		for (int32 PageIndex = 0; PageIndex < Pages.Num(); ++PageIndex)
		{
			if (!AddedIndexes.Contains(PageIndex))
			{
				AppendPage(PageIndex, 0, FString());
			}
		}
	}

	FString PresentationColumnForSource(const FString& SourceName)
	{
		if (SourceName.Contains(TEXT("review"), ESearchCase::IgnoreCase))
		{
			return TEXT("In review");
		}
		if (SourceName.Contains(TEXT("done"), ESearchCase::IgnoreCase)
			|| SourceName.Contains(TEXT("closed"), ESearchCase::IgnoreCase)
			|| SourceName.Contains(TEXT("resolved"), ESearchCase::IgnoreCase))
		{
			return TEXT("Done");
		}
		if (SourceName.Contains(TEXT("progress"), ESearchCase::IgnoreCase)
			|| SourceName.Contains(TEXT("development"), ESearchCase::IgnoreCase)
			|| SourceName.Contains(TEXT("doing"), ESearchCase::IgnoreCase))
		{
			return TEXT("In progress");
		}
		return TEXT("Triage");
	}

	FString InitialsForName(const FString& DisplayName)
	{
		TArray<FString> Words;
		DisplayName.ParseIntoArrayWS(Words);
		if (Words.Num() >= 2)
		{
			return (
				Words[0].Left(1) + Words.Last().Left(1)).ToUpper();
		}
		return DisplayName.Left(2).ToUpper();
	}

	/**
	 * Name the author of a page's current version.
	 *
	 * Confluence v2 returns an account id, not a name, so the provider carries the id and this
	 * resolves it against the fetched user list. Left blank when the author is not in that list:
	 * an opaque id in a "last edited by" slot is worse than an empty one.
	 */
	void ResolvePageEditor(FExtendedAtlassianWorkspaceSnapshot& Snapshot, const FString& PageId)
	{
		FExtendedAtlassianPage* Page = Snapshot.Pages.FindByPredicate(
			[&PageId](const FExtendedAtlassianPage& Candidate)
			{
				return Candidate.Id == PageId;
			});
		if (!Page || !Page->EditedByLabel.IsEmpty() || Page->EditedByAccountId.IsEmpty())
		{
			return;
		}
		const FString AccountId = Page->EditedByAccountId;
		if (const FExtendedAtlassianUser* Author = Snapshot.People.FindByPredicate(
			[&AccountId](const FExtendedAtlassianUser& User)
			{
				return User.AccountId == AccountId;
			}))
		{
			Page->EditedByLabel = Author->Initials.IsEmpty()
				? InitialsForName(Author->DisplayName)
				: Author->Initials;
		}
	}

	void NormalizeBoardColumns(
		FExtendedAtlassianWorkspaceSnapshot& Snapshot,
		const UExtendedAtlassianSettings* Settings)
	{
		const TArray<FExtendedAtlassianBoardColumn> Discovered =
			MoveTemp(Snapshot.BoardColumns);
		Snapshot.BoardColumns.Reset();
		for (int32 Index = 0;
			Index < UE_ARRAY_COUNT(PresentationColumnNames);
			++Index)
		{
			FExtendedAtlassianBoardColumn Column;
			Column.Id = PresentationColumnNames[Index];
			Column.DisplayName = PresentationColumnNames[Index];
			Column.AccentColor = PresentationColumnColors[Index];
			Column.WipLimit =
				Index == 1 && Settings
					? FMath::Max(1, Settings->InProgressWipLimit)
					: 0;
			Snapshot.BoardColumns.Add(MoveTemp(Column));
		}

		auto FindPresentation =
			[&Snapshot](const FString& Name)
				-> FExtendedAtlassianBoardColumn*
			{
				return Snapshot.BoardColumns.FindByPredicate(
					[&Name](const FExtendedAtlassianBoardColumn& Column)
					{
						return Column.DisplayName.Equals(
							Name,
							ESearchCase::IgnoreCase);
					});
			};

		if (Settings)
		{
			for (const FExtendedAtlassianBoardColumnMapping& Mapping :
				Settings->BoardColumns)
			{
				if (FExtendedAtlassianBoardColumn* Column =
					FindPresentation(Mapping.Column))
				{
					for (const FString& StatusName : Mapping.StatusNames)
					{
						AddUniqueInsensitive(Column->StatusNames, StatusName);
					}
				}
			}
		}

		for (const FExtendedAtlassianBoardColumn& Source : Discovered)
		{
			FString PresentationName =
				PresentationColumnForSource(Source.DisplayName);
			if (Settings)
			{
				if (const FExtendedAtlassianBoardColumnMapping* Mapping =
					Settings->BoardColumns.FindByPredicate(
						[&Source](
							const FExtendedAtlassianBoardColumnMapping& Candidate)
						{
							return Candidate.Column.Equals(
								Source.DisplayName,
								ESearchCase::IgnoreCase);
						}))
				{
					PresentationName = Mapping->Column;
				}
			}
			if (FExtendedAtlassianBoardColumn* Column =
				FindPresentation(PresentationName))
			{
				for (const FString& StatusId : Source.StatusIds)
				{
					AddUniqueInsensitive(Column->StatusIds, StatusId);
				}
				for (const FString& StatusName : Source.StatusNames)
				{
					AddUniqueInsensitive(Column->StatusNames, StatusName);
				}
			}
		}

		for (const FExtendedAtlassianIssue& Issue : Snapshot.Issues)
		{
			FString PresentationName;
			if (Issue.StatusName.Equals(TEXT("Blocked"), ESearchCase::IgnoreCase))
			{
				PresentationName = TEXT("Triage");
			}
			if (PresentationName.IsEmpty() && Settings)
			{
				for (const FExtendedAtlassianBoardColumnMapping& Mapping :
					Settings->BoardColumns)
				{
					if (Mapping.StatusNames.ContainsByPredicate(
						[&Issue](const FString& StatusName)
						{
							return StatusName.Equals(
								Issue.StatusName,
								ESearchCase::IgnoreCase);
						}))
					{
						PresentationName = Mapping.Column;
						break;
					}
				}
			}
			if (PresentationName.IsEmpty() && !Issue.StatusId.IsEmpty())
			{
				if (const FExtendedAtlassianBoardColumn* Source =
					Discovered.FindByPredicate(
						[&Issue](const FExtendedAtlassianBoardColumn& Column)
						{
							return Column.StatusIds.Contains(Issue.StatusId);
						}))
				{
					PresentationName =
						PresentationColumnForSource(Source->DisplayName);
				}
			}
			if (PresentationName.IsEmpty())
			{
				PresentationName =
					Issue.StatusCategoryKey.Equals(
						TEXT("done"),
						ESearchCase::IgnoreCase)
						? TEXT("Done")
						: (Issue.StatusName.Contains(
							TEXT("review"),
							ESearchCase::IgnoreCase)
							? TEXT("In review")
							: (Issue.StatusCategoryKey.Equals(
								TEXT("new"),
								ESearchCase::IgnoreCase)
								? TEXT("Triage")
								: TEXT("In progress")));
			}
			if (FExtendedAtlassianBoardColumn* Column =
				FindPresentation(PresentationName))
			{
				AddUniqueInsensitive(Column->StatusNames, Issue.StatusName);
				AddUniqueInsensitive(Column->StatusIds, Issue.StatusId);
			}
		}

		// Blocked is a presentation exception even when Jira configured it elsewhere.
		if (FExtendedAtlassianBoardColumn* Triage =
			FindPresentation(TEXT("Triage")))
		{
			AddUniqueInsensitive(Triage->StatusNames, TEXT("Blocked"));
		}
	}

	void BuildLiveSprintPresentation(
		FExtendedAtlassianWorkspaceSnapshot& Snapshot)
	{
		const FExtendedAtlassianSprint* Sprint =
			Snapshot.Sprints.FindByPredicate(
				[&Snapshot](const FExtendedAtlassianSprint& Candidate)
				{
					return Candidate.Id == Snapshot.SelectedSprintId;
				});
		if (Sprint)
		{
			const FDateTime Now = FDateTime::UtcNow();
			const int32 DaysLeft =
				Sprint->EndDate == FDateTime::MinValue()
					? 0
					: FMath::Max(
						0,
						FMath::CeilToInt(
							(Sprint->EndDate - Now).GetTotalDays()));
			Snapshot.SprintSummary.DaysLeft =
				FString::Printf(TEXT("%dd LEFT"), DaysLeft);
			if (Sprint->StartDate != FDateTime::MinValue()
				&& Sprint->EndDate != FDateTime::MinValue())
			{
				Snapshot.SprintSummary.DateRange = FString::Printf(
					TEXT("%s \u2013 %s"),
					*Sprint->StartDate.ToString(TEXT("%d %b")).ToUpper(),
					*Sprint->EndDate.ToString(TEXT("%d %b")).ToUpper());
			}
			Snapshot.SprintSummary.Goal = Sprint->Goal.ToUpper();
		}

		int32 Blocked = 0;
		for (const FExtendedAtlassianIssue& Issue : Snapshot.Issues)
		{
			if (Issue.StatusCategoryKey.Equals(
				TEXT("done"),
				ESearchCase::IgnoreCase))
			{
				++Snapshot.SprintSummary.Done;
			}
			else if (Issue.StatusName.Equals(
				TEXT("Blocked"),
				ESearchCase::IgnoreCase))
			{
				++Blocked;
			}
			else if (Issue.StatusCategoryKey.Equals(
				TEXT("new"),
				ESearchCase::IgnoreCase))
			{
				++Snapshot.SprintSummary.Left;
			}
			else
			{
				++Snapshot.SprintSummary.Wip;
			}
		}
		const double Total =
			FMath::Max(1, Snapshot.Issues.Num());
		Snapshot.SprintSummary.DoneFraction =
			Snapshot.SprintSummary.Done / Total;
		Snapshot.SprintSummary.WipFraction =
			Snapshot.SprintSummary.Wip / Total;
		Snapshot.SprintSummary.BlockedFraction = Blocked / Total;

		const TCHAR* AvatarBackgrounds[] = {
			TEXT("#3b4a63"), TEXT("#4b3b57"), TEXT("#58483a"),
			TEXT("#334d47"), TEXT("#494552")
		};
		const TCHAR* AvatarForegrounds[] = {
			TEXT("#cfe0ff"), TEXT("#e3c8f0"), TEXT("#f0d4bb"),
			TEXT("#bce5d9"), TEXT("#ddd5e8")
		};
		TArray<FExtendedAtlassianUser> TeamPeople = Snapshot.People;
		for (const FExtendedAtlassianIssue& Issue : Snapshot.Issues)
		{
			if (Issue.AssigneeAccountId.IsEmpty()
				|| TeamPeople.ContainsByPredicate(
					[&Issue](const FExtendedAtlassianUser& Person)
					{
						return Person.AccountId == Issue.AssigneeAccountId;
					}))
			{
				continue;
			}
			FExtendedAtlassianUser Person;
			Person.AccountId = Issue.AssigneeAccountId;
			Person.DisplayName = Issue.AssigneeDisplayName.IsEmpty()
				? Issue.AssigneeAccountId
				: Issue.AssigneeDisplayName;
			Person.AvatarUrl = Issue.AssigneeAvatarUrl;
			TeamPeople.Add(MoveTemp(Person));
		}
		for (const FExtendedAtlassianUser& Person : TeamPeople)
		{
			const bool bHasSprintWork =
				Snapshot.Issues.ContainsByPredicate(
					[&Person](const FExtendedAtlassianIssue& Issue)
					{
						return Issue.AssigneeAccountId == Person.AccountId;
					});
			if (!bHasSprintWork)
			{
				continue;
			}
			FExtendedAtlassianTeamLoad Load;
			Load.User = Person;
			if (Load.User.Initials.IsEmpty())
			{
				Load.User.Initials =
					InitialsForName(Load.User.DisplayName);
			}
			const int32 ColorIndex =
				Snapshot.TeamLoad.Num()
				% UE_ARRAY_COUNT(AvatarBackgrounds);
			if (Load.User.AvatarBackground.IsEmpty())
			{
				Load.User.AvatarBackground =
					AvatarBackgrounds[ColorIndex];
			}
			if (Load.User.AvatarForeground.IsEmpty())
			{
				Load.User.AvatarForeground =
					AvatarForegrounds[ColorIndex];
			}
			for (const FExtendedAtlassianIssue& Issue : Snapshot.Issues)
			{
				if (Issue.AssigneeAccountId == Person.AccountId
					&& !Issue.StatusCategoryKey.Equals(
						TEXT("done"),
						ESearchCase::IgnoreCase))
				{
					Load.OpenPoints += Issue.Estimate;
				}
			}
			Load.Fraction = Load.OpenPoints / 24.0;
			Load.ThresholdColor =
				Load.Fraction >= 0.95
					? TEXT("#f0665f")
					: (Load.Fraction >= 0.70
						? TEXT("#e3a54a")
						: TEXT("#57cc8a"));
			Snapshot.TeamLoad.Add(MoveTemp(Load));
		}
	}
}

FExtendedAtlassianLiveWorkspaceData::FExtendedAtlassianLiveWorkspaceData()
{
	Capabilities.bCanReadIssues = true;
	Capabilities.bCanCreateIssues = true;
	Capabilities.bCanEditIssues = true;
	Capabilities.bCanDeleteIssues = true;
	Capabilities.bCanAssignIssues = true;
	Capabilities.bCanTransitionIssues = true;
	Capabilities.bCanRankIssues = true;
	Capabilities.bCanReadBoards = true;
	Capabilities.bCanReadPages = true;
	Capabilities.bCanEditPages = true;
	Capabilities.bCanDeletePages = true;
	Capabilities.bCanComment = true;
}

void FExtendedAtlassianLiveWorkspaceData::Load(
	const FExtendedAtlassianWorkspaceRequest& Request,
	FExtendedAtlassianWorkspaceLoadDelegate Completion)
{
	CancelledGenerations.Remove(Request.Generation);
	const TSharedPtr<FExtendedAtlassianClient> Client =
		FUnrealExtendedAtlassianModule::GetClient();
	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();

	if (!Client.IsValid() || !Settings || !Settings->IsConfigured() || !Client->HasCredentials())
	{
		FExtendedAtlassianWorkspaceSnapshot Result;
		Result.State = EExtendedAtlassianLoadState::Offline;
		Result.Error.Code = TEXT("NotConfigured");
		Result.Error.Message =
			TEXT("Connect an Atlassian Cloud account in Project Settings > Plugins > Extended Atlassian.");
		Completion.ExecuteIfBound(Request, Result);
		return;
	}

	const TSharedRef<FPendingLoad> Pending = MakeShared<FPendingLoad>();
	Pending->Request = Request;
	Pending->Completion = MoveTemp(Completion);
	Pending->Snapshot.State = EExtendedAtlassianLoadState::Loading;
	Pending->Snapshot.Capabilities = Capabilities;
	Pending->Snapshot.CurrentUser = Client->GetVerifiedUser();
	if (Pending->Snapshot.CurrentUser.Initials.IsEmpty())
	{
		Pending->Snapshot.CurrentUser.Initials =
			ExtendedAtlassianLiveWorkspacePrivate::InitialsForName(
				Pending->Snapshot.CurrentUser.DisplayName);
	}
	if (Pending->Snapshot.CurrentUser.AvatarBackground.IsEmpty())
	{
		Pending->Snapshot.CurrentUser.AvatarBackground = TEXT("#3b4a63");
	}
	if (Pending->Snapshot.CurrentUser.AvatarForeground.IsEmpty())
	{
		Pending->Snapshot.CurrentUser.AvatarForeground = TEXT("#cfe0ff");
	}
	InboxAccountId = Pending->Snapshot.CurrentUser.AccountId;
	FString InboxLoadError;
	if (!FExtendedAtlassianInboxState::Load(
		InboxAccountId,
		InboxUserState,
		InboxLoadError))
	{
		// Corruption/newer-schema recovery is intentionally fail-safe: treating
		// the current remote history as the first run prevents an unread flood.
		InboxUserState = FExtendedAtlassianInboxUserState();
	}
	const bool bHasProject = !Settings->ProjectKey.IsEmpty();
	const bool bHasBoard = !Settings->BoardId.IsEmpty();
	const bool bHasSharedMetadata = !Settings->BacklotMetadataPageId.IsEmpty();
	const TCHAR* ViewIds[] = {
		TEXT("sprint"), TEXT("mine"), TEXT("triage"), TEXT("blocked"), TEXT("docs")
	};
	const TCHAR* ViewLabels[] = {
		TEXT("Sprint"), TEXT("Assigned to me"), TEXT("Needs triage"),
		TEXT("Blocked"), TEXT("Docs to write")
	};
	const TCHAR* ViewColors[] = {
		TEXT("#58a6ff"), TEXT("#b6a9ff"), TEXT("#a2a9b4"),
		TEXT("#f0665f"), TEXT("#e3a54a")
	};
	const TCHAR* ViewPredicates[] = {
		TEXT("sprint in openSprints()"),
		TEXT("assignee = currentUser() AND resolution = Unresolved"),
		TEXT("status = Triage"),
		TEXT("status = Blocked"),
		TEXT("issuetype = Doc AND resolution = Unresolved")
	};
	for (int32 ViewIndex = 0; ViewIndex < UE_ARRAY_COUNT(ViewIds); ++ViewIndex)
	{
		FExtendedAtlassianIssueView View;
		View.Id = ViewIds[ViewIndex];
		View.Label = ViewLabels[ViewIndex];
		View.DotColor = ViewColors[ViewIndex];
		const FExtendedAtlassianJqlPreset* Configured =
			Settings->JqlPresets.FindByPredicate(
				[Label = View.Label](const FExtendedAtlassianJqlPreset& Preset)
				{
					return Preset.Name.Equals(Label, ESearchCase::IgnoreCase);
				});
		if (Configured && !Configured->Jql.IsEmpty())
		{
			View.Jql = Configured->Jql;
		}
		else
		{
			const FString Predicate = ViewPredicates[ViewIndex];
			View.Jql = Settings->ProjectKey.IsEmpty()
				? Predicate
				: FString::Printf(
					TEXT("project = \"%s\" AND (%s)"),
					*Settings->ProjectKey.ReplaceCharWithEscapedChar(),
					*Predicate);
		}
		Pending->Snapshot.IssueViews.Add(MoveTemp(View));
	}
	Capabilities.bCanUseSharedMetadata = bHasSharedMetadata;
	Pending->Snapshot.Capabilities = Capabilities;
	Pending->OutstandingBranches =
		(Request.SelectedIssueKey.IsEmpty() ? 2 : 4)
		+ (Request.SelectedPageId.IsEmpty() ? 0 : 1)
		+ 1
		+ (bHasProject ? 3 : 0)
		+ (bHasBoard ? 2 : 0)
		+ (bHasSharedMetadata ? 1 : 0)
		+ (!Request.SelectedIssueKey.IsEmpty() && bHasSharedMetadata ? 1 : 0)
		+ Pending->Snapshot.IssueViews.Num();
	const TWeakPtr<FExtendedAtlassianLiveWorkspaceData> WeakProvider = AsShared();

	const FString Jql = Settings->ProjectKey.IsEmpty()
		? TEXT("ORDER BY updated DESC")
		: FString::Printf(
			TEXT("project = \"%s\" ORDER BY updated DESC"),
			*Settings->ProjectKey.ReplaceCharWithEscapedChar());
	FExtendedAtlassianJira::SearchIssues(
		Jql,
		200,
		FExtendedAtlassianIssuesDelegate::CreateLambda(
			[WeakProvider, Pending](const FExtendedAtlassianIssueQueryResult& Result)
			{
				const TSharedPtr<FExtendedAtlassianLiveWorkspaceData> Self = WeakProvider.Pin();
				if (!Self.IsValid())
				{
					return;
				}
				if (Result.bSuccess)
				{
					Pending->Snapshot.Issues = Result.Issues;
					Self->LastIssues = Result.Issues;
				}
				Self->FinishLoadBranch(Pending, Result.bSuccess, Result.Error);
			}));
	for (int32 ViewIndex = 0;
		ViewIndex < Pending->Snapshot.IssueViews.Num();
		++ViewIndex)
	{
		FExtendedAtlassianJira::SearchIssues(
			Pending->Snapshot.IssueViews[ViewIndex].Jql,
			500,
			FExtendedAtlassianIssuesDelegate::CreateLambda(
				[WeakProvider, Pending, ViewIndex](
					const FExtendedAtlassianIssueQueryResult& Result)
				{
					const TSharedPtr<FExtendedAtlassianLiveWorkspaceData> Self =
						WeakProvider.Pin();
					if (!Self.IsValid())
					{
						return;
					}
					if (Result.bSuccess
						&& Pending->Snapshot.IssueViews.IsValidIndex(ViewIndex))
					{
						FExtendedAtlassianIssueView& View =
							Pending->Snapshot.IssueViews[ViewIndex];
						View.AuthoredCount = Result.Issues.Num();
						for (const FExtendedAtlassianIssue& Issue : Result.Issues)
						{
							View.IssueKeys.Add(Issue.Key);
						}
					}
					Self->FinishLoadBranch(
						Pending,
						Result.bSuccess,
						Result.Error);
				}));
	}

	FExtendedAtlassianJira::GetPriorities(
		FExtendedAtlassianPrioritiesDelegate::CreateLambda(
			[WeakProvider, Pending](
				bool bSuccess,
				const TArray<FExtendedAtlassianPriority>& Priorities,
				const FExtendedAtlassianError& Error)
			{
				const TSharedPtr<FExtendedAtlassianLiveWorkspaceData> Self = WeakProvider.Pin();
				if (!Self.IsValid())
				{
					return;
				}
				if (bSuccess)
				{
					Pending->Snapshot.Priorities = Priorities;
				}
				Self->FinishLoadBranch(Pending, bSuccess, Error);
			}));

	if (bHasProject)
	{
		FExtendedAtlassianJira::GetIssueTypes(
			Settings->ProjectKey,
			FExtendedAtlassianIssueTypesDelegate::CreateLambda(
				[WeakProvider, Pending](
					bool bSuccess,
					const TArray<FExtendedAtlassianIssueType>& IssueTypes,
					const FExtendedAtlassianError& Error)
				{
					const TSharedPtr<FExtendedAtlassianLiveWorkspaceData> Self =
						WeakProvider.Pin();
					if (!Self.IsValid())
					{
						return;
					}
					if (bSuccess)
					{
						Pending->Snapshot.IssueTypes = IssueTypes;
					}
					Self->FinishLoadBranch(Pending, bSuccess, Error);
				}));
		FExtendedAtlassianJira::GetAssignableUsers(
			Settings->ProjectKey,
			FExtendedAtlassianUsersDelegate::CreateLambda(
				[WeakProvider, Pending](
					bool bSuccess,
					const TArray<FExtendedAtlassianUser>& Users,
					const FExtendedAtlassianError& Error)
				{
					const TSharedPtr<FExtendedAtlassianLiveWorkspaceData> Self =
						WeakProvider.Pin();
					if (!Self.IsValid())
					{
						return;
					}
					if (bSuccess)
					{
						Pending->Snapshot.People = Users;
						Self->LastPeople = Users;
					}
					Self->FinishLoadBranch(Pending, bSuccess, Error);
				}));
		FExtendedAtlassianJiraSoftware::ListBoards(
			Settings->ProjectKey,
			FExtendedAtlassianBoardsDelegate::CreateLambda(
				[WeakProvider, Pending](
					bool bSuccess,
					const TArray<FExtendedAtlassianBoard>& Boards,
					const FExtendedAtlassianError& Error)
				{
					const TSharedPtr<FExtendedAtlassianLiveWorkspaceData> Self =
						WeakProvider.Pin();
					if (!Self.IsValid())
					{
						return;
					}
					if (bSuccess)
					{
						Pending->Snapshot.Boards = Boards;
					}
					Self->FinishLoadBranch(Pending, bSuccess, Error);
				}));
	}

	if (bHasBoard)
	{
		FExtendedAtlassianJiraSoftware::GetBoardConfiguration(
			Settings->BoardId,
			FExtendedAtlassianBoardConfigurationDelegate::CreateLambda(
				[WeakProvider, Pending](
					bool bSuccess,
					const FExtendedAtlassianBoardConfiguration& Configuration,
					const FExtendedAtlassianError& Error)
				{
					const TSharedPtr<FExtendedAtlassianLiveWorkspaceData> Self =
						WeakProvider.Pin();
					if (!Self.IsValid())
					{
						return;
					}
					if (bSuccess)
					{
						Pending->Snapshot.BoardColumns = Configuration.Columns;
						Self->Capabilities.bCanRankIssues = Configuration.CanRank();
					}
					else
					{
						Self->Capabilities.bCanRankIssues = false;
					}
					Pending->Snapshot.Capabilities = Self->Capabilities;
					Self->FinishLoadBranch(Pending, bSuccess, Error);
				}));

		FExtendedAtlassianJiraSoftware::ListSprints(
			Settings->BoardId,
			FExtendedAtlassianSprintsDelegate::CreateLambda(
				[
					WeakProvider,
					Pending,
					SprintSelection = Settings->SprintSelection
				](
					bool bSuccess,
					const TArray<FExtendedAtlassianSprint>& Sprints,
					const FExtendedAtlassianError& Error)
				{
					const TSharedPtr<FExtendedAtlassianLiveWorkspaceData> Self =
						WeakProvider.Pin();
					if (!Self.IsValid())
					{
						return;
					}
					if (!bSuccess)
					{
						Self->FinishLoadBranch(Pending, false, Error);
						return;
					}
					Pending->Snapshot.Sprints = Sprints;
					const FExtendedAtlassianSprint* Sprint = Sprints.FindByPredicate(
						[&SprintSelection](const FExtendedAtlassianSprint& Candidate)
						{
							return SprintSelection.IsEmpty()
								|| SprintSelection.Equals(
									TEXT("active"),
									ESearchCase::IgnoreCase)
									? Candidate.State.Equals(
										TEXT("active"),
										ESearchCase::IgnoreCase)
									: Candidate.Id == SprintSelection;
						});
					if (!Sprint)
					{
						Self->FinishLoadBranch(
							Pending,
							true,
							FExtendedAtlassianError());
						return;
					}
					Pending->Snapshot.SelectedSprintId = Sprint->Id;
					FExtendedAtlassianJiraSoftware::GetSprintIssues(
						Sprint->Id,
						500,
						FExtendedAtlassianIssuesDelegate::CreateLambda(
							[WeakProvider,
								Pending,
								SprintId = Sprint->Id,
								SprintName = Sprint->Name](
								const FExtendedAtlassianIssueQueryResult& Result)
							{
								const TSharedPtr<FExtendedAtlassianLiveWorkspaceData> InnerSelf =
									WeakProvider.Pin();
								if (!InnerSelf.IsValid())
								{
									return;
								}
								if (Result.bSuccess)
								{
									Pending->SprintIssues = Result.Issues;
									for (FExtendedAtlassianIssue& Issue :
										Pending->SprintIssues)
									{
										Issue.SprintId = SprintId;
										Issue.SprintName = SprintName;
									}
									Pending->bHasSprintIssues = true;
								}
								InnerSelf->FinishLoadBranch(
									Pending,
									Result.bSuccess,
									Result.Error);
							}));
				}));
	}

	FExtendedAtlassianConfluence::ListSpaces(
		FExtendedAtlassianSpacesDelegate::CreateLambda(
			[WeakProvider, Pending, PrimaryKey = Settings->PrimarySpaceKey](
				bool bSuccess,
				const TArray<FExtendedAtlassianSpace>& Spaces,
				const FExtendedAtlassianError& Error)
			{
				const TSharedPtr<FExtendedAtlassianLiveWorkspaceData> Self = WeakProvider.Pin();
				if (!Self.IsValid())
				{
					return;
				}
				if (!bSuccess || Spaces.IsEmpty())
				{
					Self->FinishLoadBranch(Pending, bSuccess, Error);
					return;
				}

				const FExtendedAtlassianSpace* Space = Spaces.FindByPredicate(
					[&PrimaryKey](const FExtendedAtlassianSpace& Candidate)
					{
						return !PrimaryKey.IsEmpty() && Candidate.Key == PrimaryKey;
					});
				if (!Space)
				{
					Space = &Spaces[0];
				}
				Self->LastSpaceId = Space->Id;
				Pending->Snapshot.ConfluenceSpaceId = Space->Id;
				Pending->Snapshot.ConfluenceSpaceKey = Space->Key;
				Pending->Snapshot.ConfluenceSpaceName =
					Space->Name.IsEmpty() ? Space->Key : Space->Name;

				FExtendedAtlassianConfluence::ListPages(
					Space->Id,
					FExtendedAtlassianPagesDelegate::CreateLambda(
						[WeakProvider, Pending](
							bool bPagesSuccess,
							const TArray<FExtendedAtlassianPage>& Pages,
							const FExtendedAtlassianError& PagesError)
						{
							const TSharedPtr<FExtendedAtlassianLiveWorkspaceData> InnerSelf =
								WeakProvider.Pin();
							if (!InnerSelf.IsValid())
							{
								return;
							}
							if (bPagesSuccess)
							{
								Pending->Snapshot.Pages = Pages;
								ExtendedAtlassianLiveWorkspacePrivate::BuildDocumentTree(
									Pages,
									Pending->Snapshot.DocumentTree);
							}

							FString PageIdToLoad;
							if (bPagesSuccess && !Pages.IsEmpty())
							{
								const FExtendedAtlassianPage* RequestedPage =
									Pages.FindByPredicate(
										[&Pending](const FExtendedAtlassianPage& Page)
										{
											return Page.Id
												== Pending->Request.SelectedPageId;
										});
								PageIdToLoad = RequestedPage
									? RequestedPage->Id
									: Pages[0].Id;
							}

							if (!PageIdToLoad.IsEmpty())
							{
								FExtendedAtlassianConfluence::GetPage(
									PageIdToLoad,
									FExtendedAtlassianPageDelegate::CreateLambda(
										[WeakProvider, Pending](
											bool bPageSuccess,
											const FExtendedAtlassianPage& Page,
											const FExtendedAtlassianError& PageError)
										{
											const TSharedPtr<FExtendedAtlassianLiveWorkspaceData> FinalSelf =
												WeakProvider.Pin();
											if (!FinalSelf.IsValid())
											{
												return;
											}
											if (bPageSuccess)
											{
												if (FExtendedAtlassianPage* Existing =
													Pending->Snapshot.Pages.FindByPredicate(
														[&Page](const FExtendedAtlassianPage& Candidate)
														{
															return Candidate.Id == Page.Id;
														}))
												{
													*Existing = Page;
												}
												else
												{
													Pending->Snapshot.Pages.Add(Page);
												}
												ExtendedAtlassianLiveWorkspacePrivate::ResolvePageEditor(
													Pending->Snapshot,
													Page.Id);
											}
											FinalSelf->FinishLoadBranch(
												Pending,
												bPageSuccess,
												PageError);
										}));
								return;
							}
							InnerSelf->FinishLoadBranch(Pending, bPagesSuccess, PagesError);
						}));
			}));

	if (!Request.SelectedIssueKey.IsEmpty())
	{
		FExtendedAtlassianJira::GetComments(
			Request.SelectedIssueKey,
			FExtendedAtlassianCommentsDelegate::CreateLambda(
				[WeakProvider, Pending, IssueKey = Request.SelectedIssueKey](
					bool bSuccess,
					const TArray<FExtendedAtlassianComment>& Comments,
					const FExtendedAtlassianError& Error)
				{
					const TSharedPtr<FExtendedAtlassianLiveWorkspaceData> Self = WeakProvider.Pin();
					if (!Self.IsValid())
					{
						return;
					}
					if (bSuccess)
					{
						FExtendedAtlassianCommentCollection Collection;
						Collection.TargetId = TEXT("issue:") + IssueKey;
						Collection.Comments = Comments;
						Pending->Snapshot.CommentCollections.Add(MoveTemp(Collection));
						for (const FExtendedAtlassianComment& Comment : Comments)
						{
							FExtendedAtlassianActivity Activity;
							Activity.Id = TEXT("jira-comment:") + Comment.Id;
							Activity.IssueKey = IssueKey;
							Activity.ActorAccountId = Comment.AuthorAccountId;
							Activity.ActorDisplayName =
								Comment.AuthorDisplayName;
							Activity.Verb = Comment.Body.Contains(
								TEXT("[viewport capture attached]"))
									? TEXT("capture")
									: TEXT("comment");
							Activity.Detail =
								Comment.AuthorDisplayName
								+ (Activity.Verb == TEXT("capture")
									? TEXT(" attached a viewport capture in a comment.")
									: TEXT(" commented on this issue."));
							Activity.Created = Comment.Created;
							Activity.RelativeTime = Comment.RelativeTime;
							Pending->Snapshot.Activity.Add(MoveTemp(Activity));
						}
					}
					Self->FinishLoadBranch(Pending, bSuccess, Error);
				}));
		FExtendedAtlassianJira::GetChangelog(
			Request.SelectedIssueKey,
			FExtendedAtlassianActivitiesDelegate::CreateLambda(
				[WeakProvider, Pending](
					bool bSuccess,
					const TArray<FExtendedAtlassianActivity>& Activity,
					const FExtendedAtlassianError& Error)
				{
					const TSharedPtr<FExtendedAtlassianLiveWorkspaceData> Self =
						WeakProvider.Pin();
					if (!Self.IsValid())
					{
						return;
					}
					if (bSuccess)
					{
						Pending->Snapshot.Activity.Append(Activity);
					}
					Self->FinishLoadBranch(Pending, bSuccess, Error);
				}));
	}

	if (!Request.SelectedPageId.IsEmpty())
	{
		FExtendedAtlassianConfluenceComments::GetPageComments(
			Request.SelectedPageId,
			FExtendedAtlassianCommentsDelegate::CreateLambda(
				[WeakProvider, Pending, PageId = Request.SelectedPageId](
					bool bSuccess,
					const TArray<FExtendedAtlassianComment>& Comments,
					const FExtendedAtlassianError& Error)
				{
					const TSharedPtr<FExtendedAtlassianLiveWorkspaceData> Self =
						WeakProvider.Pin();
					if (!Self.IsValid())
					{
						return;
					}
					if (bSuccess)
					{
						FExtendedAtlassianCommentCollection Collection;
						Collection.TargetId = TEXT("page:") + PageId;
						Collection.Comments = Comments;
						Pending->Snapshot.CommentCollections.Add(MoveTemp(Collection));
						Self->LastPageComments.Reset();
						for (const FExtendedAtlassianComment& Comment : Comments)
						{
							Self->LastPageComments.Add(Comment.Id, Comment);
							for (const FExtendedAtlassianComment& Reply : Comment.Replies)
							{
								Self->LastPageComments.Add(Reply.Id, Reply);
							}
						}
					}
					Self->FinishLoadBranch(Pending, bSuccess, Error);
				}));
	}

	if (bHasSharedMetadata)
	{
		FExtendedAtlassianBacklotStore::LoadPins(
			Settings->BacklotMetadataPageId,
			FExtendedAtlassianPinsDelegate::CreateLambda(
				[WeakProvider, Pending](
					bool bSuccess,
					const TArray<FExtendedAtlassianPin>& Pins,
					const FExtendedAtlassianError& Error)
				{
					const TSharedPtr<FExtendedAtlassianLiveWorkspaceData> Self =
						WeakProvider.Pin();
					if (!Self.IsValid())
					{
						return;
					}
					if (bSuccess)
					{
						Pending->Snapshot.Pins = Pins;
						Self->LastPins = Pins;
						const FString& SelectedIssueKey =
							Pending->Request.SelectedIssueKey;
						if (!SelectedIssueKey.IsEmpty())
						{
							for (const FExtendedAtlassianPin& Pin : Pins)
							{
								for (const FExtendedAtlassianPinThread& Message :
									Pin.Threads)
								{
									const bool bLinked =
										Message.LinkedLabel.Contains(
											SelectedIssueKey,
											ESearchCase::IgnoreCase)
										|| Message.Body.Contains(
											SelectedIssueKey,
											ESearchCase::IgnoreCase)
										|| Pin.DisplayName.Contains(
											SelectedIssueKey,
											ESearchCase::IgnoreCase);
									if (!bLinked)
									{
										continue;
									}
									FExtendedAtlassianIssueThread Thread;
									Thread.Id = Pin.Id + TEXT(":") + Message.Id;
									Thread.IssueKey = SelectedIssueKey;
									Thread.AuthorAccountId =
										Message.AuthorAccountId;
									Thread.AuthorDisplayName =
										Message.AuthorDisplayName;
									Thread.RelativeTime =
										Message.RelativeTime;
									Thread.Label =
										Message.LinkedLabel.IsEmpty()
											? Pin.DisplayName.ToUpper()
											: Message.LinkedLabel.ToUpper();
									Thread.Body = Message.Body;
									Thread.AccentColor = Pin.Color;
									Thread.bResolved = Message.bResolved;
									Pending->Snapshot.IssueThreads.Add(
										MoveTemp(Thread));

									FExtendedAtlassianActivity Activity;
									Activity.Id =
										TEXT("pin:") + Pin.Id + TEXT(":")
										+ Message.Id;
									Activity.IssueKey = SelectedIssueKey;
									Activity.ActorAccountId =
										Message.AuthorAccountId;
									Activity.ActorDisplayName =
										Message.AuthorDisplayName;
									Activity.Verb = TEXT("pin");
									Activity.Detail = FString::Printf(
										TEXT("%s opened a viewport thread on %s."),
										*Message.AuthorDisplayName,
										*Pin.DisplayName);
									Activity.Created = Message.Created;
									Activity.RelativeTime =
										Message.RelativeTime;
									Pending->Snapshot.Activity.Add(
										MoveTemp(Activity));
								}
							}
						}
					}
					Self->FinishLoadBranch(Pending, bSuccess, Error);
				}));
		if (!Request.SelectedIssueKey.IsEmpty())
		{
			FExtendedAtlassianBacklotStore::LoadIssueCommentMetadata(
				Settings->BacklotMetadataPageId,
				FExtendedAtlassianIssueCommentMetadataDelegate::CreateLambda(
					[WeakProvider, Pending](
						bool bSuccess,
						const TArray<FExtendedAtlassianIssueCommentMetadata>&
							Metadata,
						const FExtendedAtlassianError& Error)
					{
						const TSharedPtr<FExtendedAtlassianLiveWorkspaceData>
							Self = WeakProvider.Pin();
						if (!Self.IsValid())
						{
							return;
						}
						if (bSuccess)
						{
							Pending->IssueCommentMetadata = Metadata;
							Self->LastIssueCommentMetadata = Metadata;
						}
						Self->FinishLoadBranch(Pending, bSuccess, Error);
					}));
		}
	}
}

void FExtendedAtlassianLiveWorkspaceData::FinishLoadBranch(
	const TSharedRef<FPendingLoad>& Pending,
	bool bSuccess,
	const FExtendedAtlassianError& Error)
{
	if (bSuccess)
	{
		++Pending->SuccessfulBranches;
	}
	else if (!Pending->FirstError.IsSet())
	{
		Pending->FirstError = Error;
	}

	if (--Pending->OutstandingBranches > 0)
	{
		return;
	}
	if (CancelledGenerations.Contains(Pending->Request.Generation))
	{
		return;
	}
	if (!Pending->Request.SelectedIssueKey.IsEmpty()
		&& !Pending->IssueCommentMetadata.IsEmpty())
	{
		const FString Scope =
			TEXT("issue:") + Pending->Request.SelectedIssueKey;
		if (FExtendedAtlassianCommentCollection* Collection =
			Pending->Snapshot.CommentCollections.FindByPredicate(
				[&Scope](const FExtendedAtlassianCommentCollection& Candidate)
				{
					return Candidate.TargetId == Scope;
				}))
		{
			for (const FExtendedAtlassianIssueCommentMetadata& Metadata :
				Pending->IssueCommentMetadata)
			{
				if (Metadata.IssueKey != Pending->Request.SelectedIssueKey)
				{
					continue;
				}
				if (FExtendedAtlassianComment* Comment =
					Collection->Comments.FindByPredicate(
						[&Metadata](const FExtendedAtlassianComment& Candidate)
						{
							return Candidate.Id == Metadata.CommentId;
						}))
				{
					Comment->bResolved = Metadata.bResolved;
				}
			}
			for (const FExtendedAtlassianIssueCommentMetadata& Metadata :
				Pending->IssueCommentMetadata)
			{
				if (Metadata.IssueKey != Pending->Request.SelectedIssueKey
					|| Metadata.ParentId.IsEmpty())
				{
					continue;
				}
				const int32 ChildIndex =
					Collection->Comments.IndexOfByPredicate(
						[&Metadata](const FExtendedAtlassianComment& Candidate)
						{
							return Candidate.Id == Metadata.CommentId;
						});
				if (ChildIndex == INDEX_NONE)
				{
					continue;
				}
				FExtendedAtlassianComment Child =
					MoveTemp(Collection->Comments[ChildIndex]);
				Collection->Comments.RemoveAt(ChildIndex);
				if (FExtendedAtlassianComment* Parent =
					Collection->Comments.FindByPredicate(
						[&Metadata](const FExtendedAtlassianComment& Candidate)
						{
							return Candidate.Id == Metadata.ParentId;
						}))
				{
					Child.ParentId = Metadata.ParentId;
					Parent->Replies.Add(MoveTemp(Child));
				}
				else
				{
					Collection->Comments.Add(MoveTemp(Child));
				}
			}
		}
	}

	if (Pending->SuccessfulBranches == 0)
	{
		Pending->Snapshot.State =
			Pending->FirstError.Code == TEXT("Network")
				? EExtendedAtlassianLoadState::Offline
				: Pending->FirstError.Code == TEXT("Forbidden")
				? EExtendedAtlassianLoadState::PermissionDenied
				: EExtendedAtlassianLoadState::Error;
		Pending->Snapshot.Error = Pending->FirstError;
	}
	else
	{
		if (Pending->Request.Route == EExtendedAtlassianWorkspaceRoute::Board
			&& Pending->bHasSprintIssues)
		{
			Pending->Snapshot.Issues = MoveTemp(Pending->SprintIssues);
		}
		Pending->Snapshot.State =
			Pending->Snapshot.Issues.IsEmpty() && Pending->Snapshot.Pages.IsEmpty()
				? EExtendedAtlassianLoadState::Empty
				: EExtendedAtlassianLoadState::Ready;
		if (Pending->FirstError.IsSet())
		{
			Pending->Snapshot.Error = Pending->FirstError;
			Pending->Snapshot.bStale = true;
		}
	}
	const UExtendedAtlassianSettings* Settings =
		UExtendedAtlassianSettings::Get();
	ExtendedAtlassianLiveWorkspacePrivate::NormalizeBoardColumns(
		Pending->Snapshot,
		Settings);
	ExtendedAtlassianLiveWorkspacePrivate::BuildLiveSprintPresentation(
		Pending->Snapshot);
	if (FExtendedAtlassianIssueView* SprintView =
		Pending->Snapshot.IssueViews.FindByPredicate(
			[](const FExtendedAtlassianIssueView& View)
			{
				return View.Id == TEXT("sprint");
			}))
	{
		const FExtendedAtlassianSprint* Active =
			Pending->Snapshot.Sprints.FindByPredicate(
				[](const FExtendedAtlassianSprint& Sprint)
				{
					return Sprint.State.Equals(
						TEXT("active"),
						ESearchCase::IgnoreCase);
				});
		if (Active)
		{
			SprintView->Label = Active->Name;
		}
	}
	const TCHAR* EpicColors[] = {
		TEXT("#b6a9ff"), TEXT("#58a6ff"), TEXT("#57cc8a"),
		TEXT("#e3a54a"), TEXT("#f0665f")
	};
	for (FExtendedAtlassianIssue& Issue : Pending->Snapshot.Issues)
	{
		const FString EpicName = !Issue.EpicName.IsEmpty()
			? Issue.EpicName
			: Issue.ParentSummary;
		if (EpicName.IsEmpty())
		{
			continue;
		}
		FExtendedAtlassianEpic* Epic =
			Pending->Snapshot.Epics.FindByPredicate(
				[&EpicName](const FExtendedAtlassianEpic& Candidate)
				{
					return Candidate.Name == EpicName;
				});
		if (!Epic)
		{
			FExtendedAtlassianEpic NewEpic;
			NewEpic.Id = !Issue.ParentId.IsEmpty()
				? Issue.ParentId
				: EpicName;
			NewEpic.Key = Issue.ParentKey;
			NewEpic.Name = EpicName;
			NewEpic.Color = EpicColors[
				Pending->Snapshot.Epics.Num() % UE_ARRAY_COUNT(EpicColors)];
			Pending->Snapshot.Epics.Add(MoveTemp(NewEpic));
			Epic = &Pending->Snapshot.Epics.Last();
		}
		++Epic->TotalIssues;
		Epic->DoneIssues +=
			Issue.StatusCategoryKey == TEXT("done")
				|| Issue.StatusName == TEXT("Done")
					? 1
					: 0;
		Issue.EpicName = EpicName;
		Issue.EpicColor = Epic->Color;
	}
	Pending->Snapshot.Activity.StableSort(
		[](const FExtendedAtlassianActivity& Left,
			const FExtendedAtlassianActivity& Right)
		{
			if (Left.Created == FDateTime::MinValue()
				|| Right.Created == FDateTime::MinValue())
			{
				return false;
			}
			return Left.Created > Right.Created;
		});
	Pending->Snapshot.SyncedAt = FDateTime::UtcNow();
	FExtendedAtlassianInboxState::SynthesizeAndApply(
		Pending->Snapshot,
		InboxUserState);
	LastNotifications = Pending->Snapshot.Notifications;
	FString InboxSaveError;
	FExtendedAtlassianInboxState::Save(
		InboxAccountId,
		InboxUserState,
		InboxSaveError);
	Pending->Completion.ExecuteIfBound(Pending->Request, Pending->Snapshot);
}

void FExtendedAtlassianLiveWorkspaceData::Mutate(
	const FExtendedAtlassianWorkspaceMutation& Mutation,
	FExtendedAtlassianWorkspaceMutationDelegate Completion)
{
	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();
	auto Field = [&Mutation](const TCHAR* Name) -> FString
	{
		return Mutation.Fields.FindRef(Name);
	};
	auto ResolveAccountId = [this](const FString& Value) -> FString
	{
		if (const FExtendedAtlassianUser* User = LastPeople.FindByPredicate(
			[&Value](const FExtendedAtlassianUser& Candidate)
			{
				return Candidate.AccountId.Equals(Value, ESearchCase::CaseSensitive)
					|| Candidate.DisplayName.Equals(Value, ESearchCase::IgnoreCase);
			}))
		{
			return User->AccountId;
		}
		// Mutation payloads use stable account ids. Preserve an id that is not present in
		// the current presentation cache instead of silently converting it to an empty value.
		return Value;
	};
	auto ResolveParentKey = [this](const FString& Value) -> FString
	{
		for (const FExtendedAtlassianIssue& Issue : LastIssues)
		{
			if (Issue.Key.Equals(Value, ESearchCase::IgnoreCase))
			{
				return Issue.Key;
			}
			if (Issue.ParentSummary.Equals(Value, ESearchCase::IgnoreCase)
				&& !Issue.ParentKey.IsEmpty())
			{
				return Issue.ParentKey;
			}
		}
		return FString();
	};
	const auto Finish = [
		this,
		MutationId = Mutation.ClientMutationId,
		Completion
	](bool bSuccess, const FExtendedAtlassianError& Error)
	{
		CompleteMutation(MutationId, Completion, bSuccess, Error);
	};
	auto PinKind = [](const FString& Value)
	{
		if (Value == TEXT("LEVEL"))
		{
			return EExtendedAtlassianPinKind::Level;
		}
		if (Value == TEXT("BLUEPRINT"))
		{
			return EExtendedAtlassianPinKind::Blueprint;
		}
		if (Value == TEXT("PAGE"))
		{
			return EExtendedAtlassianPinKind::Page;
		}
		return EExtendedAtlassianPinKind::Material;
	};
	auto FindPinForMessage = [this](const FString& MessageId)
	{
		for (const FExtendedAtlassianPin& Pin : LastPins)
		{
			if (Pin.Threads.ContainsByPredicate(
				[&MessageId](const FExtendedAtlassianPinThread& Thread)
				{
					return Thread.Id == MessageId;
				}))
			{
				return Pin.Id;
			}
		}
		return FString();
	};

	switch (Mutation.Type)
	{
	case EExtendedAtlassianWorkspaceMutation::MarkNotificationRead:
	case EExtendedAtlassianWorkspaceMutation::MarkAllNotificationsRead:
	case EExtendedAtlassianWorkspaceMutation::DismissNotification:
	case EExtendedAtlassianWorkspaceMutation::ArchiveNotifications:
	case EExtendedAtlassianWorkspaceMutation::MuteNotification:
		{
			FExtendedAtlassianInboxState::ApplyMutation(
				Mutation,
				LastNotifications,
				InboxUserState);
			FString SaveError;
			const bool bSaved = FExtendedAtlassianInboxState::Save(
				InboxAccountId,
				InboxUserState,
				SaveError);
			FExtendedAtlassianError Error;
			if (!bSaved)
			{
				Error.Code = TEXT("InboxStateWriteFailed");
				Error.Message = SaveError;
			}
			CompleteMutation(
				Mutation.ClientMutationId,
				Completion,
				bSaved,
				Error);
		}
		return;

	case EExtendedAtlassianWorkspaceMutation::CreateIssue:
	case EExtendedAtlassianWorkspaceMutation::CreateCaptureIssue:
		{
			FExtendedAtlassianNewIssue Issue;
			Issue.ProjectKey = Settings ? Settings->ProjectKey : FString();
			Issue.IssueTypeId = Field(TEXT("typeId"));
			Issue.IssueTypeName = Field(TEXT("type"));
			if (Issue.IssueTypeId.IsEmpty()
				&& Issue.IssueTypeName.IsEmpty()
				&& Settings)
			{
				Issue.IssueTypeName = Settings->DefaultIssueTypeName;
			}
			Issue.Summary = Field(TEXT("summary")).TrimStartAndEnd();
			if (Issue.Summary.IsEmpty())
			{
				Issue.Summary =
					Mutation.Type == EExtendedAtlassianWorkspaceMutation::CreateCaptureIssue
						? TEXT("Untitled capture from the viewport")
						: TEXT("Untitled card");
			}
			Issue.Description = Field(TEXT("description"));
			Issue.ContextBlock = Field(TEXT("context"));
			Issue.PriorityId = Field(TEXT("priorityId"));
			Issue.PriorityName = Field(TEXT("priority"));
			Issue.AssigneeAccountId = ResolveAccountId(Field(TEXT("assignee")));
			Issue.ParentId = Field(TEXT("epicId"));
			if (Issue.ParentId.IsEmpty())
			{
				Issue.ParentKey = ResolveParentKey(Field(TEXT("epic")));
			}
			FExtendedAtlassianJira::CreateIssue(
				Issue,
				FExtendedAtlassianCreateIssueDelegate::CreateLambda(
					[
						Finish,
						Attachment = Mutation.AttachmentBytes,
						RequestedStatusId = Field(TEXT("statusId")),
						RequestedStatus = Field(TEXT("status")),
						Points = Field(TEXT("points")),
						BoardId = Settings ? Settings->BoardId : FString()
					](
						bool bSuccess,
						const FString& IssueKey,
						const FExtendedAtlassianError& Error)
					{
						if (!bSuccess)
						{
							Finish(false, Error);
							return;
						}
						const auto AfterTransition = [Finish, IssueKey, Attachment](
							bool bTransitionSuccess,
							const FExtendedAtlassianError& TransitionError)
						{
							if (!bTransitionSuccess)
							{
								FExtendedAtlassianError Warning = TransitionError;
								Warning.Code = TEXT("IssueCreatedTransitionFailed");
								Warning.Message = FString::Printf(
									TEXT("%s was created, but its requested status could not be applied: %s"),
									*IssueKey,
									*TransitionError.Message);
								Finish(true, Warning);
								return;
							}
							if (Attachment.IsEmpty())
							{
								Finish(true, FExtendedAtlassianError());
								return;
							}
							FExtendedAtlassianJira::AddAttachment(
								IssueKey,
								TEXT("backlot-capture.png"),
								TEXT("image/png"),
								Attachment,
								FExtendedAtlassianActionDelegate::CreateLambda(
									[Finish, IssueKey](
										bool bAttachmentSuccess,
										const FExtendedAtlassianError& AttachmentError)
									{
										if (bAttachmentSuccess)
										{
											Finish(true, FExtendedAtlassianError());
											return;
										}
										FExtendedAtlassianError Warning = AttachmentError;
										Warning.Code = TEXT("IssueCreatedAttachmentFailed");
										Warning.Message = FString::Printf(
											TEXT("%s was created, but backlot-capture.png failed to upload: %s"),
											*IssueKey,
											*AttachmentError.Message);
										Finish(true, Warning);
									}));
						};
						const auto AfterEstimate =
							[
								Finish,
								IssueKey,
								RequestedStatusId,
								RequestedStatus,
								AfterTransition
							](
								bool bEstimateSuccess,
								const FExtendedAtlassianError& EstimateError)
						{
							if (!bEstimateSuccess)
							{
								FExtendedAtlassianError Warning = EstimateError;
								Warning.Code = TEXT("IssueCreatedEstimateFailed");
								Warning.Message = FString::Printf(
									TEXT("%s was created, but the estimate could not be applied: %s"),
									*IssueKey,
									*EstimateError.Message);
								Finish(true, Warning);
								return;
							}
							if (RequestedStatusId.IsEmpty()
								&& RequestedStatus.IsEmpty())
							{
								AfterTransition(true, FExtendedAtlassianError());
								return;
							}
							FExtendedAtlassianJira::TransitionIssueToStatusIdOrName(
								IssueKey,
								RequestedStatusId,
								RequestedStatus,
								FExtendedAtlassianActionDelegate::CreateLambda(
									AfterTransition));
						};
						if (!Points.IsEmpty() && !BoardId.IsEmpty())
						{
							FExtendedAtlassianJiraSoftware::SetIssueEstimate(
								IssueKey,
								BoardId,
								Points,
								FExtendedAtlassianActionDelegate::CreateLambda(
									AfterEstimate));
						}
						else
						{
							AfterEstimate(true, FExtendedAtlassianError());
						}
					}));
		}
		return;

	case EExtendedAtlassianWorkspaceMutation::UpdateIssue:
		{
			FExtendedAtlassianIssueUpdate Update;
			if (Mutation.Fields.Contains(TEXT("summary")))
			{
				Update.Summary = Field(TEXT("summary"));
			}
			if (Mutation.Fields.Contains(TEXT("description")))
			{
				Update.Description = Field(TEXT("description"));
			}
			if (Mutation.Fields.Contains(TEXT("type")))
			{
				Update.IssueTypeName = Field(TEXT("type"));
			}
			if (Mutation.Fields.Contains(TEXT("typeId")))
			{
				Update.IssueTypeId = Field(TEXT("typeId"));
				Update.IssueTypeName.Reset();
			}
			if (Mutation.Fields.Contains(TEXT("priority")))
			{
				Update.PriorityName = Field(TEXT("priority"));
			}
			if (Mutation.Fields.Contains(TEXT("priorityId")))
			{
				Update.PriorityId = Field(TEXT("priorityId"));
				Update.PriorityName.Reset();
			}
			if (Mutation.Fields.Contains(TEXT("assignee")))
			{
				Update.AssigneeAccountId = ResolveAccountId(Field(TEXT("assignee")));
			}
			if (Mutation.Fields.Contains(TEXT("epic")))
			{
				Update.ParentKey = ResolveParentKey(Field(TEXT("epic")));
			}
			if (Mutation.Fields.Contains(TEXT("epicId")))
			{
				Update.ParentId = Field(TEXT("epicId"));
				Update.ParentKey.Reset();
			}
			const FString RequestedStatusId = Field(TEXT("statusId"));
			const FString RequestedStatus = Field(TEXT("status"));
			const FString Points = Field(TEXT("points"));
			const FString BoardId = Settings ? Settings->BoardId : FString();
			const FString IssueKey = Mutation.TargetId;
			const auto PerformUpdate = [
				Finish,
				IssueKey,
				Update,
				RequestedStatusId,
				RequestedStatus,
				Points,
				BoardId
			]()
			{
				FExtendedAtlassianJira::UpdateIssue(
					IssueKey,
					Update,
					FExtendedAtlassianActionDelegate::CreateLambda(
						[
							Finish,
							IssueKey,
							RequestedStatusId,
							RequestedStatus,
							Points,
							BoardId
						](
							bool bSuccess,
							const FExtendedAtlassianError& Error)
						{
							if (!bSuccess)
							{
								Finish(false, Error);
								return;
							}
							const auto AfterEstimate =
								[
									Finish,
									IssueKey,
									RequestedStatusId,
									RequestedStatus
								](
									bool bEstimateSuccess,
									const FExtendedAtlassianError& EstimateError)
							{
								if (!bEstimateSuccess
									|| (RequestedStatusId.IsEmpty()
										&& RequestedStatus.IsEmpty()))
								{
									Finish(
										bEstimateSuccess,
										EstimateError);
									return;
								}
								FExtendedAtlassianJira::
									TransitionIssueToStatusIdOrName(
										IssueKey,
										RequestedStatusId,
										RequestedStatus,
										FExtendedAtlassianActionDelegate::
											CreateLambda(Finish));
							};
							if (!Points.IsEmpty() && !BoardId.IsEmpty())
							{
								FExtendedAtlassianJiraSoftware::
									SetIssueEstimate(
										IssueKey,
										BoardId,
										Points,
										FExtendedAtlassianActionDelegate::
											CreateLambda(AfterEstimate));
								return;
							}
							AfterEstimate(
								true,
								FExtendedAtlassianError());
						}));
			};
			const FString BaseUpdated = Field(TEXT("baseUpdated"));
			if (BaseUpdated.IsEmpty())
			{
				PerformUpdate();
				return;
			}
			FDateTime ExpectedUpdated = FDateTime::MinValue();
			FDateTime::ParseIso8601(*BaseUpdated, ExpectedUpdated);
			FExtendedAtlassianJira::GetIssue(
				IssueKey,
				FExtendedAtlassianIssueDelegate::CreateLambda(
					[Finish, PerformUpdate, ExpectedUpdated](
						bool bSuccess,
						const FExtendedAtlassianIssue& Current,
						const FExtendedAtlassianError& Error)
					{
						if (!bSuccess)
						{
							Finish(false, Error);
							return;
						}
						if (ExpectedUpdated != FDateTime::MinValue()
							&& Current.Updated != FDateTime::MinValue()
							&& Current.Updated != ExpectedUpdated)
						{
							FExtendedAtlassianError Conflict;
							Conflict.Code = TEXT("StaleIssue");
							Conflict.HttpStatus = 409;
							Conflict.Message =
								TEXT("This issue changed in Jira while you were editing. Your draft was restored; refresh and try again.");
							Finish(false, Conflict);
							return;
						}
						PerformUpdate();
					}));
		}
		return;

	case EExtendedAtlassianWorkspaceMutation::DeleteIssue:
		FExtendedAtlassianJira::DeleteIssue(
			Mutation.TargetId,
			FExtendedAtlassianActionDelegate::CreateLambda(Finish));
		return;

	case EExtendedAtlassianWorkspaceMutation::MoveIssue:
		{
			const FString TargetId = Mutation.TargetId;
			const FString TargetStatus = Field(TEXT("status"));
			const FString PreviousStatus = Field(TEXT("previousStatus"));
			const TArray<FString> OrderedIds = Mutation.OrderedIds;
			const bool bNeedsTransition =
				!TargetStatus.IsEmpty()
				&& !TargetStatus.Equals(
					PreviousStatus,
					ESearchCase::IgnoreCase);
			const auto RankAfterTransition = [
				Finish,
				TargetId,
				TargetStatus,
				OrderedIds
			](bool bTransitioned)
			{
				const int32 Index =
					OrderedIds.IndexOfByKey(TargetId);
				if (Index == INDEX_NONE || OrderedIds.Num() < 2)
				{
					// There is no meaningful Jira rank operation on a one-card board.
					Finish(true, FExtendedAtlassianError());
					return;
				}
				const bool bBefore = Index + 1 < OrderedIds.Num();
				const FString RelativeKey = bBefore
					? OrderedIds[Index + 1]
					: OrderedIds[Index - 1];
				FExtendedAtlassianJiraSoftware::RankIssue(
					TargetId,
					RelativeKey,
					bBefore,
					FExtendedAtlassianActionDelegate::CreateLambda(
						[
							Finish,
							bTransitioned,
							TargetId,
							TargetStatus
						](
							bool bRankSuccess,
							const FExtendedAtlassianError& RankError)
						{
							if (bRankSuccess)
							{
								Finish(true, FExtendedAtlassianError());
								return;
							}
							if (!bTransitioned)
							{
								Finish(false, RankError);
								return;
							}
							FExtendedAtlassianError Warning = RankError;
							Warning.Code = TEXT("PartialRank");
							Warning.Message = FString::Printf(
								TEXT("Moved %s to %s, but Jira rejected the requested rank. The board was refreshed to Jira's retained order."),
								*TargetId,
								*TargetStatus);
							Finish(true, Warning);
						}));
			};
			if (!bNeedsTransition)
			{
				RankAfterTransition(false);
				return;
			}
			FExtendedAtlassianJira::TransitionIssueToStatus(
				TargetId,
				TargetStatus,
				FExtendedAtlassianActionDelegate::CreateLambda(
					[Finish, RankAfterTransition](
						bool bTransitionSuccess,
						const FExtendedAtlassianError& TransitionError)
					{
						if (!bTransitionSuccess)
						{
							Finish(false, TransitionError);
							return;
						}
						RankAfterTransition(true);
					}));
		}
		return;

	case EExtendedAtlassianWorkspaceMutation::RankIssue:
		{
			const int32 Index = Mutation.OrderedIds.IndexOfByKey(Mutation.TargetId);
			if (Index == INDEX_NONE || Mutation.OrderedIds.Num() < 2)
			{
				FExtendedAtlassianError Error;
				Error.Code = TEXT("InvalidRank");
				Error.Message = TEXT("The requested issue order has no adjacent rank target.");
				CompleteMutation(Mutation.ClientMutationId, Completion, false, Error);
				return;
			}
			const bool bBefore = Index + 1 < Mutation.OrderedIds.Num();
			const FString RelativeKey = bBefore
				? Mutation.OrderedIds[Index + 1]
				: Mutation.OrderedIds[Index - 1];
			FExtendedAtlassianJiraSoftware::RankIssue(
				Mutation.TargetId,
				RelativeKey,
				bBefore,
				FExtendedAtlassianActionDelegate::CreateLambda(Finish));
		}
		return;

	case EExtendedAtlassianWorkspaceMutation::TransitionIssue:
		if (!Field(TEXT("transitionId")).IsEmpty())
		{
			FExtendedAtlassianJira::TransitionIssue(
				Mutation.TargetId,
				Field(TEXT("transitionId")),
				FExtendedAtlassianActionDelegate::CreateLambda(Finish));
		}
		else
		{
			FExtendedAtlassianJira::TransitionIssueToStatus(
				Mutation.TargetId,
				Field(TEXT("status")),
				FExtendedAtlassianActionDelegate::CreateLambda(Finish));
		}
		return;

	case EExtendedAtlassianWorkspaceMutation::CreateIssueComment:
		{
			FString IssueKey = Field(TEXT("target"));
			IssueKey.RemoveFromStart(TEXT("issue:"));
			const FString MetadataPageId =
				Settings ? Settings->BacklotMetadataPageId : FString();
			const FString UpdatedBy =
				FUnrealExtendedAtlassianModule::GetClient().IsValid()
					? FUnrealExtendedAtlassianModule::GetClient()
						->GetVerifiedUser().AccountId
					: FString();
			const auto ContinueWithAttachment = [
				Finish,
				IssueKey,
				Attachment = Mutation.AttachmentBytes
			](bool bSuccess, const FExtendedAtlassianError& Error)
			{
				if (!bSuccess || Attachment.IsEmpty())
				{
					Finish(bSuccess, Error);
					return;
				}
				FExtendedAtlassianJira::AddAttachment(
					IssueKey,
					TEXT("backlot-comment-capture.png"),
					TEXT("image/png"),
					Attachment,
					FExtendedAtlassianActionDelegate::CreateLambda(
						[Finish](
							bool bAttachmentSuccess,
							const FExtendedAtlassianError& AttachmentError)
						{
							if (bAttachmentSuccess)
							{
								Finish(true, FExtendedAtlassianError());
								return;
							}
							FExtendedAtlassianError Warning = AttachmentError;
							Warning.Code =
								TEXT("CommentCreatedAttachmentFailed");
							Warning.Message =
								TEXT("The comment was posted, but its viewport capture failed to upload.");
							Finish(true, Warning);
						}));
			};
			FExtendedAtlassianJira::AddCommentWithId(
				IssueKey,
				Field(TEXT("body")),
				FExtendedAtlassianCreateCommentDelegate::CreateLambda(
					[
						ContinueWithAttachment,
						IssueKey,
						ParentId = Mutation.ParentId,
						MetadataPageId,
						UpdatedBy
					](
						bool bSuccess,
						const FString& CommentId,
						const FExtendedAtlassianError& Error)
					{
						if (!bSuccess)
						{
							ContinueWithAttachment(false, Error);
							return;
						}
						if (ParentId.IsEmpty() || MetadataPageId.IsEmpty())
						{
							ContinueWithAttachment(
								true,
								FExtendedAtlassianError());
							return;
						}
						FExtendedAtlassianIssueCommentMetadataStoreMutation
							MetadataMutation;
						MetadataMutation.IssueKey = IssueKey;
						MetadataMutation.CommentId = CommentId;
						MetadataMutation.ParentId = ParentId;
						MetadataMutation.UpdatedBy = UpdatedBy;
						FExtendedAtlassianBacklotStore::
							MutateIssueCommentMetadata(
								MetadataPageId,
								MetadataMutation,
								FExtendedAtlassianIssueCommentMetadataDelegate::
									CreateLambda(
										[ContinueWithAttachment](
											bool bMetadataSuccess,
											const TArray<
												FExtendedAtlassianIssueCommentMetadata>&,
											const FExtendedAtlassianError&
												MetadataError)
										{
											ContinueWithAttachment(
												bMetadataSuccess,
												MetadataError);
										}));
					}));
		}
		return;

	case EExtendedAtlassianWorkspaceMutation::UpdateIssueComment:
		{
			FString IssueKey = Field(TEXT("target"));
			IssueKey.RemoveFromStart(TEXT("issue:"));
			FExtendedAtlassianJira::UpdateComment(
				IssueKey,
				Mutation.TargetId,
				Field(TEXT("body")),
				FExtendedAtlassianActionDelegate::CreateLambda(Finish));
		}
		return;

	case EExtendedAtlassianWorkspaceMutation::DeleteIssueComment:
		{
			FString IssueKey = Field(TEXT("target"));
			IssueKey.RemoveFromStart(TEXT("issue:"));
			const FString MetadataPageId =
				Settings ? Settings->BacklotMetadataPageId : FString();
			const FString UpdatedBy =
				FUnrealExtendedAtlassianModule::GetClient().IsValid()
					? FUnrealExtendedAtlassianModule::GetClient()
						->GetVerifiedUser().AccountId
					: FString();
			FExtendedAtlassianJira::DeleteComment(
				IssueKey,
				Mutation.TargetId,
				FExtendedAtlassianActionDelegate::CreateLambda(
					[
						Finish,
						IssueKey,
						CommentId = Mutation.TargetId,
						MetadataPageId,
						UpdatedBy
					](
						bool bSuccess,
						const FExtendedAtlassianError& Error)
					{
						if (!bSuccess || MetadataPageId.IsEmpty())
						{
							Finish(bSuccess, Error);
							return;
						}
						FExtendedAtlassianIssueCommentMetadataStoreMutation
							MetadataMutation;
						MetadataMutation.Type =
							EExtendedAtlassianIssueCommentMetadataMutation::Remove;
						MetadataMutation.IssueKey = IssueKey;
						MetadataMutation.CommentId = CommentId;
						MetadataMutation.UpdatedBy = UpdatedBy;
						FExtendedAtlassianBacklotStore::
							MutateIssueCommentMetadata(
								MetadataPageId,
								MetadataMutation,
								FExtendedAtlassianIssueCommentMetadataDelegate::
									CreateLambda(
										[Finish](
											bool bMetadataSuccess,
											const TArray<
												FExtendedAtlassianIssueCommentMetadata>&,
											const FExtendedAtlassianError&
												MetadataError)
										{
											Finish(
												bMetadataSuccess,
												MetadataError);
										}));
					}));
		}
		return;

	case EExtendedAtlassianWorkspaceMutation::ResolveIssueComment:
	case EExtendedAtlassianWorkspaceMutation::ReopenIssueComment:
		{
			FString IssueKey = Field(TEXT("target"));
			IssueKey.RemoveFromStart(TEXT("issue:"));
			if (!Settings || Settings->BacklotMetadataPageId.IsEmpty())
			{
				FExtendedAtlassianError Error;
				Error.Code = TEXT("SharedMetadataNotConfigured");
				Error.Message =
					TEXT("Set Backlot Metadata Page ID before resolving Jira comment threads.");
				Finish(false, Error);
				return;
			}
			FExtendedAtlassianIssueCommentMetadataStoreMutation
				MetadataMutation;
			MetadataMutation.IssueKey = IssueKey;
			MetadataMutation.CommentId = Mutation.TargetId;
			if (const FExtendedAtlassianIssueCommentMetadata* Existing =
				LastIssueCommentMetadata.FindByPredicate(
					[&Mutation](
						const FExtendedAtlassianIssueCommentMetadata& Candidate)
					{
						return Candidate.CommentId == Mutation.TargetId;
					}))
			{
				MetadataMutation.ParentId = Existing->ParentId;
			}
			MetadataMutation.bResolved =
				Mutation.Type
				== EExtendedAtlassianWorkspaceMutation::ResolveIssueComment;
			MetadataMutation.UpdatedBy =
				FUnrealExtendedAtlassianModule::GetClient().IsValid()
					? FUnrealExtendedAtlassianModule::GetClient()
						->GetVerifiedUser().AccountId
					: FString();
			const TWeakPtr<FExtendedAtlassianLiveWorkspaceData> WeakProvider =
				AsShared();
			FExtendedAtlassianBacklotStore::MutateIssueCommentMetadata(
				Settings->BacklotMetadataPageId,
				MetadataMutation,
				FExtendedAtlassianIssueCommentMetadataDelegate::CreateLambda(
					[WeakProvider, Finish](
						bool bSuccess,
						const TArray<FExtendedAtlassianIssueCommentMetadata>&
							Metadata,
						const FExtendedAtlassianError& Error)
					{
						if (const TSharedPtr<
							FExtendedAtlassianLiveWorkspaceData> Self =
							WeakProvider.Pin();
							Self.IsValid() && bSuccess)
						{
							Self->LastIssueCommentMetadata = Metadata;
						}
						Finish(bSuccess, Error);
					}));
		}
		return;

	case EExtendedAtlassianWorkspaceMutation::CreatePageComment:
		{
			FString PageId = Field(TEXT("target"));
			PageId.RemoveFromStart(TEXT("page:"));
			FExtendedAtlassianConfluenceComments::CreateFooterComment(
				PageId,
				Mutation.ParentId,
				Field(TEXT("body")),
				FExtendedAtlassianActionDelegate::CreateLambda(Finish));
		}
		return;

	case EExtendedAtlassianWorkspaceMutation::UpdatePageComment:
		if (const FExtendedAtlassianComment* Comment =
			LastPageComments.Find(Mutation.TargetId))
		{
			FExtendedAtlassianConfluenceComments::UpdateComment(
				*Comment,
				Field(TEXT("body")),
				FExtendedAtlassianActionDelegate::CreateLambda(Finish));
		}
		else
		{
			FExtendedAtlassianError Error;
			Error.Code = TEXT("CommentNotLoaded");
			Error.Message = TEXT("Refresh the page comments before editing this comment.");
			CompleteMutation(Mutation.ClientMutationId, Completion, false, Error);
		}
		return;

	case EExtendedAtlassianWorkspaceMutation::DeletePageComment:
		if (const FExtendedAtlassianComment* Comment =
			LastPageComments.Find(Mutation.TargetId))
		{
			FExtendedAtlassianConfluenceComments::DeleteComment(
				*Comment,
				FExtendedAtlassianActionDelegate::CreateLambda(Finish));
		}
		else
		{
			FExtendedAtlassianError Error;
			Error.Code = TEXT("CommentNotLoaded");
			Error.Message = TEXT("Refresh the page comments before deleting this comment.");
			CompleteMutation(Mutation.ClientMutationId, Completion, false, Error);
		}
		return;

	case EExtendedAtlassianWorkspaceMutation::ResolvePageComment:
	case EExtendedAtlassianWorkspaceMutation::ReopenPageComment:
		if (const FExtendedAtlassianComment* Comment =
			LastPageComments.Find(Mutation.TargetId))
		{
			FExtendedAtlassianConfluenceComments::SetInlineResolved(
				*Comment,
				Mutation.Type
					== EExtendedAtlassianWorkspaceMutation::ResolvePageComment,
				FExtendedAtlassianActionDelegate::CreateLambda(Finish));
		}
		else
		{
			FExtendedAtlassianError Error;
			Error.Code = TEXT("CommentNotLoaded");
			Error.Message = TEXT("Refresh the page comments before resolving this comment.");
			CompleteMutation(Mutation.ClientMutationId, Completion, false, Error);
		}
		return;

	case EExtendedAtlassianWorkspaceMutation::CreatePage:
		FExtendedAtlassianConfluence::CreatePage(
			LastSpaceId,
			Mutation.ParentId,
			Field(TEXT("title")).TrimStartAndEnd().IsEmpty()
				? TEXT("Untitled page")
				: Field(TEXT("title")),
			Field(TEXT("body")),
			FExtendedAtlassianPageDelegate::CreateLambda(
				[Finish](
					bool bSuccess,
					const FExtendedAtlassianPage&,
					const FExtendedAtlassianError& Error)
				{
					Finish(bSuccess, Error);
				}));
		return;

	case EExtendedAtlassianWorkspaceMutation::TogglePageTask:
	case EExtendedAtlassianWorkspaceMutation::UpdatePage:
		FExtendedAtlassianConfluence::UpdatePage(
			Mutation.TargetId,
			Field(TEXT("title")),
			Field(TEXT("body")),
			FCString::Atoi(*Field(TEXT("version"))),
			FExtendedAtlassianPageDelegate::CreateLambda(
				[Finish](
					bool bSuccess,
					const FExtendedAtlassianPage&,
					const FExtendedAtlassianError& Error)
				{
					Finish(bSuccess, Error);
				}));
		return;

	case EExtendedAtlassianWorkspaceMutation::DeletePage:
		FExtendedAtlassianConfluence::DeletePage(
			Mutation.TargetId,
			FExtendedAtlassianActionDelegate::CreateLambda(Finish));
		return;

	case EExtendedAtlassianWorkspaceMutation::CreatePin:
	case EExtendedAtlassianWorkspaceMutation::UpdatePin:
	case EExtendedAtlassianWorkspaceMutation::DeletePin:
	case EExtendedAtlassianWorkspaceMutation::CreatePinReply:
	case EExtendedAtlassianWorkspaceMutation::UpdatePinReply:
	case EExtendedAtlassianWorkspaceMutation::DeletePinReply:
	case EExtendedAtlassianWorkspaceMutation::ResolvePinReply:
		{
			if (!Settings || Settings->BacklotMetadataPageId.IsEmpty())
			{
				FExtendedAtlassianError Error;
				Error.Code = TEXT("SharedMetadataNotConfigured");
				Error.Message =
					TEXT("Set Backlot Metadata Page ID before changing shared Pins.");
				CompleteMutation(Mutation.ClientMutationId, Completion, false, Error);
				return;
			}
			FExtendedAtlassianPinStoreMutation PinMutation;
			PinMutation.AuthorAccountId =
				FUnrealExtendedAtlassianModule::GetClient().IsValid()
					? FUnrealExtendedAtlassianModule::GetClient()->GetVerifiedUser().AccountId
					: FString();
			PinMutation.AuthorDisplayName =
				FUnrealExtendedAtlassianModule::GetClient().IsValid()
					? FUnrealExtendedAtlassianModule::GetClient()->GetVerifiedUser().DisplayName
					: FString();
			PinMutation.DisplayName = Field(TEXT("name"));
			PinMutation.Body = Field(TEXT("body"));
			PinMutation.bResolved = Field(TEXT("resolved")).ToBool();
			PinMutation.MessageId =
				Mutation.Type == EExtendedAtlassianWorkspaceMutation::CreatePinReply
					? Mutation.TargetId
					: Mutation.TargetId;
			PinMutation.PinId =
				Mutation.Type == EExtendedAtlassianWorkspaceMutation::CreatePinReply
					? Mutation.ParentId
					: Mutation.TargetId;
			if (Mutation.Type == EExtendedAtlassianWorkspaceMutation::UpdatePinReply
				|| Mutation.Type == EExtendedAtlassianWorkspaceMutation::DeletePinReply
				|| Mutation.Type == EExtendedAtlassianWorkspaceMutation::ResolvePinReply)
			{
				PinMutation.PinId = FindPinForMessage(Mutation.TargetId);
			}
			switch (Mutation.Type)
			{
			case EExtendedAtlassianWorkspaceMutation::CreatePin:
				PinMutation.Type = EExtendedAtlassianPinStoreMutation::CreatePin;
				PinMutation.Target.Kind = PinKind(Field(TEXT("kind")));
				PinMutation.Target.StableId = Field(TEXT("stableId"));
				PinMutation.Target.DisplayName = Field(TEXT("name"));
				PinMutation.Target.SecondaryId = Field(TEXT("secondaryId"));
				PinMutation.Color = Field(TEXT("color"));
				if (PinMutation.PinId.IsEmpty())
				{
					PinMutation.PinId =
						FExtendedAtlassianBacklotStore::MakeStablePinId(
							PinMutation.Target);
				}
				break;
			case EExtendedAtlassianWorkspaceMutation::UpdatePin:
				PinMutation.Type = EExtendedAtlassianPinStoreMutation::UpdatePin;
				break;
			case EExtendedAtlassianWorkspaceMutation::DeletePin:
				PinMutation.Type = EExtendedAtlassianPinStoreMutation::DeletePin;
				break;
			case EExtendedAtlassianWorkspaceMutation::CreatePinReply:
				PinMutation.Type = EExtendedAtlassianPinStoreMutation::CreateMessage;
				break;
			case EExtendedAtlassianWorkspaceMutation::UpdatePinReply:
				PinMutation.Type = EExtendedAtlassianPinStoreMutation::UpdateMessage;
				break;
			case EExtendedAtlassianWorkspaceMutation::DeletePinReply:
				PinMutation.Type = EExtendedAtlassianPinStoreMutation::DeleteMessage;
				break;
			case EExtendedAtlassianWorkspaceMutation::ResolvePinReply:
				PinMutation.Type = EExtendedAtlassianPinStoreMutation::ToggleResolved;
				break;
			default:
				break;
			}
			const TWeakPtr<FExtendedAtlassianLiveWorkspaceData> WeakProvider = AsShared();
			FExtendedAtlassianBacklotStore::MutatePins(
				Settings->BacklotMetadataPageId,
				PinMutation,
				FExtendedAtlassianPinsDelegate::CreateLambda(
					[WeakProvider, Finish](
						bool bSuccess,
						const TArray<FExtendedAtlassianPin>& Pins,
						const FExtendedAtlassianError& Error)
					{
						if (const TSharedPtr<FExtendedAtlassianLiveWorkspaceData> Self =
							WeakProvider.Pin())
						{
							if (bSuccess)
							{
								Self->LastPins = Pins;
							}
						}
						Finish(bSuccess, Error);
					}));
		}
		return;

	default:
		{
			FExtendedAtlassianError Error;
			Error.Code = TEXT("Unsupported");
			Error.Message =
				TEXT("This operation is not available from the connected Atlassian provider yet.");
			CompleteMutation(Mutation.ClientMutationId, Completion, false, Error);
		}
		return;
	}
}

void FExtendedAtlassianLiveWorkspaceData::CompleteMutation(
	uint64 MutationId,
	const FExtendedAtlassianWorkspaceMutationDelegate& Completion,
	bool bSuccess,
	const FExtendedAtlassianError& Error) const
{
	Completion.ExecuteIfBound(MutationId, bSuccess, Error);
}

void FExtendedAtlassianLiveWorkspaceData::CancelGeneration(uint64 Generation)
{
	CancelledGenerations.Add(Generation);
}
