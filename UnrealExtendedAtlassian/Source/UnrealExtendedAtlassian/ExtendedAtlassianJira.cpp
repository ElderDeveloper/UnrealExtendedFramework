// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianJira.h"

#include "ExtendedAtlassianAdf.h"
#include "ExtendedAtlassianClient.h"
#include "ExtendedAtlassianLog.h"
#include "ExtendedAtlassianModelUtils.h"
#include "ExtendedAtlassianMultipart.h"
#include "ExtendedAtlassianSettings.h"
#include "UnrealExtendedAtlassian.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace ExtendedAtlassianJiraPrivate
{
	FString ArchiveErrors(const TSharedPtr<FJsonObject>& Response)
	{
		if (!Response.IsValid())
		{
			return FString();
		}
		const TSharedPtr<FJsonObject>* Errors = nullptr;
		if (!Response->TryGetObjectField(TEXT("errors"), Errors)
			|| !Errors->IsValid())
		{
			return FString();
		}
		TArray<FString> Messages;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Errors)->Values)
		{
			const TSharedPtr<FJsonObject>* Detail = nullptr;
			FString Message;
			if (Pair.Value.IsValid()
				&& Pair.Value->TryGetObject(Detail)
				&& Detail->IsValid()
				&& (*Detail)->TryGetStringField(TEXT("message"), Message)
				&& !Message.IsEmpty())
			{
				Messages.Add(MoveTemp(Message));
			}
		}
		return FString::Join(Messages, TEXT("; "));
	}

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
		TEXT("parent"),
		TEXT("comment"),
	};

	FString GetFieldsCsv()
	{
		TArray<FString> Names;
		for (const TCHAR* Name : IssueFieldNames)
		{
			Names.Add(Name);
		}
		if (const UExtendedAtlassianSettings* Settings =
			UExtendedAtlassianSettings::Get())
		{
			if (!Settings->DiscoveredEstimateFieldId.IsEmpty())
			{
				Names.AddUnique(Settings->DiscoveredEstimateFieldId);
			}
			if (!Settings->DiscoveredRankFieldId.IsEmpty())
			{
				Names.AddUnique(Settings->DiscoveredRankFieldId);
			}
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
			if (Settings)
			{
				if (!Settings->DiscoveredEstimateFieldId.IsEmpty())
				{
					Writer->WriteValue(Settings->DiscoveredEstimateFieldId);
				}
				if (!Settings->DiscoveredRankFieldId.IsEmpty())
				{
					Writer->WriteValue(Settings->DiscoveredRankFieldId);
				}
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

	FString MakeAdfBody(const FString& Text)
	{
		FString Body;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
		Writer->WriteObjectStart();
		Writer->WriteIdentifierPrefix(TEXT("body"));
		FJsonSerializer::Serialize(FExtendedAtlassianAdf::MakeDoc(Text).ToSharedRef(), Writer, false);
		Writer->WriteObjectEnd();
		Writer->Close();
		return Body;
	}

	FExtendedAtlassianUser ParseUser(const TSharedPtr<FJsonObject>& Object)
	{
		FExtendedAtlassianUser User;
		if (!Object.IsValid())
		{
			return User;
		}
		Object->TryGetStringField(TEXT("accountId"), User.AccountId);
		Object->TryGetStringField(TEXT("displayName"), User.DisplayName);
		Object->TryGetStringField(TEXT("emailAddress"), User.EmailAddress);
		const TSharedPtr<FJsonObject>* Avatars = nullptr;
		if (Object->TryGetObjectField(TEXT("avatarUrls"), Avatars) && Avatars->IsValid())
		{
			(*Avatars)->TryGetStringField(TEXT("48x48"), User.AvatarUrl);
		}
		return User;
	}

	struct FChangelogState
	{
		FString IssueKey;
		int32 StartAt = 0;
		TArray<FExtendedAtlassianActivity> Activities;
		FExtendedAtlassianActivitiesDelegate OnComplete;
	};

	FString RelativeAge(const FDateTime& Created)
	{
		// One implementation, shared with the Confluence provider.
		return ExtendedAtlassianModelUtils::RelativeAge(Created);
	}

	FString ChangelogDetail(
		const FString& Actor,
		const FString& Field,
		const FString& From,
		const FString& To)
	{
		const FString Lower = Field.ToLower();
		if (Lower == TEXT("status"))
		{
			return FString::Printf(
				TEXT("%s moved this from %s to %s."),
				*Actor,
				*From,
				*To);
		}
		if (Lower == TEXT("assignee"))
		{
			return To.IsEmpty()
				? FString::Printf(TEXT("%s unassigned this issue."), *Actor)
				: FString::Printf(
					TEXT("%s assigned this to %s."),
					*Actor,
					*To);
		}
		if (Lower == TEXT("priority"))
		{
			return FString::Printf(
				TEXT("%s changed priority from %s to %s."),
				*Actor,
				*From,
				*To);
		}
		if (Lower.Contains(TEXT("story point"))
			|| Lower.Contains(TEXT("estimate")))
		{
			return FString::Printf(
				TEXT("%s estimated this at %s points."),
				*Actor,
				*To);
		}
		if (Lower == TEXT("parent") || Lower == TEXT("epic link"))
		{
			return To.IsEmpty()
				? FString::Printf(TEXT("%s removed this from its epic."), *Actor)
				: FString::Printf(
					TEXT("%s moved this to %s."),
					*Actor,
					*To);
		}
		if (Lower == TEXT("summary"))
		{
			return FString::Printf(TEXT("%s edited the summary."), *Actor);
		}
		if (Lower == TEXT("description"))
		{
			return FString::Printf(TEXT("%s updated the description."), *Actor);
		}
		return FString::Printf(
			TEXT("%s changed %s from %s to %s."),
			*Actor,
			*Field,
			*From,
			*To);
	}

	void FetchChangelogPage(const TSharedRef<FChangelogState>& State)
	{
		const TSharedPtr<FExtendedAtlassianClient> Client =
			FUnrealExtendedAtlassianModule::GetClient();
		const UExtendedAtlassianSettings* Settings =
			UExtendedAtlassianSettings::Get();
		if (!Client.IsValid() || !Settings)
		{
			FExtendedAtlassianError Error;
			Error.Code = TEXT("NotReady");
			Error.Message =
				TEXT("The Atlassian transport module is not available.");
			State->OnComplete.ExecuteIfBound(
				false,
				State->Activities,
				Error);
			return;
		}
		const FString Url = FString::Printf(
			TEXT("%s/issue/%s/changelog?startAt=%d&maxResults=100"),
			*Settings->GetJiraApiBaseUrl(),
			*FGenericPlatformHttp::UrlEncode(State->IssueKey),
			State->StartAt);
		Client->Get(
			Url,
			FExtendedAtlassianResponseDelegate::CreateLambda(
				[State](const FExtendedAtlassianResponse& Response)
				{
					if (!Response.IsSuccess() || !Response.Object.IsValid())
					{
						State->OnComplete.ExecuteIfBound(
							false,
							State->Activities,
							Response.Error);
						return;
					}
					const TArray<TSharedPtr<FJsonValue>>* Histories = nullptr;
					if (Response.Object->TryGetArrayField(TEXT("values"), Histories))
					{
						for (const TSharedPtr<FJsonValue>& HistoryValue :
							*Histories)
						{
							const TSharedPtr<FJsonObject> History =
								HistoryValue.IsValid()
									? HistoryValue->AsObject()
									: nullptr;
							if (!History.IsValid())
							{
								continue;
							}
							FString HistoryId;
							History->TryGetStringField(TEXT("id"), HistoryId);
							FString CreatedRaw;
							FDateTime Created = FDateTime::MinValue();
							if (History->TryGetStringField(
								TEXT("created"),
								CreatedRaw))
							{
								FExtendedAtlassianJira::ParseJiraDateTime(
									CreatedRaw,
									Created);
							}
							FString AccountId;
							FString DisplayName;
							const TSharedPtr<FJsonObject>* Author = nullptr;
							if (History->TryGetObjectField(
								TEXT("author"),
								Author)
								&& Author->IsValid())
							{
								(*Author)->TryGetStringField(
									TEXT("accountId"),
									AccountId);
								(*Author)->TryGetStringField(
									TEXT("displayName"),
									DisplayName);
							}
							if (DisplayName.IsEmpty())
							{
								DisplayName = TEXT("Someone");
							}
							const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
							if (!History->TryGetArrayField(TEXT("items"), Items))
							{
								continue;
							}
							for (int32 ItemIndex = 0;
								ItemIndex < Items->Num();
								++ItemIndex)
							{
								const TSharedPtr<FJsonObject> Item =
									(*Items)[ItemIndex].IsValid()
										? (*Items)[ItemIndex]->AsObject()
										: nullptr;
								if (!Item.IsValid())
								{
									continue;
								}
								FString Field;
								FString From;
								FString To;
								Item->TryGetStringField(TEXT("field"), Field);
								Item->TryGetStringField(TEXT("fromString"), From);
								Item->TryGetStringField(TEXT("toString"), To);
								FExtendedAtlassianActivity Activity;
								Activity.Id = FString::Printf(
									TEXT("jira:%s:%d"),
									*HistoryId,
									ItemIndex);
								Activity.IssueKey = State->IssueKey;
								Activity.ActorAccountId = AccountId;
								Activity.ActorDisplayName = DisplayName;
								Activity.Verb = Field;
								Activity.Detail = ChangelogDetail(
									DisplayName,
									Field,
									From,
									To);
								Activity.Created = Created;
								Activity.RelativeTime = RelativeAge(Created);
								State->Activities.Add(MoveTemp(Activity));
							}
						}
					}
					int32 Total = State->Activities.Num();
					int32 MaxResults = 100;
					Response.Object->TryGetNumberField(TEXT("total"), Total);
					Response.Object->TryGetNumberField(
						TEXT("maxResults"),
						MaxResults);
					State->StartAt += Histories ? Histories->Num() : 0;
					if (Histories
						&& !Histories->IsEmpty()
						&& State->StartAt < Total)
					{
						FetchChangelogPage(State);
						return;
					}
					State->Activities.StableSort(
						[](const FExtendedAtlassianActivity& Left,
							const FExtendedAtlassianActivity& Right)
						{
							return Left.Created > Right.Created;
						});
					State->OnComplete.ExecuteIfBound(
						true,
						State->Activities,
						FExtendedAtlassianError());
				}));
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
		(*Status)->TryGetStringField(TEXT("id"), Issue.StatusId);
		(*Status)->TryGetStringField(TEXT("name"), Issue.StatusName);
		Issue.StatusCategoryKey = GetNestedString(*Status, TEXT("statusCategory"), TEXT("key"));
	}

	const TSharedPtr<FJsonObject>* IssueType = nullptr;
	if (Fields->TryGetObjectField(TEXT("issuetype"), IssueType) && IssueType->IsValid())
	{
		(*IssueType)->TryGetStringField(TEXT("id"), Issue.IssueTypeId);
		(*IssueType)->TryGetStringField(TEXT("name"), Issue.IssueTypeName);
		(*IssueType)->TryGetStringField(TEXT("iconUrl"), Issue.IssueTypeIconUrl);
	}

	const TSharedPtr<FJsonObject>* Priority = nullptr;
	if (Fields->TryGetObjectField(TEXT("priority"), Priority) && Priority->IsValid())
	{
		(*Priority)->TryGetStringField(TEXT("id"), Issue.PriorityId);
		(*Priority)->TryGetStringField(TEXT("name"), Issue.PriorityName);
	}
	Issue.ReporterDisplayName = GetNestedString(Fields, TEXT("reporter"), TEXT("displayName"));

	const TSharedPtr<FJsonObject>* Assignee = nullptr;
	if (Fields->TryGetObjectField(TEXT("assignee"), Assignee) && Assignee->IsValid())
	{
		(*Assignee)->TryGetStringField(TEXT("displayName"), Issue.AssigneeDisplayName);
		(*Assignee)->TryGetStringField(TEXT("accountId"), Issue.AssigneeAccountId);
		const TSharedPtr<FJsonObject>* Avatars = nullptr;
		if ((*Assignee)->TryGetObjectField(TEXT("avatarUrls"), Avatars) && Avatars->IsValid())
		{
			(*Avatars)->TryGetStringField(TEXT("48x48"), Issue.AssigneeAvatarUrl);
		}
	}

	const TSharedPtr<FJsonObject>* Parent = nullptr;
	if (Fields->TryGetObjectField(TEXT("parent"), Parent) && Parent->IsValid())
	{
		(*Parent)->TryGetStringField(TEXT("id"), Issue.ParentId);
		(*Parent)->TryGetStringField(TEXT("key"), Issue.ParentKey);
		const TSharedPtr<FJsonObject>* ParentFields = nullptr;
		if ((*Parent)->TryGetObjectField(TEXT("fields"), ParentFields) && ParentFields->IsValid())
		{
			(*ParentFields)->TryGetStringField(TEXT("summary"), Issue.ParentSummary);
			const TSharedPtr<FJsonObject>* ParentType = nullptr;
			FString ParentTypeName;
			if ((*ParentFields)->TryGetObjectField(
					TEXT("issuetype"),
					ParentType)
				&& ParentType->IsValid())
			{
				(*ParentType)->TryGetStringField(
					TEXT("name"),
					ParentTypeName);
			}
			if (ParentTypeName.Equals(
				TEXT("Epic"),
				ESearchCase::IgnoreCase))
			{
				Issue.EpicId = Issue.ParentId;
				Issue.EpicName = Issue.ParentSummary;
			}
		}
	}

	const TSharedPtr<FJsonObject>* CommentContainer = nullptr;
	if (Fields->TryGetObjectField(TEXT("comment"), CommentContainer) && CommentContainer->IsValid())
	{
		(*CommentContainer)->TryGetNumberField(TEXT("total"), Issue.CommentCount);
	}

	// "created" and "updated" are already in IssueFieldNames, so every response has carried these
	// all along and nothing read them: the UPDATED column rendered an empty string on live data
	// while the fixture assigned RelativeUpdated directly, which is why no test caught it.
	FString Timestamp;
	if (Fields->TryGetStringField(TEXT("updated"), Timestamp)
		&& ParseJiraDateTime(Timestamp, Issue.Updated))
	{
		Issue.RelativeUpdated = RelativeAge(Issue.Updated);
	}
	if (Fields->TryGetStringField(TEXT("created"), Timestamp)
		&& ParseJiraDateTime(Timestamp, Issue.Created))
	{
		Issue.RelativeCreated = RelativeAge(Issue.Created);
	}

	if (const UExtendedAtlassianSettings* Settings =
		UExtendedAtlassianSettings::Get())
	{
		if (!Settings->DiscoveredEstimateFieldId.IsEmpty())
		{
			double NumericEstimate = 0.0;
			if (Fields->TryGetNumberField(
				Settings->DiscoveredEstimateFieldId,
				NumericEstimate))
			{
				Issue.Estimate = NumericEstimate;
			}
			else
			{
				FString Estimate;
				if (Fields->TryGetStringField(
					Settings->DiscoveredEstimateFieldId,
					Estimate))
				{
					Issue.Estimate = FCString::Atod(*Estimate);
				}
			}
		}
		if (!Settings->DiscoveredRankFieldId.IsEmpty())
		{
			Fields->TryGetStringField(
				Settings->DiscoveredRankFieldId,
				Issue.Rank);
		}
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
		Issue.DescriptionBlocks = FExtendedAtlassianAdf::ToBlocks(*Description);
	}
	else
	{
		// Older Jira deployments and test doubles may still expose a string. Keep it usable; the
		// shared reader will adapt this fallback through the Markdown/plain-text block parser.
		Fields->TryGetStringField(TEXT("description"), Issue.Description);
	}

	return Issue;
}

FExtendedAtlassianUser FExtendedAtlassianJira::ParseUser(
	const TSharedPtr<FJsonObject>& UserJson)
{
	return ExtendedAtlassianJiraPrivate::ParseUser(UserJson);
}

bool FExtendedAtlassianJira::ParseChangelogPage(
	const TSharedPtr<FJsonObject>& Object,
	const FString& IssueKey,
	TArray<FExtendedAtlassianActivity>& OutActivities,
	int32& OutHistoryCount,
	int32& OutTotal,
	FExtendedAtlassianError& OutError)
{
	OutActivities.Reset();
	OutHistoryCount = 0;
	OutTotal = 0;
	OutError = FExtendedAtlassianError();
	if (!Object.IsValid())
	{
		OutError.Code = TEXT("UnexpectedResponse");
		OutError.Message = TEXT("Jira returned an invalid changelog page.");
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Histories = nullptr;
	if (!Object->TryGetArrayField(TEXT("values"), Histories))
	{
		Object->TryGetNumberField(TEXT("total"), OutTotal);
		return true;
	}
	OutHistoryCount = Histories->Num();
	for (const TSharedPtr<FJsonValue>& HistoryValue : *Histories)
	{
		const TSharedPtr<FJsonObject> History =
			HistoryValue.IsValid() ? HistoryValue->AsObject() : nullptr;
		if (!History.IsValid())
		{
			continue;
		}
		FString HistoryId;
		History->TryGetStringField(TEXT("id"), HistoryId);
		FString CreatedRaw;
		FDateTime Created = FDateTime::MinValue();
		if (History->TryGetStringField(TEXT("created"), CreatedRaw))
		{
			ParseJiraDateTime(CreatedRaw, Created);
		}
		FString AccountId;
		FString DisplayName;
		const TSharedPtr<FJsonObject>* Author = nullptr;
		if (History->TryGetObjectField(TEXT("author"), Author)
			&& Author->IsValid())
		{
			(*Author)->TryGetStringField(TEXT("accountId"), AccountId);
			(*Author)->TryGetStringField(TEXT("displayName"), DisplayName);
		}
		if (DisplayName.IsEmpty())
		{
			DisplayName = TEXT("Someone");
		}
		const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
		if (!History->TryGetArrayField(TEXT("items"), Items))
		{
			continue;
		}
		for (int32 ItemIndex = 0; ItemIndex < Items->Num(); ++ItemIndex)
		{
			const TSharedPtr<FJsonObject> Item =
				(*Items)[ItemIndex].IsValid()
					? (*Items)[ItemIndex]->AsObject()
					: nullptr;
			if (!Item.IsValid())
			{
				continue;
			}
			FString Field;
			FString From;
			FString To;
			Item->TryGetStringField(TEXT("field"), Field);
			Item->TryGetStringField(TEXT("fromString"), From);
			Item->TryGetStringField(TEXT("toString"), To);
			FExtendedAtlassianActivity Activity;
			Activity.Id = FString::Printf(
				TEXT("jira:%s:%d"),
				*HistoryId,
				ItemIndex);
			Activity.IssueKey = IssueKey;
			Activity.ActorAccountId = AccountId;
			Activity.ActorDisplayName = DisplayName;
			Activity.Verb = Field;
			Activity.Detail =
				ExtendedAtlassianJiraPrivate::ChangelogDetail(
					DisplayName,
					Field,
					From,
					To);
			Activity.Created = Created;
			Activity.RelativeTime =
				ExtendedAtlassianJiraPrivate::RelativeAge(Created);
			OutActivities.Add(MoveTemp(Activity));
		}
	}
	Object->TryGetNumberField(TEXT("total"), OutTotal);
	if (OutTotal == 0)
	{
		OutTotal = OutHistoryCount;
	}
	return true;
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

FString FExtendedAtlassianJira::BuildIssueUpdateBody(
	const FExtendedAtlassianIssueUpdate& Update)
{
	FString Body;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
	Writer->WriteObjectStart();
	Writer->WriteObjectStart(TEXT("fields"));
	if (Update.Summary.IsSet())
	{
		Writer->WriteValue(TEXT("summary"), Update.Summary.GetValue());
	}
	if (Update.Description.IsSet())
	{
		Writer->WriteIdentifierPrefix(TEXT("description"));
		FJsonSerializer::Serialize(
			FExtendedAtlassianAdf::MakeDoc(Update.Description.GetValue()).ToSharedRef(),
			Writer,
			false);
	}
	if (Update.IssueTypeId.IsSet() || Update.IssueTypeName.IsSet())
	{
		Writer->WriteObjectStart(TEXT("issuetype"));
		if (Update.IssueTypeId.IsSet())
		{
			Writer->WriteValue(TEXT("id"), Update.IssueTypeId.GetValue());
		}
		else
		{
			Writer->WriteValue(TEXT("name"), Update.IssueTypeName.GetValue());
		}
		Writer->WriteObjectEnd();
	}
	if (Update.PriorityId.IsSet() || Update.PriorityName.IsSet())
	{
		Writer->WriteObjectStart(TEXT("priority"));
		if (Update.PriorityId.IsSet())
		{
			Writer->WriteValue(TEXT("id"), Update.PriorityId.GetValue());
		}
		else
		{
			Writer->WriteValue(TEXT("name"), Update.PriorityName.GetValue());
		}
		Writer->WriteObjectEnd();
	}
	if (Update.AssigneeAccountId.IsSet())
	{
		Writer->WriteObjectStart(TEXT("assignee"));
		Writer->WriteValue(TEXT("accountId"), Update.AssigneeAccountId.GetValue());
		Writer->WriteObjectEnd();
	}
	if (!Update.bClearParent
		&& (Update.ParentId.IsSet() || Update.ParentKey.IsSet()))
	{
		Writer->WriteObjectStart(TEXT("parent"));
		if (Update.ParentId.IsSet())
		{
			Writer->WriteValue(TEXT("id"), Update.ParentId.GetValue());
		}
		else
		{
			Writer->WriteValue(TEXT("key"), Update.ParentKey.GetValue());
		}
		Writer->WriteObjectEnd();
	}
	if (Update.Estimate.IsSet() && !Update.EstimateFieldId.IsEmpty())
	{
		Writer->WriteValue(Update.EstimateFieldId, Update.Estimate.GetValue());
	}
	Writer->WriteObjectEnd();
	if (Update.bClearParent)
	{
		Writer->WriteObjectStart(TEXT("update"));
		Writer->WriteArrayStart(TEXT("parent"));
		Writer->WriteObjectStart();
		Writer->WriteObjectStart(TEXT("set"));
		Writer->WriteValue(TEXT("none"), true);
		Writer->WriteObjectEnd();
		Writer->WriteObjectEnd();
		Writer->WriteArrayEnd();
		Writer->WriteObjectEnd();
	}
	Writer->WriteObjectEnd();
	Writer->Close();
	return Body;
}

void FExtendedAtlassianJira::UpdateIssue(
	const FString& IssueKey,
	const FExtendedAtlassianIssueUpdate& Update,
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

	const FString Body = BuildIssueUpdateBody(Update);

	const FString Url = FString::Printf(
		TEXT("%s/issue/%s"),
		*Settings->GetJiraApiBaseUrl(),
		*IssueKey);
	Client->PutJson(
		Url,
		Body,
		FExtendedAtlassianResponseDelegate::CreateLambda(
			[OnComplete](const FExtendedAtlassianResponse& Response)
			{
				OnComplete.ExecuteIfBound(Response.IsSuccess(), Response.Error);
			}));
}

void FExtendedAtlassianJira::DeleteIssue(
	const FString& IssueKey,
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
	Client->Delete(
		FString::Printf(TEXT("%s/issue/%s"), *Settings->GetJiraApiBaseUrl(), *IssueKey),
		FExtendedAtlassianResponseDelegate::CreateLambda(
			[OnComplete](const FExtendedAtlassianResponse& Response)
			{
				OnComplete.ExecuteIfBound(Response.IsSuccess(), Response.Error);
			}));
}

void FExtendedAtlassianJira::ArchiveIssue(
	const FString& IssueKey,
	FExtendedAtlassianActionDelegate OnComplete)
{
	const TSharedPtr<FExtendedAtlassianClient> Client =
		FUnrealExtendedAtlassianModule::GetClient();
	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();
	if (!Client.IsValid() || !Settings || IssueKey.TrimStartAndEnd().IsEmpty())
	{
		FExtendedAtlassianError Error;
		Error.Code = TEXT("NotReady");
		Error.Message = TEXT("An issue key and the Atlassian transport module are required to archive an issue.");
		OnComplete.ExecuteIfBound(false, Error);
		return;
	}

	FString Body;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
	Writer->WriteObjectStart();
	Writer->WriteArrayStart(TEXT("issueIdsOrKeys"));
	Writer->WriteValue(IssueKey);
	Writer->WriteArrayEnd();
	Writer->WriteObjectEnd();
	Writer->Close();

	Client->PutJson(
		Settings->GetJiraApiBaseUrl() + TEXT("/issue/archive"),
		Body,
		FExtendedAtlassianResponseDelegate::CreateLambda(
			[OnComplete](const FExtendedAtlassianResponse& Response)
			{
				if (!Response.IsSuccess())
				{
					OnComplete.ExecuteIfBound(false, Response.Error);
					return;
				}

				double NumberOfIssuesUpdated = 0.0;
				if (Response.Object.IsValid()
					&& Response.Object->TryGetNumberField(
						TEXT("numberOfIssuesUpdated"),
						NumberOfIssuesUpdated)
					&& NumberOfIssuesUpdated >= 1.0)
				{
					OnComplete.ExecuteIfBound(true, FExtendedAtlassianError());
					return;
				}

				FExtendedAtlassianError Error;
				Error.HttpStatus = Response.HttpStatus;
				Error.Code = TEXT("ArchiveRejected");
				Error.Message = ExtendedAtlassianJiraPrivate::ArchiveErrors(Response.Object);
				if (Error.Message.IsEmpty())
				{
					Error.Message = TEXT("Jira returned success but did not archive the requested issue.");
				}
				OnComplete.ExecuteIfBound(false, Error);
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
		[OnComplete, IssueKey](const FExtendedAtlassianResponse& Response)
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
						(*To)->TryGetStringField(TEXT("id"), Transition.ToStatusId);
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

void FExtendedAtlassianJira::TransitionIssueToStatus(
	const FString& IssueKey,
	const FString& StatusName,
	FExtendedAtlassianActionDelegate OnComplete)
{
	TransitionIssueToStatusIdOrName(
		IssueKey,
		FString(),
		StatusName,
		MoveTemp(OnComplete));
}

void FExtendedAtlassianJira::TransitionIssueToStatusIdOrName(
	const FString& IssueKey,
	const FString& StatusId,
	const FString& StatusName,
	FExtendedAtlassianActionDelegate OnComplete)
{
	GetIssue(
		IssueKey,
		FExtendedAtlassianIssueDelegate::CreateLambda(
			[IssueKey, StatusId, StatusName, OnComplete](
				bool bSuccess,
				const FExtendedAtlassianIssue& Issue,
				const FExtendedAtlassianError& Error)
			{
				if (!bSuccess)
				{
					OnComplete.ExecuteIfBound(false, Error);
					return;
				}
				if ((!StatusId.IsEmpty()
						&& Issue.StatusId.Equals(
							StatusId,
							ESearchCase::CaseSensitive))
					|| (!StatusName.IsEmpty()
						&& Issue.StatusName.Equals(
							StatusName,
							ESearchCase::IgnoreCase)))
				{
					OnComplete.ExecuteIfBound(true, FExtendedAtlassianError());
					return;
				}
				GetTransitions(
					IssueKey,
					FExtendedAtlassianTransitionsDelegate::CreateLambda(
						[IssueKey, StatusId, StatusName, OnComplete](
							bool bTransitionsSuccess,
							const TArray<FExtendedAtlassianTransition>& Transitions,
							const FExtendedAtlassianError& TransitionsError)
						{
							if (!bTransitionsSuccess)
							{
								OnComplete.ExecuteIfBound(false, TransitionsError);
								return;
							}
							const FExtendedAtlassianTransition* Match =
								Transitions.FindByPredicate(
									[&StatusId, &StatusName](
										const FExtendedAtlassianTransition& Transition)
									{
										return (!StatusId.IsEmpty()
												&& Transition.ToStatusId.Equals(
													StatusId,
													ESearchCase::CaseSensitive))
											|| (!StatusName.IsEmpty()
												&& (Transition.ToStatusName.Equals(
														StatusName,
														ESearchCase::IgnoreCase)
													|| Transition.Name.Equals(
														StatusName,
														ESearchCase::IgnoreCase)));
									});
							if (!Match)
							{
								FExtendedAtlassianError Missing;
								Missing.Code = TEXT("TransitionUnavailable");
								Missing.Message = FString::Printf(
									TEXT("No available Jira transition reaches status '%s'."),
									StatusName.IsEmpty() ? *StatusId : *StatusName);
								OnComplete.ExecuteIfBound(false, Missing);
								return;
							}
							TransitionIssue(IssueKey, Match->Id, OnComplete);
						}));
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
		[OnComplete, IssueKey](const FExtendedAtlassianResponse& Response)
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
					Comment.ContainerId = IssueKey;
					const TSharedPtr<FJsonObject>* Author = nullptr;
					if ((*Object)->TryGetObjectField(TEXT("author"), Author) && Author->IsValid())
					{
						(*Author)->TryGetStringField(TEXT("accountId"), Comment.AuthorAccountId);
						(*Author)->TryGetStringField(TEXT("displayName"), Comment.AuthorDisplayName);
						const TSharedPtr<FJsonObject>* Avatars = nullptr;
						if ((*Author)->TryGetObjectField(TEXT("avatarUrls"), Avatars) && Avatars->IsValid())
						{
							(*Avatars)->TryGetStringField(TEXT("48x48"), Comment.AuthorAvatarUrl);
						}
					}

					const TSharedPtr<FJsonObject>* Body = nullptr;
					if ((*Object)->TryGetObjectField(TEXT("body"), Body) && Body->IsValid())
					{
						Comment.Body = FExtendedAtlassianAdf::ToPlainText(*Body);
					}

					FString CreatedRaw;
					if ((*Object)->TryGetStringField(TEXT("created"), CreatedRaw))
					{
						ParseJiraDateTime(CreatedRaw, Comment.Created);
						Comment.RelativeTime =
							ExtendedAtlassianJiraPrivate::RelativeAge(
								Comment.Created);
					}
					FString UpdatedRaw;
					if ((*Object)->TryGetStringField(TEXT("updated"), UpdatedRaw))
					{
						ParseJiraDateTime(UpdatedRaw, Comment.Updated);
					}
					Comment.bCanEdit = true;
					Comment.bCanDelete = true;

					Comments.Add(Comment);
				}
			}

			OnComplete.ExecuteIfBound(true, Comments, FExtendedAtlassianError());
		}));
}

void FExtendedAtlassianJira::GetChangelog(
	const FString& IssueKey,
	FExtendedAtlassianActivitiesDelegate OnComplete)
{
	using namespace ExtendedAtlassianJiraPrivate;
	if (IssueKey.IsEmpty())
	{
		FExtendedAtlassianError Error;
		Error.Code = TEXT("InvalidIssueKey");
		Error.Message = TEXT("An issue key is required to read its changelog.");
		OnComplete.ExecuteIfBound(
			false,
			TArray<FExtendedAtlassianActivity>(),
			Error);
		return;
	}
	const TSharedRef<FChangelogState> State = MakeShared<FChangelogState>();
	State->IssueKey = IssueKey;
	State->OnComplete = MoveTemp(OnComplete);
	FetchChangelogPage(State);
}

void FExtendedAtlassianJira::AddComment(const FString& IssueKey, const FString& CommentText, FExtendedAtlassianActionDelegate OnComplete)
{
	AddCommentWithId(
		IssueKey,
		CommentText,
		FExtendedAtlassianCreateCommentDelegate::CreateLambda(
			[OnComplete](
				bool bSuccess,
				const FString&,
				const FExtendedAtlassianError& Error)
			{
				OnComplete.ExecuteIfBound(bSuccess, Error);
			}));
}

void FExtendedAtlassianJira::AddCommentWithId(
	const FString& IssueKey,
	const FString& CommentText,
	FExtendedAtlassianCreateCommentDelegate OnComplete)
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

	const FString Url = FString::Printf(TEXT("%s/issue/%s/comment"), *Settings->GetJiraApiBaseUrl(), *IssueKey);

	Client->PostJson(Url, ExtendedAtlassianJiraPrivate::MakeAdfBody(CommentText), FExtendedAtlassianResponseDelegate::CreateLambda(
		[OnComplete](const FExtendedAtlassianResponse& Response)
		{
			FString CommentId;
			if (Response.Object.IsValid())
			{
				Response.Object->TryGetStringField(TEXT("id"), CommentId);
			}
			if (Response.IsSuccess() && CommentId.IsEmpty())
			{
				FExtendedAtlassianError Error;
				Error.Code = TEXT("UnexpectedResponse");
				Error.HttpStatus = Response.HttpStatus;
				Error.Message =
					TEXT("Jira created the comment but omitted its id.");
				OnComplete.ExecuteIfBound(false, FString(), Error);
				return;
			}
			OnComplete.ExecuteIfBound(
				Response.IsSuccess(),
				CommentId,
				Response.Error);
		}));
}

void FExtendedAtlassianJira::UpdateComment(
	const FString& IssueKey,
	const FString& CommentId,
	const FString& CommentText,
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
	const FString Url = FString::Printf(
		TEXT("%s/issue/%s/comment/%s"),
		*Settings->GetJiraApiBaseUrl(),
		*IssueKey,
		*CommentId);
	Client->PutJson(
		Url,
		ExtendedAtlassianJiraPrivate::MakeAdfBody(CommentText),
		FExtendedAtlassianResponseDelegate::CreateLambda(
			[OnComplete](const FExtendedAtlassianResponse& Response)
			{
				OnComplete.ExecuteIfBound(Response.IsSuccess(), Response.Error);
			}));
}

void FExtendedAtlassianJira::DeleteComment(
	const FString& IssueKey,
	const FString& CommentId,
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
	Client->Delete(
		FString::Printf(
			TEXT("%s/issue/%s/comment/%s"),
			*Settings->GetJiraApiBaseUrl(),
			*IssueKey,
			*CommentId),
		FExtendedAtlassianResponseDelegate::CreateLambda(
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

	if (NewIssue.ProjectKey.IsEmpty()
		|| (NewIssue.IssueTypeId.IsEmpty() && NewIssue.IssueTypeName.IsEmpty()))
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
		if (!NewIssue.IssueTypeId.IsEmpty())
		{
			Writer->WriteValue(TEXT("id"), NewIssue.IssueTypeId);
		}
		else
		{
			Writer->WriteValue(TEXT("name"), NewIssue.IssueTypeName);
		}
		Writer->WriteObjectEnd();

		Writer->WriteValue(TEXT("summary"), NewIssue.Summary);

		Writer->WriteIdentifierPrefix(TEXT("description"));
		FJsonSerializer::Serialize(
			FExtendedAtlassianAdf::MakeDocWithCodeBlock(NewIssue.Description, NewIssue.ContextBlock).ToSharedRef(),
			Writer,
			false);

		// Priority is not configured on every project, and sending it where it does not exist fails
		// the whole create. Only include it when the user actually picked one.
		if (!NewIssue.PriorityId.IsEmpty() || !NewIssue.PriorityName.IsEmpty())
		{
			Writer->WriteObjectStart(TEXT("priority"));
			if (!NewIssue.PriorityId.IsEmpty())
			{
				Writer->WriteValue(TEXT("id"), NewIssue.PriorityId);
			}
			else
			{
				Writer->WriteValue(TEXT("name"), NewIssue.PriorityName);
			}
			Writer->WriteObjectEnd();
		}

		if (!NewIssue.AssigneeAccountId.IsEmpty())
		{
			Writer->WriteObjectStart(TEXT("assignee"));
			Writer->WriteValue(TEXT("accountId"), NewIssue.AssigneeAccountId);
			Writer->WriteObjectEnd();
		}

		if (!NewIssue.ParentId.IsEmpty() || !NewIssue.ParentKey.IsEmpty())
		{
			Writer->WriteObjectStart(TEXT("parent"));
			if (!NewIssue.ParentId.IsEmpty())
			{
				Writer->WriteValue(TEXT("id"), NewIssue.ParentId);
			}
			else
			{
				Writer->WriteValue(TEXT("key"), NewIssue.ParentKey);
			}
			Writer->WriteObjectEnd();
		}

		if (NewIssue.Estimate.IsSet() && !NewIssue.EstimateFieldId.IsEmpty())
		{
			Writer->WriteValue(NewIssue.EstimateFieldId, NewIssue.Estimate.GetValue());
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

void FExtendedAtlassianJira::GetAssignableUsers(
	const FString& ProjectKey,
	FExtendedAtlassianUsersDelegate OnComplete)
{
	const TSharedPtr<FExtendedAtlassianClient> Client = FUnrealExtendedAtlassianModule::GetClient();
	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();
	if (!Client.IsValid() || !Settings || ProjectKey.IsEmpty())
	{
		FExtendedAtlassianError Error;
		Error.Code = ProjectKey.IsEmpty() ? TEXT("NotConfigured") : TEXT("NotReady");
		Error.Message = ProjectKey.IsEmpty()
			? TEXT("Choose a Jira project before loading assignable users.")
			: TEXT("The Atlassian transport module is not available.");
		OnComplete.ExecuteIfBound(false, TArray<FExtendedAtlassianUser>(), Error);
		return;
	}

	const FString Url = FString::Printf(
		TEXT("%s/user/assignable/search?project=%s&maxResults=100"),
		*Settings->GetJiraApiBaseUrl(),
		*FGenericPlatformHttp::UrlEncode(ProjectKey));
	Client->Get(
		Url,
		FExtendedAtlassianResponseDelegate::CreateLambda(
			[OnComplete](const FExtendedAtlassianResponse& Response)
			{
				if (!Response.IsSuccess())
				{
					OnComplete.ExecuteIfBound(
						false,
						TArray<FExtendedAtlassianUser>(),
						Response.Error);
					return;
				}

				TArray<TSharedPtr<FJsonValue>> Values = Response.Array;
				if (!Response.bIsArray && Response.Object.IsValid())
				{
					const TArray<TSharedPtr<FJsonValue>>* Wrapped = nullptr;
					if (Response.Object->TryGetArrayField(TEXT("values"), Wrapped))
					{
						Values = *Wrapped;
					}
				}
				TArray<FExtendedAtlassianUser> Users;
				for (const TSharedPtr<FJsonValue>& Value : Values)
				{
					const TSharedPtr<FJsonObject>* Object = nullptr;
					if (!Value->TryGetObject(Object) || !Object->IsValid())
					{
						continue;
					}
					FExtendedAtlassianUser User =
						ExtendedAtlassianJiraPrivate::ParseUser(*Object);
					if (User.IsValid())
					{
						Users.Add(MoveTemp(User));
					}
				}
				OnComplete.ExecuteIfBound(true, Users, FExtendedAtlassianError());
			}));
}
