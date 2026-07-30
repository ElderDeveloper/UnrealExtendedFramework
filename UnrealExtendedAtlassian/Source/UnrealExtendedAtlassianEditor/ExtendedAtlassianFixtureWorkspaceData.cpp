// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianFixtureWorkspaceData.h"

#include "ExtendedAtlassianModelUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace ExtendedAtlassianFixturePrivate
{
	FString StringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name)
	{
		FString Value;
		if (Object.IsValid())
		{
			Object->TryGetStringField(Name, Value);
		}
		return Value;
	}

	double NumberField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name, double Default = 0.0)
	{
		double Value = Default;
		if (Object.IsValid())
		{
			Object->TryGetNumberField(Name, Value);
		}
		return Value;
	}

	bool BoolField(const TSharedPtr<FJsonObject>& Object, const TCHAR* Name, bool Default = false)
	{
		bool Value = Default;
		if (Object.IsValid())
		{
			Object->TryGetBoolField(Name, Value);
		}
		return Value;
	}

	EExtendedAtlassianPinKind PinKind(const FString& Kind)
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

	EExtendedAtlassianNotificationKind NotificationKind(const FString& Kind)
	{
		if (Kind == TEXT("REVIEW"))
		{
			return EExtendedAtlassianNotificationKind::Review;
		}
		if (Kind == TEXT("PIN"))
		{
			return EExtendedAtlassianNotificationKind::Pin;
		}
		if (Kind == TEXT("ASSIGN"))
		{
			return EExtendedAtlassianNotificationKind::Assign;
		}
		if (Kind == TEXT("STATUS"))
		{
			return EExtendedAtlassianNotificationKind::Status;
		}
		if (Kind == TEXT("COMMENT"))
		{
			return EExtendedAtlassianNotificationKind::Comment;
		}
		if (Kind == TEXT("LINK"))
		{
			return EExtendedAtlassianNotificationKind::Link;
		}
		return EExtendedAtlassianNotificationKind::Mention;
	}

	FExtendedAtlassianUser UserFromFixture(
		const FString& Initials,
		const TSharedPtr<FJsonObject>& Value)
	{
		FExtendedAtlassianUser User;
		User.AccountId = Initials;
		User.Initials = Initials;
		User.DisplayName = StringField(Value, TEXT("name"));
		User.AvatarBackground = StringField(Value, TEXT("bg"));
		User.AvatarForeground = StringField(Value, TEXT("fg"));
		return User;
	}

	FExtendedAtlassianComment CommentFromFixture(
		const TSharedPtr<FJsonObject>& Object,
		const TMap<FString, FExtendedAtlassianUser>& Users,
		const FString& FallbackId)
	{
		FExtendedAtlassianComment Comment;
		Comment.Id = StringField(Object, TEXT("id"));
		if (Comment.Id.IsEmpty())
		{
			Comment.Id = FallbackId;
		}
		Comment.AuthorAccountId = StringField(Object, TEXT("who"));
		Comment.AuthorDisplayName = Users.FindRef(Comment.AuthorAccountId).DisplayName;
		Comment.Body = StringField(Object, TEXT("body"));
		Comment.Quote = StringField(Object, TEXT("quote"));
		Comment.AccentColor = StringField(Object, TEXT("mark"));
		Comment.RelativeTime = StringField(Object, TEXT("when"));
		Comment.bResolved = BoolField(Object, TEXT("resolved"));
		Comment.bCanEdit = true;
		Comment.bCanDelete = true;

		const TArray<TSharedPtr<FJsonValue>>* Replies = nullptr;
		if (Object->TryGetArrayField(TEXT("replies"), Replies))
		{
			for (int32 ReplyIndex = 0; ReplyIndex < Replies->Num(); ++ReplyIndex)
			{
				Comment.Replies.Add(CommentFromFixture(
					(*Replies)[ReplyIndex]->AsObject(),
					Users,
					FString::Printf(TEXT("%s:reply:%d"), *Comment.Id, ReplyIndex)));
			}
		}
		return Comment;
	}
}

FExtendedAtlassianFixtureWorkspaceData::FExtendedAtlassianFixtureWorkspaceData()
{
	bValid = LoadFixture();
	if (!bValid)
	{
		Snapshot.State = EExtendedAtlassianLoadState::Error;
		Snapshot.Error.Code = TEXT("FixtureLoad");
		Snapshot.Error.Message = LoadError;
	}
}

void FExtendedAtlassianFixtureWorkspaceData::Load(
	const FExtendedAtlassianWorkspaceRequest& Request,
	FExtendedAtlassianWorkspaceLoadDelegate Completion)
{
	if (!Completion.IsBound() || CancelledGenerations.Contains(Request.Generation))
	{
		return;
	}

	Completion.Execute(Request, Snapshot);
}

void FExtendedAtlassianFixtureWorkspaceData::Mutate(
	const FExtendedAtlassianWorkspaceMutation& Mutation,
	FExtendedAtlassianWorkspaceMutationDelegate Completion)
{
	if (!bValid)
	{
		if (Completion.IsBound())
		{
			Completion.Execute(Mutation.ClientMutationId, false, Snapshot.Error);
		}
		return;
	}

	ApplyMutation(Mutation);
	if (Completion.IsBound())
	{
		Completion.Execute(Mutation.ClientMutationId, true, FExtendedAtlassianError());
	}
}

void FExtendedAtlassianFixtureWorkspaceData::CancelGeneration(uint64 Generation)
{
	CancelledGenerations.Add(Generation);
}

const FExtendedAtlassianCapabilities& FExtendedAtlassianFixtureWorkspaceData::GetCapabilities() const
{
	return Snapshot.Capabilities;
}

bool FExtendedAtlassianFixtureWorkspaceData::LoadFixture()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UnrealExtendedAtlassian"));
	if (!Plugin.IsValid())
	{
		LoadError = TEXT("Could not find the UnrealExtendedAtlassian plugin.");
		return false;
	}

	const FString Path = FPaths::Combine(
		Plugin->GetBaseDir(),
		TEXT("Tests"),
		TEXT("Parity"),
		TEXT("BacklotFixture.json"));

	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		LoadError = FString::Printf(TEXT("Could not read %s"), *Path);
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		LoadError = FString::Printf(TEXT("Could not parse %s"), *Path);
		return false;
	}

	ParseFixture(Root.ToSharedRef());
	return true;
}

void FExtendedAtlassianFixtureWorkspaceData::ParseFixture(const TSharedRef<FJsonObject>& Root)
{
	using namespace ExtendedAtlassianFixturePrivate;

	Snapshot = FExtendedAtlassianWorkspaceSnapshot();
	Snapshot.State = EExtendedAtlassianLoadState::Ready;
	Snapshot.ProjectFileLabel = TEXT("NIGHTFALL_BAY.UPROJECT");
	Snapshot.PluginVersionLabel = TEXT("PLUGIN v0.9.2");
	Snapshot.PlatformLabel = TEXT("SM6 · WIN64");
	Snapshot.ConfluenceSpaceId = TEXT("fixture-production-bible");
	Snapshot.ConfluenceSpaceKey = TEXT("PB");
	Snapshot.ConfluenceSpaceName = TEXT("Production Bible");
	Snapshot.Capabilities.bCanReadIssues = true;
	Snapshot.Capabilities.bCanCreateIssues = true;
	Snapshot.Capabilities.bCanEditIssues = true;
	Snapshot.Capabilities.bCanDeleteIssues = true;
	Snapshot.Capabilities.bCanAssignIssues = true;
	Snapshot.Capabilities.bCanTransitionIssues = true;
	Snapshot.Capabilities.bCanRankIssues = true;
	Snapshot.Capabilities.bCanReadBoards = true;
	Snapshot.Capabilities.bCanReadPages = true;
	Snapshot.Capabilities.bCanEditPages = true;
	Snapshot.Capabilities.bCanDeletePages = true;
	Snapshot.Capabilities.bCanComment = true;
	Snapshot.Capabilities.bCanUseSharedMetadata = true;

	const TSharedPtr<FJsonObject> Constants = Root->GetObjectField(TEXT("constants"));
	const TSharedPtr<FJsonObject> State = Root->GetObjectField(TEXT("initialState"));
	NextIssueNumber = static_cast<int32>(NumberField(State, TEXT("nextKey"), 1065.0));
	NextPageNumber = static_cast<int32>(NumberField(State, TEXT("newPageN"), 0.0)) + 1;
	NextCommentNumber = static_cast<int32>(NumberField(State, TEXT("nextCid"), 20.0));
	const TSharedPtr<FJsonObject> People = Constants->GetObjectField(TEXT("people"));
	TMap<FString, FExtendedAtlassianUser> Users;
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : People->Values)
	{
		const FExtendedAtlassianUser User = UserFromFixture(Pair.Key, Pair.Value->AsObject());
		Users.Add(Pair.Key, User);
	}
	Snapshot.CurrentUser = Users.FindRef(TEXT("AK"));
	for (const TCHAR* Initials : {
		TEXT("AK"), TEXT("MR"), TEXT("JT"), TEXT("SO"), TEXT("LN")
	})
	{
		Snapshot.People.Add(Users.FindRef(Initials));
	}

	const TSharedPtr<FJsonObject> Types = Constants->GetObjectField(TEXT("types"));
	for (const TCHAR* TypeName : { TEXT("Bug"), TEXT("Task"), TEXT("Doc") })
	{
		if (Types->HasField(TypeName))
		{
			FExtendedAtlassianIssueType Type;
			Type.Id = TypeName;
			Type.Name = TypeName;
			Snapshot.IssueTypes.Add(MoveTemp(Type));
		}
	}

	const TArray<TSharedPtr<FJsonValue>>& Priorities =
		Constants->GetArrayField(TEXT("priorities"));
	for (const TSharedPtr<FJsonValue>& PriorityValue : Priorities)
	{
		FExtendedAtlassianPriority Priority;
		Priority.Id = PriorityValue->AsString();
		Priority.Name = Priority.Id;
		Snapshot.Priorities.Add(MoveTemp(Priority));
	}

	const TArray<TSharedPtr<FJsonValue>>& Views = Constants->GetArrayField(TEXT("views"));
	const int32 ViewCounts[] = { 12, 3, 4, 1, 1 };
	const TCHAR* ViewIds[] = {
		TEXT("sprint"), TEXT("mine"), TEXT("triage"), TEXT("blocked"), TEXT("docs")
	};
	const TCHAR* ViewJql[] = {
		TEXT("sprint in openSprints()"),
		TEXT("assignee = currentUser() AND resolution = Unresolved"),
		TEXT("status = Triage"),
		TEXT("status = Blocked"),
		TEXT("issuetype = Doc AND resolution = Unresolved")
	};
	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
	{
		const TSharedPtr<FJsonObject> Object = Views[ViewIndex]->AsObject();
		FExtendedAtlassianIssueView View;
		const int32 ContractIndex = FMath::Min(
			ViewIndex,
			static_cast<int32>(UE_ARRAY_COUNT(ViewIds)) - 1);
		View.Id = ViewIds[ContractIndex];
		View.Label = StringField(Object, TEXT("label"));
		View.DotColor = StringField(Object, TEXT("dot"));
		View.Jql = ViewJql[ContractIndex];
		View.AuthoredCount = ViewCounts[ContractIndex];
		Snapshot.IssueViews.Add(MoveTemp(View));
	}

	const TSharedPtr<FJsonObject> EpicColors = Constants->GetObjectField(TEXT("epics"));
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : EpicColors->Values)
	{
		FExtendedAtlassianEpic Epic;
		Epic.Id = Pair.Key;
		Epic.Name = Pair.Key;
		Epic.Color = Pair.Value->AsString();
		Snapshot.Epics.Add(MoveTemp(Epic));
	}

	const TArray<TSharedPtr<FJsonValue>>& Issues = State->GetArrayField(TEXT("issues"));
	for (const TSharedPtr<FJsonValue>& Value : Issues)
	{
		const TSharedPtr<FJsonObject> Object = Value->AsObject();
		FExtendedAtlassianIssue Issue;
		Issue.Id = StringField(Object, TEXT("key"));
		Issue.Key = Issue.Id;
		Issue.Summary = StringField(Object, TEXT("summary"));
		Issue.IssueTypeName = StringField(Object, TEXT("type"));
		Issue.IssueTypeId = Issue.IssueTypeName;
		Issue.StatusName = StringField(Object, TEXT("status"));
		Issue.StatusId = Issue.StatusName;
		Issue.StatusCategoryKey =
			Issue.StatusName == TEXT("Done")
				? TEXT("done")
				: (Issue.StatusName == TEXT("Triage") ? TEXT("new") : TEXT("indeterminate"));
		Issue.PriorityName = StringField(Object, TEXT("prio"));
		Issue.PriorityId = Issue.PriorityName;
		Issue.AssigneeAccountId = StringField(Object, TEXT("who"));
		Issue.AssigneeDisplayName = Users.FindRef(Issue.AssigneeAccountId).DisplayName;
		Issue.AssigneeAvatarUrl = Users.FindRef(Issue.AssigneeAccountId).AvatarUrl;
		Issue.EpicName = StringField(Object, TEXT("epic"));
		Issue.EpicId = Issue.EpicName;
		Issue.Estimate = NumberField(Object, TEXT("pts"));
		Issue.RelativeUpdated = StringField(Object, TEXT("updated"));
		Issue.CommentCount = static_cast<int32>(NumberField(Object, TEXT("threads")));
		Issue.EditableFields = {
			TEXT("summary"), TEXT("description"), TEXT("status"), TEXT("assignee"),
			TEXT("parent"), TEXT("priority"), TEXT("storyPoints")
		};
		if (Issue.Key == TEXT("NFB-1038"))
		{
			Issue.RelativeBlocked = TEXT("2d");
		}
		if (Issue.Key == TEXT("NFB-1042"))
		{
			Issue.Description = TEXT(
				"Crossing the harbour trigger from the east causes the fog volume to pop one frame "
				"late, so the skybox reads flat for about 200 ms. Repro is reliable when sprinting; "
				"walking hides it.\n\nLikely the volume blend is keyed to the streaming callback rather "
				"than the camera. Related to the wetness work in Wet Surfaces & Rain.");
			Issue.ReporterDisplayName = Users.FindRef(TEXT("JT")).DisplayName;
			Issue.Labels = { TEXT("fog"), TEXT("streaming") };
		}
		Snapshot.Issues.Add(MoveTemp(Issue));
	}

	for (FExtendedAtlassianEpic& Epic : Snapshot.Epics)
	{
		for (FExtendedAtlassianIssue& Issue : Snapshot.Issues)
		{
			if (Issue.EpicName == Epic.Name)
			{
				++Epic.TotalIssues;
				Epic.DoneIssues += Issue.StatusName == TEXT("Done") ? 1 : 0;
				Issue.EpicColor = Epic.Color;
			}
		}
	}

	TMap<FString, FString> PageTitles;
	const TArray<TSharedPtr<FJsonValue>>& Tree = State->GetArrayField(TEXT("docTree"));
	FString CurrentSectionId;
	for (const TSharedPtr<FJsonValue>& Value : Tree)
	{
		const TSharedPtr<FJsonObject> Object = Value->AsObject();
		FExtendedAtlassianDocumentTreeNode Node;
		Node.Id = StringField(Object, TEXT("id"));
		Node.Label = StringField(Object, TEXT("label"));
		Node.Depth = static_cast<int32>(NumberField(Object, TEXT("d")));
		Node.CommentBadge = FCString::Atoi(*StringField(Object, TEXT("badge")));
		Node.bSection = BoolField(Object, TEXT("section"));
		Node.bExpanded = BoolField(Object, TEXT("open"));
		if (Node.bSection)
		{
			CurrentSectionId = Node.Id;
		}
		else
		{
			Node.ParentId = Node.Depth > 0 ? CurrentSectionId : FString();
			PageTitles.Add(Node.Id, Node.Label);
		}
		Snapshot.DocumentTree.Add(MoveTemp(Node));
	}

	const TSharedPtr<FJsonObject> Pages = State->GetObjectField(TEXT("pages"));
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Pages->Values)
	{
		const TSharedPtr<FJsonObject> Object = Pair.Value->AsObject();
		FExtendedAtlassianPage Page;
		Page.Id = Pair.Key;
		Page.Title = PageTitles.FindRef(Pair.Key);
		Page.OwnerAccountId = StringField(Object, TEXT("owner"));
		Page.Version = static_cast<int32>(NumberField(Object, TEXT("version")));
		Page.EditedByLabel = StringField(Object, TEXT("editedBy"));
		Page.EditedAtLabel = StringField(Object, TEXT("editedAt"));
		Page.ReviewState = Pair.Key == TEXT("weekly") ? TEXT("DRAFT") : TEXT("LIVE DOC");
		Page.MilestoneText = TEXT("Reviewed each milestone");
		if (Pair.Key == TEXT("wet"))
		{
			Page.LinkedIssueKeys = {
				TEXT("NFB-1038"),
				TEXT("NFB-1061"),
				TEXT("NFB-1042")
			};
		}
		Page.Contributors = {
			Users.FindRef(TEXT("MR")),
			Users.FindRef(TEXT("AK")),
			Users.FindRef(TEXT("JT"))
		};
		const TArray<TSharedPtr<FJsonValue>>* Blocks = nullptr;
		if (Object->TryGetArrayField(TEXT("blocks"), Blocks))
		{
			Page.CommentCount = 0;
			for (const TSharedPtr<FJsonValue>& BlockValue : *Blocks)
			{
				const TSharedPtr<FJsonObject> BlockObject = BlockValue->AsObject();
				const FString Kind = StringField(BlockObject, TEXT("kind"));
				const FString TextValue = StringField(BlockObject, TEXT("text"));

				if (Kind == TEXT("h2") || Kind == TEXT("para") || Kind == TEXT("callout"))
				{
					FExtendedAtlassianDocBlock Block;
					Block.Kind =
						Kind == TEXT("h2")
							? EExtendedAtlassianBlockKind::Heading
							: (Kind == TEXT("callout")
								? EExtendedAtlassianBlockKind::Quote
								: EExtendedAtlassianBlockKind::Paragraph);
					Block.Level = Kind == TEXT("h2") ? 2 : 0;
					Block.Markup = FExtendedAtlassianMarkup::Escape(TextValue);
					Page.Blocks.Add(MoveTemp(Block));
					Page.Body += TextValue + TEXT("\n");
					continue;
				}

				if (Kind == TEXT("code"))
				{
					FExtendedAtlassianDocBlock Block;
					Block.Kind = EExtendedAtlassianBlockKind::CodeBlock;
					Block.CodeLanguage = StringField(BlockObject, TEXT("lang"));
					Block.ImageAlt = StringField(BlockObject, TEXT("file"));
					Block.RawText = TextValue;
					Page.Blocks.Add(MoveTemp(Block));
					Page.Body += TextValue + TEXT("\n");
					continue;
				}

				const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
				if (!BlockObject->TryGetArrayField(TEXT("items"), Items))
				{
					continue;
				}

				if (Kind == TEXT("rules"))
				{
					for (int32 ItemIndex = 0; ItemIndex < Items->Num(); ++ItemIndex)
					{
						const FString ItemText = (*Items)[ItemIndex]->AsString();
						FExtendedAtlassianDocBlock Block;
						Block.Kind = EExtendedAtlassianBlockKind::OrderedItem;
						Block.OrderedIndex = ItemIndex + 1;
						Block.Markup = FExtendedAtlassianMarkup::Escape(ItemText);
						Page.Blocks.Add(MoveTemp(Block));
						Page.Body += ItemText + TEXT("\n");
					}
				}
				else if (Kind == TEXT("todo"))
				{
					for (const TSharedPtr<FJsonValue>& ItemValue : *Items)
					{
						const TSharedPtr<FJsonObject> Item = ItemValue->AsObject();
						const FString ItemText = StringField(Item, TEXT("text"));
						FExtendedAtlassianDocBlock Block;
						Block.Kind = EExtendedAtlassianBlockKind::TaskItem;
						Block.Markup = FExtendedAtlassianMarkup::Escape(ItemText);
						Block.bChecked = BoolField(Item, TEXT("done"));
						Page.Blocks.Add(MoveTemp(Block));
						Page.Body += ItemText + TEXT("\n");
					}
				}
				else if (Kind == TEXT("table"))
				{
					FExtendedAtlassianDocBlock Header;
					Header.Kind = EExtendedAtlassianBlockKind::TableRow;
					Header.bIsHeaderRow = true;
					Header.Cells = {
						TEXT("PARAMETER"), TEXT("RANGE"), TEXT("DEFAULT"), TEXT("OWNER")
					};
					Page.Blocks.Add(MoveTemp(Header));
					for (const TSharedPtr<FJsonValue>& ItemValue : *Items)
					{
						const TSharedPtr<FJsonObject> Item = ItemValue->AsObject();
						FExtendedAtlassianDocBlock Row;
						Row.Kind = EExtendedAtlassianBlockKind::TableRow;
						Row.Cells = {
							FExtendedAtlassianMarkup::Escape(StringField(Item, TEXT("name"))),
							FExtendedAtlassianMarkup::Escape(StringField(Item, TEXT("range"))),
							FExtendedAtlassianMarkup::Escape(StringField(Item, TEXT("def"))),
							FExtendedAtlassianMarkup::Escape(StringField(Item, TEXT("owner")))
						};
						Page.Body += StringField(Item, TEXT("name")) + TEXT("\n");
						Page.Blocks.Add(MoveTemp(Row));
					}
				}
				else if (Kind == TEXT("embeds"))
				{
					for (const TSharedPtr<FJsonValue>& ItemValue : *Items)
					{
						const TSharedPtr<FJsonObject> Item = ItemValue->AsObject();
						const FString ItemName = StringField(Item, TEXT("name"));
						FExtendedAtlassianDocBlock Block;
						Block.Kind = EExtendedAtlassianBlockKind::Image;
						Block.ImageAlt = ItemName;
						Block.ImageMeta = StringField(Item, TEXT("meta"));
						Block.EmbedSlot = StringField(Item, TEXT("slot"));
						Page.Blocks.Add(MoveTemp(Block));
						Page.Body += ItemName + TEXT("\n");
					}
				}
			}
		}
		Snapshot.Pages.Add(MoveTemp(Page));
	}

	const TSharedPtr<FJsonObject> CommentSets = State->GetObjectField(TEXT("comments"));
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : CommentSets->Values)
	{
		FExtendedAtlassianCommentCollection Collection;
		Collection.TargetId = Pair.Key;
		const TArray<TSharedPtr<FJsonValue>>& Comments = Pair.Value->AsArray();
		for (int32 CommentIndex = 0; CommentIndex < Comments.Num(); ++CommentIndex)
		{
			Collection.Comments.Add(CommentFromFixture(
				Comments[CommentIndex]->AsObject(),
				Users,
				FString::Printf(TEXT("%s:comment:%d"), *Pair.Key, CommentIndex)));
		}
		Snapshot.CommentCollections.Add(MoveTemp(Collection));
	}
	for (FExtendedAtlassianPage& Page : Snapshot.Pages)
	{
		if (const FExtendedAtlassianCommentCollection* Collection =
			Snapshot.CommentCollections.FindByPredicate(
				[&Page](const FExtendedAtlassianCommentCollection& Candidate)
				{
					return Candidate.TargetId == TEXT("page:") + Page.Id;
				}))
		{
			Page.CommentCount = Collection->Comments.Num();
		}
	}

	const TSharedPtr<FJsonObject> ActivityLog = State->GetObjectField(TEXT("activityLog"));
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : ActivityLog->Values)
	{
		const TArray<TSharedPtr<FJsonValue>>& Rows = Pair.Value->AsArray();
		for (int32 RowIndex = 0; RowIndex < Rows.Num(); ++RowIndex)
		{
			const TSharedPtr<FJsonObject> Row = Rows[RowIndex]->AsObject();
			FExtendedAtlassianActivity Activity;
			Activity.Id = FString::Printf(TEXT("%s:activity:%d"), *Pair.Key, RowIndex);
			Activity.IssueKey = Pair.Key;
			Activity.ActorAccountId = StringField(Row, TEXT("who"));
			Activity.ActorDisplayName = Users.FindRef(Activity.ActorAccountId).DisplayName;
			Activity.Detail = StringField(Row, TEXT("text"));
			Activity.Verb = TEXT("fixture");
			Activity.RelativeTime = StringField(Row, TEXT("when"));
			Snapshot.Activity.Add(MoveTemp(Activity));
		}
	}

	const struct
	{
		const TCHAR* Author;
		const TCHAR* Time;
		const TCHAR* Label;
		const TCHAR* Body;
		bool bResolved;
	} FixtureThreads[] = {
		{
			TEXT("AK"),
			TEXT("12m"),
			TEXT("FOG VOLUME"),
			TEXT("Worst on the east approach — the horizon goes flat for about six frames."),
			false
		},
		{
			TEXT("MR"),
			TEXT("8m"),
			TEXT("STREAMING"),
			TEXT("Blend is keyed to the streaming callback, not the camera. Reordering it fixes the pop but costs 0.4 ms."),
			false
		},
		{
			TEXT("JT"),
			TEXT("4m"),
			TEXT("RESOLVED"),
			TEXT("Wet planks are unrelated to this one — ignore that part of the repro."),
			true
		}
	};
	for (int32 ThreadIndex = 0; ThreadIndex < UE_ARRAY_COUNT(FixtureThreads); ++ThreadIndex)
	{
		FExtendedAtlassianIssueThread Thread;
		Thread.Id = FString::Printf(TEXT("NFB-1042:thread:%d"), ThreadIndex);
		Thread.IssueKey = TEXT("NFB-1042");
		Thread.AuthorAccountId = FixtureThreads[ThreadIndex].Author;
		Thread.AuthorDisplayName = Users.FindRef(Thread.AuthorAccountId).DisplayName;
		Thread.RelativeTime = FixtureThreads[ThreadIndex].Time;
		Thread.Label = FixtureThreads[ThreadIndex].Label;
		Thread.Body = FixtureThreads[ThreadIndex].Body;
		Thread.AccentColor = ThreadIndex == 0
			? TEXT("#58a6ff")
			: (ThreadIndex == 1 ? TEXT("#e3a54a") : TEXT("#b6a9ff"));
		Thread.bResolved = FixtureThreads[ThreadIndex].bResolved;
		Snapshot.IssueThreads.Add(MoveTemp(Thread));
	}

	FExtendedAtlassianBoard Board;
	Board.Id = TEXT("fixture-board");
	Board.Name = TEXT("Nightfall Bay");
	Board.Type = TEXT("scrum");
	Board.ProjectKey = TEXT("NFB");
	Snapshot.Boards.Add(MoveTemp(Board));

	FExtendedAtlassianSprint Sprint;
	Sprint.Id = TEXT("24");
	Sprint.Name = TEXT("Sprint 24");
	Sprint.State = TEXT("active");
	Sprint.Goal = TEXT("HARBOUR VERTICAL SLICE");
	Snapshot.Sprints.Add(MoveTemp(Sprint));
	Snapshot.SelectedSprintId = TEXT("24");
	Snapshot.SprintSummary.DaysLeft = TEXT("4d LEFT");
	Snapshot.SprintSummary.DateRange = TEXT("14 JUL – 01 AUG");
	Snapshot.SprintSummary.Goal = TEXT("HARBOUR VERTICAL SLICE");
	Snapshot.SprintSummary.Done = 28;
	Snapshot.SprintSummary.Wip = 15;
	Snapshot.SprintSummary.Left = 21;
	Snapshot.SprintSummary.DoneFraction = 0.44;
	Snapshot.SprintSummary.WipFraction = 0.23;
	Snapshot.SprintSummary.BlockedFraction = 0.11;

	const double TeamPoints[] = { 10.0, 24.0, 4.0, 2.0, 7.0 };
	for (int32 PersonIndex = 0; PersonIndex < Snapshot.People.Num(); ++PersonIndex)
	{
		FExtendedAtlassianTeamLoad Load;
		Load.User = Snapshot.People[PersonIndex];
		const int32 ContractIndex = FMath::Min(
			PersonIndex,
			static_cast<int32>(UE_ARRAY_COUNT(TeamPoints)) - 1);
		Load.OpenPoints = TeamPoints[ContractIndex];
		Load.Fraction = Load.OpenPoints / 24.0;
		Load.ThresholdColor =
			Load.Fraction >= 0.95
				? TEXT("#f0665f")
				: (Load.Fraction >= 0.70 ? TEXT("#e3a54a") : TEXT("#57cc8a"));
		Snapshot.TeamLoad.Add(MoveTemp(Load));
	}

	auto AddColumn = [this](
		const TCHAR* Name,
		const TCHAR* PrimaryStatus,
		const TCHAR* SecondaryStatus,
		int32 WipLimit)
	{
		FExtendedAtlassianBoardColumn Column;
		Column.Id = Name;
		Column.DisplayName = Name;
		Column.StatusNames.Add(PrimaryStatus);
		Column.StatusIds.Add(PrimaryStatus);
		if (SecondaryStatus && SecondaryStatus[0] != TCHAR('\0'))
		{
			Column.StatusNames.Add(SecondaryStatus);
			Column.StatusIds.Add(SecondaryStatus);
		}
		Column.WipLimit = WipLimit;
		Snapshot.BoardColumns.Add(MoveTemp(Column));
	};
	AddColumn(TEXT("Triage"), TEXT("Triage"), TEXT("Blocked"), 0);
	AddColumn(TEXT("In progress"), TEXT("In progress"), TEXT(""), 3);
	AddColumn(TEXT("In review"), TEXT("In review"), TEXT(""), 0);
	AddColumn(TEXT("Done"), TEXT("Done"), TEXT(""), 0);

	const TArray<TSharedPtr<FJsonValue>>& PinCards = State->GetArrayField(TEXT("pinCards"));
	for (int32 PinIndex = 0; PinIndex < PinCards.Num(); ++PinIndex)
	{
		const TSharedPtr<FJsonObject> Object = PinCards[PinIndex]->AsObject();
		FExtendedAtlassianPin Pin;
		Pin.Id = StringField(Object, TEXT("name"));
		Pin.DisplayName = Pin.Id;
		Pin.Target.Kind = PinKind(StringField(Object, TEXT("kind")));
		Pin.Target.StableId = Pin.Id;
		Pin.Target.DisplayName = Pin.Id;
		Pin.Color = StringField(Object, TEXT("kindColor"));
		Pin.Version = 1;

		const TArray<TSharedPtr<FJsonValue>>& Threads = Object->GetArrayField(TEXT("threads"));
		for (int32 ThreadIndex = 0; ThreadIndex < Threads.Num(); ++ThreadIndex)
		{
			const TSharedPtr<FJsonObject> ThreadObject = Threads[ThreadIndex]->AsObject();
			FExtendedAtlassianPinThread Thread;
			Thread.Id = FString::Printf(TEXT("%s:%d"), *Pin.Id, ThreadIndex);
			Thread.AuthorAccountId = StringField(ThreadObject, TEXT("who"));
			Thread.AuthorDisplayName = Users.FindRef(Thread.AuthorAccountId).DisplayName;
			Thread.Body = StringField(ThreadObject, TEXT("body"));
			Thread.RelativeTime = StringField(ThreadObject, TEXT("when"));
			Thread.LinkedLabel = StringField(ThreadObject, TEXT("tag"));
			Thread.bResolved = BoolField(ThreadObject, TEXT("resolved"));
			Pin.Threads.Add(MoveTemp(Thread));
		}
		Snapshot.Pins.Add(MoveTemp(Pin));
	}

	const TArray<TSharedPtr<FJsonValue>>& Inbox = State->GetArrayField(TEXT("inbox"));
	for (int32 Index = 0; Index < Inbox.Num(); ++Index)
	{
		const TSharedPtr<FJsonObject> Object = Inbox[Index]->AsObject();
		FExtendedAtlassianNotification Notification;
		Notification.Id = FString::Printf(TEXT("fixture-inbox-%d"), Index);
		Notification.ActorAccountId = StringField(Object, TEXT("who"));
		Notification.ActorDisplayName = Users.FindRef(Notification.ActorAccountId).DisplayName;
		Notification.Action = StringField(Object, TEXT("action"));
		Notification.Target = StringField(Object, TEXT("target"));
		Notification.Quote = StringField(Object, TEXT("quote"));
		Notification.RelativeTime = StringField(Object, TEXT("when"));
		Notification.Kind = NotificationKind(StringField(Object, TEXT("kind")));
		Notification.Source = StringField(Object, TEXT("source"));
		Notification.SourceId = StringField(Object, TEXT("to"));
		Notification.bRead = !BoolField(Object, TEXT("unread"));
		Snapshot.Notifications.Add(MoveTemp(Notification));
	}
	ExtendedAtlassianModelUtils::RefreshCommentPresentation(Snapshot);
	for (FExtendedAtlassianIssueView& View : Snapshot.IssueViews)
	{
		for (const FExtendedAtlassianIssue& Issue : Snapshot.Issues)
		{
			const bool bMatches =
				View.Id == TEXT("sprint")
				|| (View.Id == TEXT("mine")
					&& Issue.AssigneeAccountId == Snapshot.CurrentUser.AccountId)
				|| (View.Id == TEXT("triage")
					&& Issue.StatusName == TEXT("Triage"))
				|| (View.Id == TEXT("blocked")
					&& Issue.StatusName == TEXT("Blocked"))
				|| (View.Id == TEXT("docs")
					&& Issue.IssueTypeName == TEXT("Doc"));
			if (bMatches)
			{
				View.IssueKeys.Add(Issue.Key);
			}
		}
	}
	Snapshot.SyncedAt = FDateTime::UtcNow() - FTimespan::FromSeconds(12.0);
}

void FExtendedAtlassianFixtureWorkspaceData::ApplyMutation(
	const FExtendedAtlassianWorkspaceMutation& Mutation)
{
	if (Mutation.Type == EExtendedAtlassianWorkspaceMutation::MoveIssue)
	{
		FExtendedAtlassianWorkspaceMutation Transition = Mutation;
		Transition.Type =
			EExtendedAtlassianWorkspaceMutation::TransitionIssue;
		ApplyMutation(Transition);
		FExtendedAtlassianWorkspaceMutation Rank = Mutation;
		Rank.Type = EExtendedAtlassianWorkspaceMutation::RankIssue;
		ApplyMutation(Rank);
		return;
	}

	if (ExtendedAtlassianModelUtils::ApplyDocumentMutation(Snapshot, Mutation))
	{
		if (Mutation.Type == EExtendedAtlassianWorkspaceMutation::UpdatePage)
		{
			if (FExtendedAtlassianPage* Page = Snapshot.Pages.FindByPredicate(
				[&Mutation](const FExtendedAtlassianPage& Candidate)
				{
					return Candidate.Id == Mutation.TargetId;
				}))
			{
				Page->EditedByLabel = TEXT("A. KWAN");
				Page->EditedAtLabel = TEXT("JUST NOW");
			}
		}
		return;
	}

	auto Field = [&Mutation](const TCHAR* Name) -> FString
	{
		if (const FString* Value = Mutation.Fields.Find(Name))
		{
			return *Value;
		}
		return FString();
	};

	switch (Mutation.Type)
	{
	case EExtendedAtlassianWorkspaceMutation::CreateIssue:
	case EExtendedAtlassianWorkspaceMutation::CreateCaptureIssue:
		{
			FExtendedAtlassianIssue Issue;
			Issue.Key = Mutation.TargetId.IsEmpty()
				? FString::Printf(TEXT("NFB-%d"), NextIssueNumber++)
				: Mutation.TargetId;
			Issue.Id = Issue.Key;
			Issue.Summary = Field(TEXT("summary")).TrimStartAndEnd();
			if (Issue.Summary.IsEmpty())
			{
				Issue.Summary =
					Mutation.Type == EExtendedAtlassianWorkspaceMutation::CreateCaptureIssue
						? TEXT("Untitled capture from the viewport")
						: TEXT("Untitled card");
			}
			Issue.Description = Field(TEXT("description"));
			Issue.IssueTypeId = Field(TEXT("typeId"));
			Issue.IssueTypeName = Field(TEXT("type"));
			Issue.StatusName = Field(TEXT("status"));
			Issue.StatusId = Field(TEXT("statusId"));
			if (Issue.StatusId.IsEmpty())
			{
				Issue.StatusId = Issue.StatusName;
			}
			Issue.PriorityId = Field(TEXT("priorityId"));
			Issue.PriorityName = Field(TEXT("priority"));
			Issue.AssigneeAccountId = Field(TEXT("assignee"));
			Issue.EpicId = Field(TEXT("epicId"));
			Issue.EpicName = Field(TEXT("epic"));
			Issue.Estimate = FCString::Atod(*Field(TEXT("points")));
			Issue.RelativeUpdated = TEXT("now");
			const FString NewIssueKey = Issue.Key;
			Issue.CommentCount =
				Mutation.Type == EExtendedAtlassianWorkspaceMutation::CreateCaptureIssue
					? FCString::Atoi(*Field(TEXT("annotationCount"))) > 0
					: 0;
			Snapshot.Issues.Insert(MoveTemp(Issue), 0);
			if (Mutation.Type == EExtendedAtlassianWorkspaceMutation::CreateCaptureIssue)
			{
				const int32 PinCount = FCString::Atoi(*Field(TEXT("pinCount")));
				const int32 BoxCount = FCString::Atoi(*Field(TEXT("boxCount")));
				const int32 BlurCount = FCString::Atoi(*Field(TEXT("blurCount")));
				TArray<FString> Parts;
				if (PinCount > 0)
				{
					Parts.Add(FString::Printf(TEXT("%d Ã— pin"), PinCount));
				}
				if (BoxCount > 0)
				{
					Parts.Add(FString::Printf(TEXT("%d Ã— box"), BoxCount));
				}
				if (BlurCount > 0)
				{
					Parts.Add(FString::Printf(TEXT("%d Ã— blur"), BlurCount));
				}
				if (!Parts.IsEmpty())
				{
					FExtendedAtlassianCommentCollection Collection;
					Collection.TargetId = TEXT("issue:") + NewIssueKey;
					FExtendedAtlassianComment Comment;
					Comment.Id = FString::Printf(
						TEXT("capture-comment-%d"),
						NextCommentNumber++);
					Comment.ContainerId = NewIssueKey;
					Comment.AuthorAccountId = Snapshot.CurrentUser.AccountId;
					Comment.AuthorDisplayName = Snapshot.CurrentUser.DisplayName;
					Comment.Body =
						TEXT("Captured from the editor viewport with ")
						+ FString::Join(Parts, TEXT(", "))
						+ TEXT(".");
					Comment.RelativeTime = TEXT("now");
					Comment.bCanEdit = true;
					Comment.bCanDelete = true;
					Collection.Comments.Add(MoveTemp(Comment));
					Snapshot.CommentCollections.Add(MoveTemp(Collection));
				}
				FExtendedAtlassianActivity Activity;
				Activity.Id = TEXT("capture-activity-") + NewIssueKey;
				Activity.IssueKey = NewIssueKey;
				Activity.ActorAccountId = Snapshot.CurrentUser.AccountId;
				Activity.ActorDisplayName = Snapshot.CurrentUser.DisplayName;
				Activity.Verb = TEXT("capture");
				Activity.Detail =
					Snapshot.CurrentUser.DisplayName
					+ TEXT(" created this from a viewport capture.");
				Activity.Created = FDateTime::UtcNow();
				Activity.RelativeTime = TEXT("now");
				Snapshot.Activity.Insert(MoveTemp(Activity), 0);
			}
		}
		break;

	case EExtendedAtlassianWorkspaceMutation::DeleteIssue:
		Snapshot.Issues.RemoveAll(
			[&Mutation](const FExtendedAtlassianIssue& Issue) { return Issue.Key == Mutation.TargetId; });
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
			const FString PreviousEpic = Issue.EpicName;
			if (Mutation.Fields.Contains(TEXT("summary"))) { Issue.Summary = Field(TEXT("summary")); }
			if (Mutation.Fields.Contains(TEXT("description"))) { Issue.Description = Field(TEXT("description")); }
			if (Mutation.Fields.Contains(TEXT("typeId"))) { Issue.IssueTypeId = Field(TEXT("typeId")); }
			if (Mutation.Fields.Contains(TEXT("type"))) { Issue.IssueTypeName = Field(TEXT("type")); }
			if (Mutation.Fields.Contains(TEXT("statusId"))) { Issue.StatusId = Field(TEXT("statusId")); }
			if (Mutation.Fields.Contains(TEXT("status"))) { Issue.StatusName = Field(TEXT("status")); }
			if (Mutation.Fields.Contains(TEXT("priorityId"))) { Issue.PriorityId = Field(TEXT("priorityId")); }
			if (Mutation.Fields.Contains(TEXT("priority"))) { Issue.PriorityName = Field(TEXT("priority")); }
			if (Mutation.Fields.Contains(TEXT("assignee")))
			{
				Issue.AssigneeAccountId = Field(TEXT("assignee"));
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
				Issue.EpicName = Field(TEXT("epic"));
				Issue.ParentSummary = Issue.EpicName;
			}
			if (Mutation.Fields.Contains(TEXT("epicId")))
			{
				Issue.EpicId = Field(TEXT("epicId"));
			}
			if (Mutation.Fields.Contains(TEXT("points"))) { Issue.Estimate = FCString::Atod(*Field(TEXT("points"))); }
			Issue.RelativeUpdated = TEXT("now");
			FExtendedAtlassianActivity Activity;
			Activity.Id = FString::Printf(
				TEXT("%s:mutation:%d"),
				*Issue.Key,
				Snapshot.Activity.Num());
			Activity.IssueKey = Issue.Key;
			Activity.ActorAccountId = Snapshot.CurrentUser.AccountId;
			Activity.ActorDisplayName = Snapshot.CurrentUser.DisplayName;
			Activity.Created = FDateTime::UtcNow();
			Activity.RelativeTime = TEXT("now");
			const FString Actor = Snapshot.CurrentUser.DisplayName;
			if (Mutation.Fields.Contains(TEXT("status")))
			{
				Activity.Verb = TEXT("status");
				Activity.Detail = FString::Printf(
					TEXT("%s moved this from %s to %s."),
					*Actor,
					*PreviousStatus,
					*Issue.StatusName);
			}
			else if (Mutation.Fields.Contains(TEXT("assignee")))
			{
				Activity.Verb = TEXT("assignee");
				Activity.Detail = FString::Printf(
					TEXT("%s assigned this to %s."),
					*Actor,
					*Issue.AssigneeAccountId);
			}
			else if (Mutation.Fields.Contains(TEXT("epic")))
			{
				Activity.Verb = TEXT("epic");
				Activity.Detail = FString::Printf(
					TEXT("%s moved this from %s to the %s epic."),
					*Actor,
					*PreviousEpic,
					*Issue.EpicName);
			}
			else if (Mutation.Fields.Contains(TEXT("priority")))
			{
				Activity.Verb = TEXT("priority");
				Activity.Detail = FString::Printf(
					TEXT("%s changed priority from %s to %s."),
					*Actor,
					*PreviousPriority,
					*Issue.PriorityName);
			}
			else if (Mutation.Fields.Contains(TEXT("points")))
			{
				Activity.Verb = TEXT("estimate");
				Activity.Detail = FString::Printf(
					TEXT("%s estimated this at %.0f points."),
					*Actor,
					Issue.Estimate);
			}
			else
			{
				Activity.Verb = TEXT("edit");
				Activity.Detail =
					Actor + TEXT(" updated the summary or description.");
			}
			Snapshot.Activity.Insert(MoveTemp(Activity), 0);
			break;
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

	case EExtendedAtlassianWorkspaceMutation::CreateIssueComment:
	case EExtendedAtlassianWorkspaceMutation::CreatePageComment:
		{
			const FString Target = Field(TEXT("target"));
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
			const FString Body = Field(TEXT("body")).TrimStartAndEnd();
			if (Body.IsEmpty())
			{
				break;
			}
			FExtendedAtlassianComment Comment;
			Comment.Id = Mutation.TargetId.IsEmpty()
				? FString::Printf(TEXT("c%d"), NextCommentNumber++)
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
					TEXT("%s:comment:%d"),
					*Target,
					Snapshot.Activity.Num());
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
					Comment->Body = Field(TEXT("body"));
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

	case EExtendedAtlassianWorkspaceMutation::CreatePage:
		{
			FExtendedAtlassianPage Page;
			Page.Id = Mutation.TargetId.IsEmpty()
				? FString::Printf(TEXT("new-page-%d"), NextPageNumber++)
				: Mutation.TargetId;
			Page.ParentId = Mutation.ParentId;
			Page.Title = Field(TEXT("title")).TrimStartAndEnd();
			if (Page.Title.IsEmpty())
			{
				Page.Title = TEXT("Untitled page");
			}
			Page.Version = 1;
			Page.EditedByLabel = TEXT("A. KWAN");
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
			if (Mutation.Fields.Contains(TEXT("title"))) { Page.Title = Field(TEXT("title")); }
			if (Mutation.Fields.Contains(TEXT("body")))
			{
				Page.Body = Field(TEXT("body"));
				Page.Markdown = Page.Body;
				++Page.Version;
			}
			if (!Mutation.ParentId.IsEmpty()) { Page.ParentId = Mutation.ParentId; }
			Page.EditedByLabel = TEXT("A. KWAN");
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
				Copy.Id = Field(TEXT("newId"));
				if (Copy.Id.IsEmpty())
				{
					Copy.Id = FString::Printf(TEXT("new-page-%d"), NextPageNumber++);
				}
				Copy.Title = Field(TEXT("title"));
				if (Copy.Title.IsEmpty())
				{
					Copy.Title = Page.Title + TEXT(" copy");
				}
				Copy.Version = 1;
				Copy.EditedByLabel = TEXT("A. KWAN");
				Copy.EditedAtLabel = TEXT("JUST NOW");
				Snapshot.Pages.Add(MoveTemp(Copy));
				break;
			}
		}
		break;

	case EExtendedAtlassianWorkspaceMutation::DeletePage:
		Snapshot.Pages.RemoveAll(
			[&Mutation](const FExtendedAtlassianPage& Page) { return Page.Id == Mutation.TargetId; });
		break;

	case EExtendedAtlassianWorkspaceMutation::CreatePin:
		if (!Field(TEXT("name")).TrimStartAndEnd().IsEmpty())
		{
			FExtendedAtlassianPin Pin;
			Pin.Id = Mutation.TargetId.IsEmpty() ? Field(TEXT("stableId")) : Mutation.TargetId;
			Pin.DisplayName = Field(TEXT("name"));
			Pin.Target.Kind = ExtendedAtlassianFixturePrivate::PinKind(Field(TEXT("kind")));
			Pin.Target.StableId = Field(TEXT("stableId"));
			Pin.Target.DisplayName = Pin.DisplayName;
			Pin.Target.SecondaryId = Field(TEXT("secondaryId"));
			Pin.Color = Field(TEXT("color"));
			Pin.Version = 1;
			Snapshot.Pins.Insert(MoveTemp(Pin), 0);
		}
		break;

	case EExtendedAtlassianWorkspaceMutation::UpdatePin:
		for (FExtendedAtlassianPin& Pin : Snapshot.Pins)
		{
			if (Pin.Id == Mutation.TargetId)
			{
				Pin.DisplayName = Field(TEXT("name"));
				Pin.Target.DisplayName = Pin.DisplayName;
				++Pin.Version;
				break;
			}
		}
		break;

	case EExtendedAtlassianWorkspaceMutation::DeletePin:
		Snapshot.Pins.RemoveAll(
			[&Mutation](const FExtendedAtlassianPin& Pin) { return Pin.Id == Mutation.TargetId; });
		break;

	case EExtendedAtlassianWorkspaceMutation::CreatePinReply:
		for (FExtendedAtlassianPin& Pin : Snapshot.Pins)
		{
			if (Pin.Id != Mutation.ParentId)
			{
				continue;
			}
			const FString Body = Field(TEXT("body")).TrimStartAndEnd();
			if (Body.IsEmpty())
			{
				break;
			}
			FExtendedAtlassianPinThread Thread;
			Thread.Id = Mutation.TargetId.IsEmpty()
				? FString::Printf(TEXT("%s:thread:%d"), *Pin.Id, NextCommentNumber++)
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
					Thread.Body = Field(TEXT("body"));
				}
				if (Mutation.Type == EExtendedAtlassianWorkspaceMutation::ResolvePinReply)
				{
					Thread.bResolved =
						Mutation.Fields.Contains(TEXT("resolved"))
							? Field(TEXT("resolved")).ToBool()
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

	case EExtendedAtlassianWorkspaceMutation::MarkAllNotificationsRead:
		for (FExtendedAtlassianNotification& Notification : Snapshot.Notifications)
		{
			Notification.bRead = true;
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

	default:
		break;
	}
	ExtendedAtlassianModelUtils::RefreshCommentPresentation(Snapshot);
}
