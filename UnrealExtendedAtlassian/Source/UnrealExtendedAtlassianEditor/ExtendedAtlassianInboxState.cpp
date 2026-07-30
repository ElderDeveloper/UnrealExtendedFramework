// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianInboxState.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace ExtendedAtlassianInboxStatePrivate
{
	FString Revision(const FDateTime& Timestamp, int32 Fallback)
	{
		return Timestamp == FDateTime::MinValue()
			? FString::FromInt(Fallback)
			: FString::Printf(TEXT("%lld"), Timestamp.GetTicks());
	}

	bool MentionsUser(
		const FString& Body,
		const FExtendedAtlassianUser& User)
	{
		if (Body.Contains(TEXT("@") + User.DisplayName, ESearchCase::IgnoreCase))
		{
			return true;
		}
		if (!User.AccountId.IsEmpty())
		{
			return Body.Contains(
				User.AccountId,
				ESearchCase::IgnoreCase);
		}
		return false;
	}

	void AddEvent(
		TArray<FExtendedAtlassianNotification>& Events,
		EExtendedAtlassianNotificationKind Kind,
		const FString& Source,
		const FString& SourceId,
		const FString& ObjectId,
		const FString& RevisionValue,
		const FString& ActorAccountId,
		const FString& ActorDisplayName,
		const FString& Action,
		const FString& Target,
		const FString& Quote,
		const FDateTime& Created,
		const FString& RelativeTime)
	{
		FExtendedAtlassianNotification Event;
		Event.Id = FExtendedAtlassianInboxState::MakeStableEventId(
			SourceId,
			ObjectId,
			LexToString(static_cast<uint8>(Kind)),
			RevisionValue);
		Event.Kind = Kind;
		Event.Source = Source;
		Event.SourceId = SourceId;
		Event.ActorAccountId = ActorAccountId;
		Event.ActorDisplayName =
			ActorDisplayName.IsEmpty() ? TEXT("Atlassian") : ActorDisplayName;
		Event.Action = Action;
		Event.Target = Target;
		Event.Quote = Quote;
		Event.Created = Created;
		Event.RelativeTime =
			RelativeTime.IsEmpty() ? TEXT("now") : RelativeTime;
		Events.Add(MoveTemp(Event));
	}

	TArray<TSharedPtr<FJsonValue>> ToJsonArray(const TSet<FString>& Values)
	{
		TArray<FString> Sorted = Values.Array();
		Sorted.Sort();
		TArray<TSharedPtr<FJsonValue>> Result;
		Result.Reserve(Sorted.Num());
		for (const FString& Value : Sorted)
		{
			Result.Add(MakeShared<FJsonValueString>(Value));
		}
		return Result;
	}

	void FromJsonArray(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* Field,
		TSet<FString>& OutValues)
	{
		OutValues.Reset();
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object.IsValid() || !Object->TryGetArrayField(Field, Values))
		{
			return;
		}
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			FString Text;
			if (Value.IsValid() && Value->TryGetString(Text) && !Text.IsEmpty())
			{
				OutValues.Add(MoveTemp(Text));
			}
		}
	}

	void UpdateCursor(
		FExtendedAtlassianInboxUserState& State,
		const FString& Source,
		const FDateTime& Timestamp)
	{
		if (Timestamp == FDateTime::MinValue())
		{
			return;
		}
		const FString Candidate =
			FString::Printf(TEXT("%019lld"), Timestamp.GetTicks());
		FString& Cursor = State.SourceCursors.FindOrAdd(Source);
		if (Candidate > Cursor)
		{
			Cursor = Candidate;
		}
	}
}

FString FExtendedAtlassianInboxState::MakeStableEventId(
	const FString& Source,
	const FString& SourceObjectId,
	const FString& EventKind,
	const FString& Revision)
{
	const FString Seed =
		Source + TEXT("\x1f")
		+ SourceObjectId + TEXT("\x1f")
		+ EventKind + TEXT("\x1f")
		+ Revision;
	return TEXT("evt-") + FMD5::HashAnsiString(*Seed).ToLower();
}

void FExtendedAtlassianInboxState::SynthesizeAndApply(
	FExtendedAtlassianWorkspaceSnapshot& Snapshot,
	FExtendedAtlassianInboxUserState& UserState)
{
	using namespace ExtendedAtlassianInboxStatePrivate;
	TArray<FExtendedAtlassianNotification> Events;

	for (const FExtendedAtlassianIssue& Issue : Snapshot.Issues)
	{
		const FString Target = Issue.Key + TEXT(" · ") + Issue.Summary;
		const FString EventRevision = Revision(Issue.Updated, 0);
		if (!Snapshot.CurrentUser.AccountId.IsEmpty()
			&& Issue.AssigneeAccountId == Snapshot.CurrentUser.AccountId)
		{
			AddEvent(
				Events,
				EExtendedAtlassianNotificationKind::Assign,
				TEXT("Jira"),
				Issue.Key,
				Issue.Key,
				EventRevision,
				FString(),
				Issue.ReporterDisplayName,
				TEXT("assigned this issue to you"),
				Target,
				Issue.Description.Left(240),
				Issue.Updated,
				Issue.RelativeUpdated);
		}
		else if (!Issue.StatusName.IsEmpty())
		{
			AddEvent(
				Events,
				EExtendedAtlassianNotificationKind::Status,
				TEXT("Jira"),
				Issue.Key,
				Issue.Key,
				EventRevision,
				FString(),
				Issue.AssigneeDisplayName,
				TEXT("updated the issue"),
				Target,
				Issue.StatusName,
				Issue.Updated,
				Issue.RelativeUpdated);
		}
		UpdateCursor(UserState, TEXT("jira"), Issue.Updated);
	}

	for (const FExtendedAtlassianPage& Page : Snapshot.Pages)
	{
		if (!Page.ReviewState.IsEmpty())
		{
			AddEvent(
				Events,
				EExtendedAtlassianNotificationKind::Review,
				TEXT("Confluence"),
				TEXT("docs"),
				Page.Id,
				Revision(Page.VersionCreatedAt, Page.Version),
				Page.AuthorAccountId,
				Page.AuthorDisplayName,
				TEXT("requested a document review"),
				Page.Title,
				Page.MilestoneText,
				Page.VersionCreatedAt,
				Page.EditedAtLabel);
		}
		UpdateCursor(UserState, TEXT("confluence"), Page.VersionCreatedAt);
	}

	for (const FExtendedAtlassianCommentCollection& Collection :
		Snapshot.CommentCollections)
	{
		const bool bPage = Collection.TargetId.StartsWith(TEXT("page:"));
		const FString ContainerId =
			Collection.TargetId.RightChop(bPage ? 5 : 6);
		auto AppendComment = [
			&Events,
			&Snapshot,
			bPage,
			&ContainerId,
			&UserState
		](const FExtendedAtlassianComment& Comment)
		{
			if (Comment.AuthorAccountId == Snapshot.CurrentUser.AccountId)
			{
				return;
			}
			const bool bMention = MentionsUser(Comment.Body, Snapshot.CurrentUser);
			AddEvent(
				Events,
				bMention
					? EExtendedAtlassianNotificationKind::Mention
					: EExtendedAtlassianNotificationKind::Comment,
				bPage ? TEXT("Confluence") : TEXT("Jira"),
				bPage ? TEXT("docs") : ContainerId,
				Comment.Id,
				Revision(Comment.Updated, Comment.Version),
				Comment.AuthorAccountId,
				Comment.AuthorDisplayName,
				bMention ? TEXT("mentioned you") : TEXT("commented"),
				bPage ? ContainerId : ContainerId,
				Comment.Body.Left(320),
				Comment.Updated == FDateTime::MinValue()
					? Comment.Created
					: Comment.Updated,
				Comment.RelativeTime);
			UpdateCursor(
				UserState,
				bPage ? TEXT("confluence-comments") : TEXT("jira-comments"),
				Comment.Updated == FDateTime::MinValue()
					? Comment.Created
					: Comment.Updated);
		};
		for (const FExtendedAtlassianComment& Comment : Collection.Comments)
		{
			AppendComment(Comment);
			for (const FExtendedAtlassianComment& Reply : Comment.Replies)
			{
				AppendComment(Reply);
			}
		}
	}

	for (const FExtendedAtlassianPin& Pin : Snapshot.Pins)
	{
		for (const FExtendedAtlassianPinThread& Thread : Pin.Threads)
		{
			if (Thread.AuthorAccountId == Snapshot.CurrentUser.AccountId)
			{
				continue;
			}
			const bool bLinked = !Thread.LinkedLabel.IsEmpty();
			AddEvent(
				Events,
				bLinked
					? EExtendedAtlassianNotificationKind::Link
					: EExtendedAtlassianNotificationKind::Pin,
				TEXT("Backlot Pins"),
				TEXT("pins"),
				Thread.Id,
				Revision(Thread.Updated, Pin.Version),
				Thread.AuthorAccountId,
				Thread.AuthorDisplayName,
				bLinked ? TEXT("linked a Pin thread") : TEXT("replied to a Pin"),
				Pin.DisplayName,
				Thread.Body.Left(320),
				Thread.Updated == FDateTime::MinValue()
					? Thread.Created
					: Thread.Updated,
				Thread.RelativeTime);
			UpdateCursor(
				UserState,
				TEXT("pins"),
				Thread.Updated == FDateTime::MinValue()
					? Thread.Created
					: Thread.Updated);
		}
	}

	Events.Sort(
		[](const FExtendedAtlassianNotification& A,
			const FExtendedAtlassianNotification& B)
		{
			if (A.Created != B.Created)
			{
				return A.Created > B.Created;
			}
			return A.Id < B.Id;
		});
	if (Events.Num() > 500)
	{
		Events.SetNum(500);
	}

	const bool bFirstRun = !UserState.bInitialized;
	for (FExtendedAtlassianNotification& Event : Events)
	{
		if (bFirstRun)
		{
			UserState.ReadEventIds.Add(Event.Id);
		}
		UserState.KnownEventIds.Add(Event.Id);
		Event.bRead = UserState.ReadEventIds.Contains(Event.Id);
		Event.bMuted = UserState.MutedEventIds.Contains(Event.Id);
		Event.bArchived = UserState.ArchivedEventIds.Contains(Event.Id);
	}
	UserState.bInitialized = true;
	Events.RemoveAll(
		[&UserState](const FExtendedAtlassianNotification& Event)
		{
			return UserState.DismissedEventIds.Contains(Event.Id);
		});
	Snapshot.Notifications = MoveTemp(Events);
}

void FExtendedAtlassianInboxState::ApplyMutation(
	const FExtendedAtlassianWorkspaceMutation& Mutation,
	const TArray<FExtendedAtlassianNotification>& CurrentNotifications,
	FExtendedAtlassianInboxUserState& UserState)
{
	switch (Mutation.Type)
	{
	case EExtendedAtlassianWorkspaceMutation::MarkNotificationRead:
		UserState.ReadEventIds.Add(Mutation.TargetId);
		break;
	case EExtendedAtlassianWorkspaceMutation::MarkAllNotificationsRead:
		for (const FExtendedAtlassianNotification& Notification :
			CurrentNotifications)
		{
			UserState.ReadEventIds.Add(Notification.Id);
		}
		break;
	case EExtendedAtlassianWorkspaceMutation::DismissNotification:
		UserState.DismissedEventIds.Add(Mutation.TargetId);
		break;
	case EExtendedAtlassianWorkspaceMutation::ArchiveNotifications:
		for (const FExtendedAtlassianNotification& Notification :
			CurrentNotifications)
		{
			if (Notification.bRead)
			{
				UserState.ArchivedEventIds.Add(Notification.Id);
			}
		}
		break;
	case EExtendedAtlassianWorkspaceMutation::MuteNotification:
		UserState.ReadEventIds.Add(Mutation.TargetId);
		UserState.MutedEventIds.Add(Mutation.TargetId);
		break;
	default:
		break;
	}
}

FString FExtendedAtlassianInboxState::StatePathForAccount(
	const FString& AccountId)
{
	const FString AccountHash =
		FMD5::HashAnsiString(*AccountId).ToLower();
	return FPaths::Combine(
		FPlatformProcess::UserSettingsDir(),
		TEXT("UnrealExtendedAtlassian"),
		TEXT("Inbox"),
		AccountHash + TEXT(".json"));
}

bool FExtendedAtlassianInboxState::Serialize(
	const FExtendedAtlassianInboxUserState& State,
	FString& OutJson,
	FString& OutError)
{
	OutJson.Reset();
	OutError.Reset();
	TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetNumberField(TEXT("schemaVersion"), State.SchemaVersion);
	Object->SetBoolField(TEXT("initialized"), State.bInitialized);
	Object->SetArrayField(
		TEXT("known"),
		ExtendedAtlassianInboxStatePrivate::ToJsonArray(State.KnownEventIds));
	Object->SetArrayField(
		TEXT("read"),
		ExtendedAtlassianInboxStatePrivate::ToJsonArray(State.ReadEventIds));
	Object->SetArrayField(
		TEXT("muted"),
		ExtendedAtlassianInboxStatePrivate::ToJsonArray(State.MutedEventIds));
	Object->SetArrayField(
		TEXT("archived"),
		ExtendedAtlassianInboxStatePrivate::ToJsonArray(State.ArchivedEventIds));
	Object->SetArrayField(
		TEXT("dismissed"),
		ExtendedAtlassianInboxStatePrivate::ToJsonArray(State.DismissedEventIds));
	TSharedRef<FJsonObject> Cursors = MakeShared<FJsonObject>();
	for (const TPair<FString, FString>& Pair : State.SourceCursors)
	{
		Cursors->SetStringField(Pair.Key, Pair.Value);
	}
	Object->SetObjectField(TEXT("sourceCursors"), Cursors);
	const TSharedRef<TJsonWriter<>> Writer =
		TJsonWriterFactory<>::Create(&OutJson);
	if (!FJsonSerializer::Serialize(Object, Writer))
	{
		OutError = TEXT("Unable to serialize Inbox state.");
		return false;
	}
	return true;
}

bool FExtendedAtlassianInboxState::Deserialize(
	const FString& Json,
	FExtendedAtlassianInboxUserState& OutState,
	FString& OutError)
{
	OutState = FExtendedAtlassianInboxUserState();
	OutError.Reset();
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
	{
		OutError =
			TEXT("Inbox state is corrupt; current remote events will be treated as existing.");
		return false;
	}
	double Version = 1.0;
	Object->TryGetNumberField(TEXT("schemaVersion"), Version);
	OutState.SchemaVersion = static_cast<int32>(Version);
	if (OutState.SchemaVersion > 1)
	{
		OutError = TEXT("Inbox state was written by a newer plugin version.");
		return false;
	}
	Object->TryGetBoolField(TEXT("initialized"), OutState.bInitialized);
	ExtendedAtlassianInboxStatePrivate::FromJsonArray(
		Object,
		TEXT("known"),
		OutState.KnownEventIds);
	ExtendedAtlassianInboxStatePrivate::FromJsonArray(
		Object,
		TEXT("read"),
		OutState.ReadEventIds);
	ExtendedAtlassianInboxStatePrivate::FromJsonArray(
		Object,
		TEXT("muted"),
		OutState.MutedEventIds);
	ExtendedAtlassianInboxStatePrivate::FromJsonArray(
		Object,
		TEXT("archived"),
		OutState.ArchivedEventIds);
	ExtendedAtlassianInboxStatePrivate::FromJsonArray(
		Object,
		TEXT("dismissed"),
		OutState.DismissedEventIds);
	const TSharedPtr<FJsonObject>* Cursors = nullptr;
	if (Object->TryGetObjectField(TEXT("sourceCursors"), Cursors)
		&& Cursors && Cursors->IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair :
			(*Cursors)->Values)
		{
			FString Cursor;
			if (Pair.Value.IsValid() && Pair.Value->TryGetString(Cursor))
			{
				OutState.SourceCursors.Add(Pair.Key, MoveTemp(Cursor));
			}
		}
	}
	return true;
}

bool FExtendedAtlassianInboxState::Load(
	const FString& AccountId,
	FExtendedAtlassianInboxUserState& OutState,
	FString& OutError)
{
	OutState = FExtendedAtlassianInboxUserState();
	OutError.Reset();
	if (AccountId.IsEmpty())
	{
		OutError = TEXT("Cannot load Inbox state without an Atlassian account ID.");
		return false;
	}
	FString Json;
	const FString Path = StatePathForAccount(AccountId);
	if (!FPaths::FileExists(Path))
	{
		return true;
	}
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		OutError = TEXT("Unable to read the Inbox state file.");
		return false;
	}
	return Deserialize(Json, OutState, OutError);
}

bool FExtendedAtlassianInboxState::Save(
	const FString& AccountId,
	const FExtendedAtlassianInboxUserState& State,
	FString& OutError)
{
	OutError.Reset();
	if (AccountId.IsEmpty())
	{
		OutError = TEXT("Cannot save Inbox state without an Atlassian account ID.");
		return false;
	}
	FString Json;
	if (!Serialize(State, Json, OutError))
	{
		return false;
	}
	const FString Path = StatePathForAccount(AccountId);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	const FString TemporaryPath = Path + TEXT(".tmp");
	if (!FFileHelper::SaveStringToFile(
		Json,
		*TemporaryPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutError = TEXT("Unable to write Inbox state.");
		return false;
	}
	if (!IFileManager::Get().Move(
		*Path,
		*TemporaryPath,
		true,
		true,
		false,
		true))
	{
		IFileManager::Get().Delete(*TemporaryPath);
		OutError = TEXT("Unable to replace Inbox state atomically.");
		return false;
	}
	return true;
}
