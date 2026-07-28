// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianJira.h"

#include "ExtendedAtlassianAdf.h"
#include "ExtendedAtlassianClient.h"
#include "ExtendedAtlassianLog.h"
#include "ExtendedAtlassianMultipart.h"
#include "ExtendedAtlassianSettings.h"
#include "UnrealExtendedAtlassian.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace ExtendedAtlassianJiraPrivate
{
	/** Which search endpoint this site accepts. Resolved once per session on first use. */
	enum class ESearchEndpoint : uint8
	{
		Unknown,
		JqlPost,
		LegacyGet,
	};

	ESearchEndpoint ResolvedEndpoint = ESearchEndpoint::Unknown;

	/** Jira caps page size well below this, but asking for 50 keeps round trips down. */
	constexpr int32 PageSize = 50;

	const TCHAR* IssueFieldNames[] = {
		TEXT("summary"),
		TEXT("status"),
		TEXT("issuetype"),
		TEXT("priority"),
		TEXT("assignee"),
		TEXT("reporter"),
		TEXT("labels"),
		TEXT("created"),
		TEXT("updated"),
		TEXT("description"),
	};

	FString GetFieldsCsv()
	{
		TArray<FString> Names;
		for (const TCHAR* Name : IssueFieldNames)
		{
			Names.Add(Name);
		}
		return FString::Join(Names, TEXT(","));
	}

	struct FSearchState
	{
		FString Jql;
		int32 MaxResults = 100;
		TArray<FExtendedAtlassianIssue> Issues;

		/** Cursor for POST /search/jql. */
		FString NextPageToken;

		/** Cursor for the legacy GET /search. */
		int32 StartAt = 0;

		/** Endpoint used for the request currently in flight. */
		ESearchEndpoint UsedEndpoint = ESearchEndpoint::Unknown;

		bool bTriedFallback = false;
		bool bTruncated = false;

		FExtendedAtlassianIssuesDelegate OnComplete;
	};

	void FetchPage(TSharedRef<FSearchState> State);

	void CompleteSearch(const TSharedRef<FSearchState>& State, bool bSuccess, const FExtendedAtlassianError& Error)
	{
		FExtendedAtlassianIssueQueryResult Result;
		Result.Issues = MoveTemp(State->Issues);
		Result.bSuccess = bSuccess;
		Result.Error = Error;
		Result.bTruncated = State->bTruncated;

		State->OnComplete.ExecuteIfBound(Result);
	}

	void HandlePage(TSharedRef<FSearchState> State, const FExtendedAtlassianResponse& Response)
	{
		if (!Response.IsSuccess())
		{
			// Atlassian removed GET /search in favour of POST /search/jql, but the rollout is not
			// uniform across sites. If the new endpoint is missing, drop to the old one once and
			// remember the answer for the rest of the session.
			const bool bEndpointMissing = Response.Error.HttpStatus == 404 || Response.Error.HttpStatus == 410;

			if (bEndpointMissing && State->UsedEndpoint == ESearchEndpoint::JqlPost && !State->bTriedFallback)
			{
				UE_LOG(LogExtendedAtlassian, Log,
					TEXT("POST /search/jql is unavailable on this site (%d); falling back to GET /search."),
					Response.Error.HttpStatus);

				State->bTriedFallback = true;
				ResolvedEndpoint = ESearchEndpoint::LegacyGet;
				FetchPage(State);
				return;
			}

			CompleteSearch(State, false, Response.Error);
			return;
		}

		// The request succeeded, so this endpoint is the right one for this site.
		if (ResolvedEndpoint == ESearchEndpoint::Unknown)
		{
			ResolvedEndpoint = State->UsedEndpoint;
		}

		if (!Response.Object.IsValid())
		{
			FExtendedAtlassianError Error;
			Error.Code = TEXT("UnexpectedResponse");
			Error.HttpStatus = Response.HttpStatus;
			Error.Message = TEXT("Jira returned a search response that could not be parsed.");
			CompleteSearch(State, false, Error);
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* Issues = nullptr;
		if (Response.Object->TryGetArrayField(TEXT("issues"), Issues))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Issues)
			{
				const TSharedPtr<FJsonObject>* IssueObject = nullptr;
				if (Value->TryGetObject(IssueObject) && IssueObject->IsValid())
				{
					State->Issues.Add(FExtendedAtlassianJira::ParseIssue(*IssueObject));
				}
			}
		}

		// Work out whether another page exists, in whichever dialect this endpoint speaks.
		bool bMorePagesExist = false;

		if (State->UsedEndpoint == ESearchEndpoint::JqlPost)
		{
			FString NextToken;
			Response.Object->TryGetStringField(TEXT("nextPageToken"), NextToken);

			bool bIsLast = NextToken.IsEmpty();
			Response.Object->TryGetBoolField(TEXT("isLast"), bIsLast);

			State->NextPageToken = NextToken;
			bMorePagesExist = !bIsLast && !NextToken.IsEmpty();
		}
		else
		{
			int32 Total = 0;
			Response.Object->TryGetNumberField(TEXT("total"), Total);

			State->StartAt = State->Issues.Num();
			bMorePagesExist = State->Issues.Num() < Total;
		}

		if (State->Issues.Num() >= State->MaxResults)
		{
			State->bTruncated = bMorePagesExist;
			CompleteSearch(State, true, FExtendedAtlassianError());
			return;
		}

		if (bMorePagesExist)
		{
			FetchPage(State);
			return;
		}

		CompleteSearch(State, true, FExtendedAtlassianError());
	}

	void FetchPage(TSharedRef<FSearchState> State)
	{
		const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient();
		const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();

		if (!Client.IsValid() || !Settings)
		{
			FExtendedAtlassianError Error;
			Error.Code = TEXT("NotReady");
			Error.Message = TEXT("The Atlassian transport module is not available.");
			CompleteSearch(State, false, Error);
			return;
		}

		const FString Base = Settings->GetJiraApiBaseUrl();
		const int32 Remaining = FMath::Max(1, State->MaxResults - State->Issues.Num());
		const int32 RequestedCount = FMath::Min(Remaining, PageSize);

		FExtendedAtlassianResponseDelegate Handler = FExtendedAtlassianResponseDelegate::CreateLambda(
			[State](const FExtendedAtlassianResponse& Response)
			{
				HandlePage(State, Response);
			});

		if (ResolvedEndpoint == ESearchEndpoint::LegacyGet)
		{
			State->UsedEndpoint = ESearchEndpoint::LegacyGet;

			const FString Url = FString::Printf(
				TEXT("%s/search?jql=%s&startAt=%d&maxResults=%d&fields=%s"),
				*Base,
				*FGenericPlatformHttp::UrlEncode(State->Jql),
				State->StartAt,
				RequestedCount,
				*FGenericPlatformHttp::UrlEncode(GetFieldsCsv()));

			Client->Get(Url, Handler);
			return;
		}

		State->UsedEndpoint = ESearchEndpoint::JqlPost;

		FString Body;
		{
			const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
				TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);

			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("jql"), State->Jql);
			Writer->WriteValue(TEXT("maxResults"), RequestedCount);

			Writer->WriteArrayStart(TEXT("fields"));
			for (const TCHAR* Name : IssueFieldNames)
			{
				Writer->WriteValue(FString(Name));
			}
			Writer->WriteArrayEnd();

			if (!State->NextPageToken.IsEmpty())
			{
				Writer->WriteValue(TEXT("nextPageToken"), State->NextPageToken);
			}

			Writer->WriteObjectEnd();
			Writer->Close();
		}

		Client->PostJson(Base + TEXT("/search/jql"), Body, Handler);
	}

	FString GetNestedString(const TSharedPtr<FJsonObject>& Parent, const TCHAR* ObjectField, const TCHAR* StringField)
	{
		const TSharedPtr<FJsonObject>* Child = nullptr;
		if (Parent->TryGetObjectField(ObjectField, Child) && Child->IsValid())
		{
			FString Value;
			(*Child)->TryGetStringField(StringField, Value);
			return Value;
		}
		return FString();
	}
}

void FExtendedAtlassianJira::SearchIssues(const FString& Jql, int32 MaxResults, FExtendedAtlassianIssuesDelegate OnComplete)
{
	using namespace ExtendedAtlassianJiraPrivate;

	TSharedRef<FSearchState> State = MakeShared<FSearchState>();
	State->Jql = Jql;
	State->MaxResults = FMath::Max(1, MaxResults);
	State->OnComplete = OnComplete;

	FetchPage(State);
}

FExtendedAtlassianIssue FExtendedAtlassianJira::ParseIssue(const TSharedPtr<FJsonObject>& IssueJson)
{
	using namespace ExtendedAtlassianJiraPrivate;

	FExtendedAtlassianIssue Issue;
	if (!IssueJson.IsValid())
	{
		return Issue;
	}

	IssueJson->TryGetStringField(TEXT("id"), Issue.Id);
	IssueJson->TryGetStringField(TEXT("key"), Issue.Key);

	const TSharedPtr<FJsonObject>* FieldsPtr = nullptr;
	if (!IssueJson->TryGetObjectField(TEXT("fields"), FieldsPtr) || !FieldsPtr->IsValid())
	{
		return Issue;
	}

	const TSharedPtr<FJsonObject>& Fields = *FieldsPtr;

	Fields->TryGetStringField(TEXT("summary"), Issue.Summary);

	const TSharedPtr<FJsonObject>* Status = nullptr;
	if (Fields->TryGetObjectField(TEXT("status"), Status) && Status->IsValid())
	{
		(*Status)->TryGetStringField(TEXT("name"), Issue.StatusName);
		Issue.StatusCategoryKey = GetNestedString(*Status, TEXT("statusCategory"), TEXT("key"));
	}

	const TSharedPtr<FJsonObject>* IssueType = nullptr;
	if (Fields->TryGetObjectField(TEXT("issuetype"), IssueType) && IssueType->IsValid())
	{
		(*IssueType)->TryGetStringField(TEXT("name"), Issue.IssueTypeName);
		(*IssueType)->TryGetStringField(TEXT("iconUrl"), Issue.IssueTypeIconUrl);
	}

	Issue.PriorityName = GetNestedString(Fields, TEXT("priority"), TEXT("name"));
	Issue.ReporterDisplayName = GetNestedString(Fields, TEXT("reporter"), TEXT("displayName"));

	const TSharedPtr<FJsonObject>* Assignee = nullptr;
	if (Fields->TryGetObjectField(TEXT("assignee"), Assignee) && Assignee->IsValid())
	{
		(*Assignee)->TryGetStringField(TEXT("displayName"), Issue.AssigneeDisplayName);
		(*Assignee)->TryGetStringField(TEXT("accountId"), Issue.AssigneeAccountId);
	}

	const TArray<TSharedPtr<FJsonValue>>* Labels = nullptr;
	if (Fields->TryGetArrayField(TEXT("labels"), Labels))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Labels)
		{
			const FString Label = Value->AsString();
			if (!Label.IsEmpty())
			{
				Issue.Labels.Add(Label);
			}
		}
	}

	FString CreatedRaw;
	if (Fields->TryGetStringField(TEXT("created"), CreatedRaw))
	{
		ParseJiraDateTime(CreatedRaw, Issue.Created);
	}

	FString UpdatedRaw;
	if (Fields->TryGetStringField(TEXT("updated"), UpdatedRaw))
	{
		ParseJiraDateTime(UpdatedRaw, Issue.Updated);
	}

	// description is an ADF document, or null on issues that have none.
	const TSharedPtr<FJsonObject>* Description = nullptr;
	if (Fields->TryGetObjectField(TEXT("description"), Description) && Description->IsValid())
	{
		Issue.Description = FExtendedAtlassianAdf::ToPlainText(*Description);
	}

	return Issue;
}

bool FExtendedAtlassianJira::ParseJiraDateTime(const FString& In, FDateTime& Out)
{
	if (In.IsEmpty())
	{
		return false;
	}

	FString Normalized = In;

	// Jira emits offsets as +0000; ParseIso8601 requires +00:00.
	const int32 Length = Normalized.Len();
	if (Length > 5)
	{
		const TCHAR Sign = Normalized[Length - 5];
		if ((Sign == TEXT('+') || Sign == TEXT('-')) &&
			FChar::IsDigit(Normalized[Length - 4]) &&
			FChar::IsDigit(Normalized[Length - 3]) &&
			FChar::IsDigit(Normalized[Length - 2]) &&
			FChar::IsDigit(Normalized[Length - 1]))
		{
			Normalized = Normalized.Left(Length - 2) + TEXT(":") + Normalized.Right(2);
		}
	}

	return FDateTime::ParseIso8601(*Normalized, Out);
}

TArray<FString> FExtendedAtlassianJira::ExtractIssueKeys(const FString& Text, const TArray<FString>& ProjectKeys)
{
	TArray<FString> Keys;

	if (Text.IsEmpty())
	{
		return Keys;
	}

	for (const FString& RawProject : ProjectKeys)
	{
		const FString Project = RawProject.TrimStartAndEnd();
		if (Project.IsEmpty())
		{
			continue;
		}

		const FString Needle = Project + TEXT("-");

		int32 SearchStart = 0;
		while (SearchStart < Text.Len())
		{
			const int32 Found = Text.Find(Needle, ESearchCase::CaseSensitive, ESearchDir::FromStart, SearchStart);
			if (Found == INDEX_NONE)
			{
				break;
			}

			// The character before must not be alphanumeric, or "XTOT-1" would match "TOT-1".
			const bool bBoundaryBefore = Found == 0 || !FChar::IsAlnum(Text[Found - 1]);

			int32 DigitEnd = Found + Needle.Len();
			while (DigitEnd < Text.Len() && FChar::IsDigit(Text[DigitEnd]))
			{
				++DigitEnd;
			}

			const int32 DigitCount = DigitEnd - (Found + Needle.Len());

			if (bBoundaryBefore && DigitCount > 0)
			{
				const FString Key = Text.Mid(Found, DigitEnd - Found);
				Keys.AddUnique(Key);
			}

			SearchStart = Found + Needle.Len();
		}
	}

	return Keys;
}

TArray<FString> FExtendedAtlassianJira::ParseLabels(const FString& CommaSeparated)
{
	TArray<FString> Labels;
	CommaSeparated.ParseIntoArray(Labels, TEXT(","), true);

	for (FString& Label : Labels)
	{
		Label.TrimStartAndEndInline();
		// Jira rejects labels containing spaces outright.
		Label.ReplaceInline(TEXT(" "), TEXT("-"));
	}

	Labels.RemoveAll([](const FString& Label) { return Label.IsEmpty(); });

	return Labels;
}

FString FExtendedAtlassianJira::GetIssueBrowseUrl(const FString& IssueKey)
{
	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();
	if (!Settings || IssueKey.IsEmpty())
	{
		return FString();
	}

	const FString Base = Settings->GetNormalizedSiteUrl();
	return Base.IsEmpty() ? FString() : Base + TEXT("/browse/") + IssueKey;
}

FString FExtendedAtlassianJira::GetResolvedSearchEndpointName()
{
	switch (ExtendedAtlassianJiraPrivate::ResolvedEndpoint)
	{
	case ExtendedAtlassianJiraPrivate::ESearchEndpoint::JqlPost:   return TEXT("POST /search/jql");
	case ExtendedAtlassianJiraPrivate::ESearchEndpoint::LegacyGet: return TEXT("GET /search");
	default:                                                       return TEXT("unresolved");
	}
}

void FExtendedAtlassianJira::GetIssue(const FString& IssueKey, FExtendedAtlassianIssueDelegate OnComplete)
{
	const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient();
	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();

	if (!Client.IsValid() || !Settings)
	{
		FExtendedAtlassianError Error;
		Error.Code = TEXT("NotReady");
		Error.Message = TEXT("The Atlassian transport module is not available.");
		OnComplete.ExecuteIfBound(false, FExtendedAtlassianIssue(), Error);
		return;
	}

	const FString Url = FString::Printf(TEXT("%s/issue/%s?fields=%s"),
		*Settings->GetJiraApiBaseUrl(),
		*IssueKey,
		*FGenericPlatformHttp::UrlEncode(ExtendedAtlassianJiraPrivate::GetFieldsCsv()));

	Client->Get(Url, FExtendedAtlassianResponseDelegate::CreateLambda(
		[OnComplete](const FExtendedAtlassianResponse& Response)
		{
			if (!Response.IsSuccess())
			{
				OnComplete.ExecuteIfBound(false, FExtendedAtlassianIssue(), Response.Error);
				return;
			}

			OnComplete.ExecuteIfBound(true, ParseIssue(Response.Object), FExtendedAtlassianError());
		}));
}

void FExtendedAtlassianJira::GetTransitions(const FString& IssueKey, FExtendedAtlassianTransitionsDelegate OnComplete)
{
	const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient();
	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();

	if (!Client.IsValid() || !Settings)
	{
		FExtendedAtlassianError Error;
		Error.Code = TEXT("NotReady");
		Error.Message = TEXT("The Atlassian transport module is not available.");
		OnComplete.ExecuteIfBound(false, TArray<FExtendedAtlassianTransition>(), Error);
		return;
	}

	const FString Url = FString::Printf(TEXT("%s/issue/%s/transitions"), *Settings->GetJiraApiBaseUrl(), *IssueKey);

	Client->Get(Url, FExtendedAtlassianResponseDelegate::CreateLambda(
		[OnComplete](const FExtendedAtlassianResponse& Response)
		{
			if (!Response.IsSuccess() || !Response.Object.IsValid())
			{
				OnComplete.ExecuteIfBound(false, TArray<FExtendedAtlassianTransition>(), Response.Error);
				return;
			}

			TArray<FExtendedAtlassianTransition> Transitions;

			const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
			if (Response.Object->TryGetArrayField(TEXT("transitions"), Array))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Array)
				{
					const TSharedPtr<FJsonObject>* Object = nullptr;
					if (!Value->TryGetObject(Object) || !Object->IsValid())
					{
						continue;
					}

					FExtendedAtlassianTransition Transition;
					(*Object)->TryGetStringField(TEXT("id"), Transition.Id);
					(*Object)->TryGetStringField(TEXT("name"), Transition.Name);

					const TSharedPtr<FJsonObject>* To = nullptr;
					if ((*Object)->TryGetObjectField(TEXT("to"), To) && To->IsValid())
					{
						(*To)->TryGetStringField(TEXT("name"), Transition.ToStatusName);
						Transition.ToStatusCategoryKey =
							ExtendedAtlassianJiraPrivate::GetNestedString(*To, TEXT("statusCategory"), TEXT("key"));
					}

					Transitions.Add(Transition);
				}
			}

			OnComplete.ExecuteIfBound(true, Transitions, FExtendedAtlassianError());
		}));
}

void FExtendedAtlassianJira::TransitionIssue(const FString& IssueKey, const FString& TransitionId, FExtendedAtlassianActionDelegate OnComplete)
{
	const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient();
	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();

	if (!Client.IsValid() || !Settings)
	{
		FExtendedAtlassianError Error;
		Error.Code = TEXT("NotReady");
		Error.Message = TEXT("The Atlassian transport module is not available.");
		OnComplete.ExecuteIfBound(false, Error);
		return;
	}

	const FString Url = FString::Printf(TEXT("%s/issue/%s/transitions"), *Settings->GetJiraApiBaseUrl(), *IssueKey);
	const FString Body = FString::Printf(TEXT("{\"transition\":{\"id\":\"%s\"}}"), *TransitionId);

	Client->PostJson(Url, Body, FExtendedAtlassianResponseDelegate::CreateLambda(
		[OnComplete](const FExtendedAtlassianResponse& Response)
		{
			OnComplete.ExecuteIfBound(Response.IsSuccess(), Response.Error);
		}));
}

void FExtendedAtlassianJira::GetComments(const FString& IssueKey, FExtendedAtlassianCommentsDelegate OnComplete)
{
	const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient();
	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();

	if (!Client.IsValid() || !Settings)
	{
		FExtendedAtlassianError Error;
		Error.Code = TEXT("NotReady");
		Error.Message = TEXT("The Atlassian transport module is not available.");
		OnComplete.ExecuteIfBound(false, TArray<FExtendedAtlassianComment>(), Error);
		return;
	}

	const FString Url = FString::Printf(TEXT("%s/issue/%s/comment?maxResults=50&orderBy=created"),
		*Settings->GetJiraApiBaseUrl(), *IssueKey);

	Client->Get(Url, FExtendedAtlassianResponseDelegate::CreateLambda(
		[OnComplete](const FExtendedAtlassianResponse& Response)
		{
			if (!Response.IsSuccess() || !Response.Object.IsValid())
			{
				OnComplete.ExecuteIfBound(false, TArray<FExtendedAtlassianComment>(), Response.Error);
				return;
			}

			TArray<FExtendedAtlassianComment> Comments;

			const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
			if (Response.Object->TryGetArrayField(TEXT("comments"), Array))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Array)
				{
					const TSharedPtr<FJsonObject>* Object = nullptr;
					if (!Value->TryGetObject(Object) || !Object->IsValid())
					{
						continue;
					}

					FExtendedAtlassianComment Comment;
					(*Object)->TryGetStringField(TEXT("id"), Comment.Id);
					Comment.AuthorDisplayName =
						ExtendedAtlassianJiraPrivate::GetNestedString(*Object, TEXT("author"), TEXT("displayName"));

					const TSharedPtr<FJsonObject>* Body = nullptr;
					if ((*Object)->TryGetObjectField(TEXT("body"), Body) && Body->IsValid())
					{
						Comment.Body = FExtendedAtlassianAdf::ToPlainText(*Body);
					}

					FString CreatedRaw;
					if ((*Object)->TryGetStringField(TEXT("created"), CreatedRaw))
					{
						ParseJiraDateTime(CreatedRaw, Comment.Created);
					}

					Comments.Add(Comment);
				}
			}

			OnComplete.ExecuteIfBound(true, Comments, FExtendedAtlassianError());
		}));
}

void FExtendedAtlassianJira::AddComment(const FString& IssueKey, const FString& CommentText, FExtendedAtlassianActionDelegate OnComplete)
{
	const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient();
	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();

	if (!Client.IsValid() || !Settings)
	{
		FExtendedAtlassianError Error;
		Error.Code = TEXT("NotReady");
		Error.Message = TEXT("The Atlassian transport module is not available.");
		OnComplete.ExecuteIfBound(false, Error);
		return;
	}

	const FString Url = FString::Printf(TEXT("%s/issue/%s/comment"), *Settings->GetJiraApiBaseUrl(), *IssueKey);

	FString Body;
	{
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);

		Writer->WriteObjectStart();
		Writer->WriteIdentifierPrefix(TEXT("body"));
		FJsonSerializer::Serialize(FExtendedAtlassianAdf::MakeDoc(CommentText).ToSharedRef(), Writer, false);
		Writer->WriteObjectEnd();
		Writer->Close();
	}

	Client->PostJson(Url, Body, FExtendedAtlassianResponseDelegate::CreateLambda(
		[OnComplete](const FExtendedAtlassianResponse& Response)
		{
			OnComplete.ExecuteIfBound(Response.IsSuccess(), Response.Error);
		}));
}

void FExtendedAtlassianJira::CreateIssue(const FExtendedAtlassianNewIssue& NewIssue, FExtendedAtlassianCreateIssueDelegate OnComplete)
{
	const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient();
	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();

	if (!Client.IsValid() || !Settings)
	{
		FExtendedAtlassianError Error;
		Error.Code = TEXT("NotReady");
		Error.Message = TEXT("The Atlassian transport module is not available.");
		OnComplete.ExecuteIfBound(false, FString(), Error);
		return;
	}

	if (NewIssue.ProjectKey.IsEmpty() || NewIssue.IssueTypeName.IsEmpty())
	{
		FExtendedAtlassianError Error;
		Error.Code = TEXT("NotConfigured");
		Error.Message = TEXT("Set a Jira project key and issue type in Project Settings > Plugins > Extended Atlassian.");
		OnComplete.ExecuteIfBound(false, FString(), Error);
		return;
	}

	FString Body;
	{
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);

		Writer->WriteObjectStart();
		Writer->WriteObjectStart(TEXT("fields"));

		Writer->WriteObjectStart(TEXT("project"));
		Writer->WriteValue(TEXT("key"), NewIssue.ProjectKey);
		Writer->WriteObjectEnd();

		Writer->WriteObjectStart(TEXT("issuetype"));
		Writer->WriteValue(TEXT("name"), NewIssue.IssueTypeName);
		Writer->WriteObjectEnd();

		Writer->WriteValue(TEXT("summary"), NewIssue.Summary);

		Writer->WriteIdentifierPrefix(TEXT("description"));
		FJsonSerializer::Serialize(
			FExtendedAtlassianAdf::MakeDocWithCodeBlock(NewIssue.Description, NewIssue.ContextBlock).ToSharedRef(),
			Writer,
			false);

		// Priority is not configured on every project, and sending it where it does not exist fails
		// the whole create. Only include it when the user actually picked one.
		if (!NewIssue.PriorityName.IsEmpty())
		{
			Writer->WriteObjectStart(TEXT("priority"));
			Writer->WriteValue(TEXT("name"), NewIssue.PriorityName);
			Writer->WriteObjectEnd();
		}

		if (NewIssue.Labels.Num() > 0)
		{
			Writer->WriteArrayStart(TEXT("labels"));
			for (const FString& Label : NewIssue.Labels)
			{
				Writer->WriteValue(Label);
			}
			Writer->WriteArrayEnd();
		}

		Writer->WriteObjectEnd();
		Writer->WriteObjectEnd();
		Writer->Close();
	}

	Client->PostJson(Settings->GetJiraApiBaseUrl() + TEXT("/issue"), Body,
		FExtendedAtlassianResponseDelegate::CreateLambda(
			[OnComplete](const FExtendedAtlassianResponse& Response)
			{
				if (!Response.IsSuccess())
				{
					OnComplete.ExecuteIfBound(false, FString(), Response.Error);
					return;
				}

				FString IssueKey;
				if (Response.Object.IsValid())
				{
					Response.Object->TryGetStringField(TEXT("key"), IssueKey);
				}

				if (IssueKey.IsEmpty())
				{
					FExtendedAtlassianError Error;
					Error.Code = TEXT("UnexpectedResponse");
					Error.HttpStatus = Response.HttpStatus;
					Error.Message = TEXT("Jira accepted the issue but did not return its key.");
					OnComplete.ExecuteIfBound(false, FString(), Error);
					return;
				}

				OnComplete.ExecuteIfBound(true, IssueKey, FExtendedAtlassianError());
			}));
}

void FExtendedAtlassianJira::AddAttachment(
	const FString& IssueKey,
	const FString& FileName,
	const FString& ContentType,
	const TArray<uint8>& Data,
	FExtendedAtlassianActionDelegate OnComplete)
{
	const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient();
	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();

	if (!Client.IsValid() || !Settings)
	{
		FExtendedAtlassianError Error;
		Error.Code = TEXT("NotReady");
		Error.Message = TEXT("The Atlassian transport module is not available.");
		OnComplete.ExecuteIfBound(false, Error);
		return;
	}

	if (Data.Num() == 0)
	{
		FExtendedAtlassianError Error;
		Error.Code = TEXT("EmptyAttachment");
		Error.Message = TEXT("Refusing to upload an empty attachment.");
		OnComplete.ExecuteIfBound(false, Error);
		return;
	}

	FExtendedAtlassianMultipartFile File;
	File.FileName = FileName;
	File.ContentType = ContentType;
	File.Data = Data;

	TArray<FExtendedAtlassianMultipartFile> Files;
	Files.Add(MoveTemp(File));

	const FString Boundary = FExtendedAtlassianMultipart::MakeBoundary();
	TArray<uint8> Body = FExtendedAtlassianMultipart::BuildBody(Files, Boundary);

	TMap<FString, FString> Headers;
	// Jira rejects attachment uploads from non-browser clients without this XSRF opt-out.
	Headers.Add(TEXT("X-Atlassian-Token"), TEXT("no-check"));

	const FString Url = FString::Printf(TEXT("%s/issue/%s/attachments"), *Settings->GetJiraApiBaseUrl(), *IssueKey);

	Client->RequestRaw(TEXT("POST"), Url, MoveTemp(Body),
		FExtendedAtlassianMultipart::MakeContentTypeHeader(Boundary), Headers,
		FExtendedAtlassianResponseDelegate::CreateLambda(
			[OnComplete](const FExtendedAtlassianResponse& Response)
			{
				OnComplete.ExecuteIfBound(Response.IsSuccess(), Response.Error);
			}));
}

void FExtendedAtlassianJira::GetIssueTypes(const FString& ProjectKey, FExtendedAtlassianIssueTypesDelegate OnComplete)
{
	const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient();
	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();

	if (!Client.IsValid() || !Settings || ProjectKey.IsEmpty())
	{
		FExtendedAtlassianError Error;
		Error.Code = TEXT("NotReady");
		Error.Message = TEXT("No Jira project key is configured.");
		OnComplete.ExecuteIfBound(false, TArray<FExtendedAtlassianIssueType>(), Error);
		return;
	}

	const FString Url = FString::Printf(TEXT("%s/issue/createmeta/%s/issuetypes"), *Settings->GetJiraApiBaseUrl(), *ProjectKey);

	Client->Get(Url, FExtendedAtlassianResponseDelegate::CreateLambda(
		[OnComplete](const FExtendedAtlassianResponse& Response)
		{
			if (!Response.IsSuccess() || !Response.Object.IsValid())
			{
				OnComplete.ExecuteIfBound(false, TArray<FExtendedAtlassianIssueType>(), Response.Error);
				return;
			}

			// The field has been named both "issueTypes" and "values" across API revisions.
			const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
			if (!Response.Object->TryGetArrayField(TEXT("issueTypes"), Array))
			{
				Response.Object->TryGetArrayField(TEXT("values"), Array);
			}

			TArray<FExtendedAtlassianIssueType> IssueTypes;
			if (Array)
			{
				for (const TSharedPtr<FJsonValue>& Value : *Array)
				{
					const TSharedPtr<FJsonObject>* Object = nullptr;
					if (!Value->TryGetObject(Object) || !Object->IsValid())
					{
						continue;
					}

					FExtendedAtlassianIssueType IssueType;
					(*Object)->TryGetStringField(TEXT("id"), IssueType.Id);
					(*Object)->TryGetStringField(TEXT("name"), IssueType.Name);
					(*Object)->TryGetBoolField(TEXT("subtask"), IssueType.bSubtask);

					// A subtask cannot be created standalone, so offering one would only produce a failure.
					if (!IssueType.bSubtask && !IssueType.Name.IsEmpty())
					{
						IssueTypes.Add(IssueType);
					}
				}
			}

			OnComplete.ExecuteIfBound(true, IssueTypes, FExtendedAtlassianError());
		}));
}

void FExtendedAtlassianJira::GetProjects(FExtendedAtlassianProjectsDelegate OnComplete)
{
	const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient();
	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();

	if (!Client.IsValid() || !Settings)
	{
		FExtendedAtlassianError Error;
		Error.Code = TEXT("NotReady");
		Error.Message = TEXT("The Atlassian transport module is not available.");
		OnComplete.ExecuteIfBound(false, TArray<FExtendedAtlassianProject>(), Error);
		return;
	}

	Client->Get(Settings->GetJiraApiBaseUrl() + TEXT("/project/search?maxResults=100&orderBy=key"),
		FExtendedAtlassianResponseDelegate::CreateLambda(
			[OnComplete](const FExtendedAtlassianResponse& Response)
			{
				if (!Response.IsSuccess() || !Response.Object.IsValid())
				{
					OnComplete.ExecuteIfBound(false, TArray<FExtendedAtlassianProject>(), Response.Error);
					return;
				}

				TArray<FExtendedAtlassianProject> Projects;

				const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
				if (Response.Object->TryGetArrayField(TEXT("values"), Values))
				{
					for (const TSharedPtr<FJsonValue>& Value : *Values)
					{
						const TSharedPtr<FJsonObject>* Object = nullptr;
						if (!Value->TryGetObject(Object) || !Object->IsValid())
						{
							continue;
						}

						FExtendedAtlassianProject Project;
						(*Object)->TryGetStringField(TEXT("id"), Project.Id);
						(*Object)->TryGetStringField(TEXT("key"), Project.Key);
						(*Object)->TryGetStringField(TEXT("name"), Project.Name);

						if (!Project.Key.IsEmpty())
						{
							Projects.Add(Project);
						}
					}
				}

				// One page is plenty for a settings dropdown, but say so rather than silently truncating.
				bool bIsLast = true;
				Response.Object->TryGetBoolField(TEXT("isLast"), bIsLast);
				if (!bIsLast)
				{
					UE_LOG(LogExtendedAtlassian, Warning,
						TEXT("More than 100 Jira projects are visible; the dropdown lists only the first 100. Type the key manually if yours is missing."));
				}

				OnComplete.ExecuteIfBound(true, Projects, FExtendedAtlassianError());
			}));
}

void FExtendedAtlassianJira::GetPriorities(FExtendedAtlassianPrioritiesDelegate OnComplete)
{
	const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient();
	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();

	if (!Client.IsValid() || !Settings)
	{
		FExtendedAtlassianError Error;
		Error.Code = TEXT("NotReady");
		Error.Message = TEXT("The Atlassian transport module is not available.");
		OnComplete.ExecuteIfBound(false, TArray<FExtendedAtlassianPriority>(), Error);
		return;
	}

	Client->Get(Settings->GetJiraApiBaseUrl() + TEXT("/priority"),
		FExtendedAtlassianResponseDelegate::CreateLambda(
			[OnComplete](const FExtendedAtlassianResponse& Response)
			{
				if (!Response.IsSuccess())
				{
					OnComplete.ExecuteIfBound(false, TArray<FExtendedAtlassianPriority>(), Response.Error);
					return;
				}

				// /priority returns a bare array; the paginated variant wraps it in "values".
				TArray<TSharedPtr<FJsonValue>> Values = Response.Array;
				if (!Response.bIsArray && Response.Object.IsValid())
				{
					const TArray<TSharedPtr<FJsonValue>>* Wrapped = nullptr;
					if (Response.Object->TryGetArrayField(TEXT("values"), Wrapped))
					{
						Values = *Wrapped;
					}
				}

				TArray<FExtendedAtlassianPriority> Priorities;
				for (const TSharedPtr<FJsonValue>& Value : Values)
				{
					const TSharedPtr<FJsonObject>* Object = nullptr;
					if (!Value->TryGetObject(Object) || !Object->IsValid())
					{
						continue;
					}

					FExtendedAtlassianPriority Priority;
					(*Object)->TryGetStringField(TEXT("id"), Priority.Id);
					(*Object)->TryGetStringField(TEXT("name"), Priority.Name);

					if (!Priority.Name.IsEmpty())
					{
						Priorities.Add(Priority);
					}
				}

				OnComplete.ExecuteIfBound(true, Priorities, FExtendedAtlassianError());
			}));
}
