// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianJiraSoftware.h"

#include "ExtendedAtlassianClient.h"
#include "ExtendedAtlassianSettings.h"
#include "UnrealExtendedAtlassian.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"

namespace ExtendedAtlassianJiraSoftwarePrivate
{
	constexpr int32 PageSize = 50;

	FExtendedAtlassianError NotReady()
	{
		FExtendedAtlassianError Error;
		Error.Code = TEXT("NotReady");
		Error.Message = TEXT("The Atlassian transport module is not available.");
		return Error;
	}

	FString BaseUrl()
	{
		const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();
		return Settings ? Settings->GetJiraSoftwareApiBaseUrl() : FString();
	}

	FString JsonStringBody(
		const TCHAR* OuterName,
		const TCHAR* InnerName,
		const FString& Value)
	{
		FString Body;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
		Writer->WriteObjectStart();
		Writer->WriteArrayStart(OuterName);
		Writer->WriteObjectStart();
		Writer->WriteValue(InnerName, Value);
		Writer->WriteObjectEnd();
		Writer->WriteArrayEnd();
		Writer->WriteObjectEnd();
		Writer->Close();
		return Body;
	}

	struct FBoardsState
	{
		FString ProjectKey;
		int32 StartAt = 0;
		TArray<FExtendedAtlassianBoard> Values;
		FExtendedAtlassianBoardsDelegate Completion;
	};

	void FetchBoards(const TSharedRef<FBoardsState>& State)
	{
		const TSharedPtr<FExtendedAtlassianClient> Client =
			FUnrealExtendedAtlassianModule::GetClient();
		if (!Client.IsValid())
		{
			State->Completion.ExecuteIfBound(false, State->Values, NotReady());
			return;
		}
		FString Url = FString::Printf(
			TEXT("%s/board?startAt=%d&maxResults=%d"),
			*BaseUrl(),
			State->StartAt,
			PageSize);
		if (!State->ProjectKey.IsEmpty())
		{
			Url += TEXT("&projectKeyOrId=")
				+ FGenericPlatformHttp::UrlEncode(State->ProjectKey);
		}
		Client->Get(
			Url,
			FExtendedAtlassianResponseDelegate::CreateLambda(
				[State](const FExtendedAtlassianResponse& Response)
				{
					if (!Response.IsSuccess() || !Response.Object.IsValid())
					{
						State->Completion.ExecuteIfBound(
							false,
							State->Values,
							Response.Error);
						return;
					}
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
							FExtendedAtlassianBoard Board;
							double NumericId = 0.0;
							if ((*Object)->TryGetNumberField(TEXT("id"), NumericId))
							{
								Board.Id = FString::Printf(TEXT("%.0f"), NumericId);
							}
							else
							{
								(*Object)->TryGetStringField(TEXT("id"), Board.Id);
							}
							(*Object)->TryGetStringField(TEXT("name"), Board.Name);
							(*Object)->TryGetStringField(TEXT("type"), Board.Type);
							(*Object)->TryGetStringField(TEXT("self"), Board.SelfUrl);
							const TSharedPtr<FJsonObject>* Location = nullptr;
							if ((*Object)->TryGetObjectField(TEXT("location"), Location)
								&& Location->IsValid())
							{
								(*Location)->TryGetStringField(
									TEXT("projectKey"),
									Board.ProjectKey);
							}
							State->Values.Add(MoveTemp(Board));
						}
					}
					bool bIsLast = true;
					Response.Object->TryGetBoolField(TEXT("isLast"), bIsLast);
					if (!bIsLast && Values && !Values->IsEmpty())
					{
						State->StartAt += Values->Num();
						FetchBoards(State);
						return;
					}
					State->Completion.ExecuteIfBound(
						true,
						State->Values,
						FExtendedAtlassianError());
				}));
	}

	struct FSprintsState
	{
		FString BoardId;
		int32 StartAt = 0;
		TArray<FExtendedAtlassianSprint> Values;
		FExtendedAtlassianSprintsDelegate Completion;
	};

	void FetchSprints(const TSharedRef<FSprintsState>& State)
	{
		const TSharedPtr<FExtendedAtlassianClient> Client =
			FUnrealExtendedAtlassianModule::GetClient();
		if (!Client.IsValid())
		{
			State->Completion.ExecuteIfBound(false, State->Values, NotReady());
			return;
		}
		const FString Url = FString::Printf(
			TEXT("%s/board/%s/sprint?state=active,future&startAt=%d&maxResults=%d"),
			*BaseUrl(),
			*State->BoardId,
			State->StartAt,
			PageSize);
		Client->Get(
			Url,
			FExtendedAtlassianResponseDelegate::CreateLambda(
				[State](const FExtendedAtlassianResponse& Response)
				{
					if (!Response.IsSuccess() || !Response.Object.IsValid())
					{
						State->Completion.ExecuteIfBound(
							false,
							State->Values,
							Response.Error);
						return;
					}
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
							FExtendedAtlassianSprint Sprint;
							double NumericId = 0.0;
							if ((*Object)->TryGetNumberField(TEXT("id"), NumericId))
							{
								Sprint.Id = FString::Printf(TEXT("%.0f"), NumericId);
							}
							else
							{
								(*Object)->TryGetStringField(TEXT("id"), Sprint.Id);
							}
							(*Object)->TryGetStringField(TEXT("name"), Sprint.Name);
							(*Object)->TryGetStringField(TEXT("state"), Sprint.State);
							(*Object)->TryGetStringField(TEXT("goal"), Sprint.Goal);
							FString Date;
							if ((*Object)->TryGetStringField(TEXT("startDate"), Date))
							{
								FDateTime::ParseIso8601(*Date, Sprint.StartDate);
							}
							if ((*Object)->TryGetStringField(TEXT("endDate"), Date))
							{
								FDateTime::ParseIso8601(*Date, Sprint.EndDate);
							}
							if ((*Object)->TryGetStringField(TEXT("completeDate"), Date))
							{
								FDateTime::ParseIso8601(*Date, Sprint.CompleteDate);
							}
							State->Values.Add(MoveTemp(Sprint));
						}
					}
					bool bIsLast = true;
					Response.Object->TryGetBoolField(TEXT("isLast"), bIsLast);
					if (!bIsLast && Values && !Values->IsEmpty())
					{
						State->StartAt += Values->Num();
						FetchSprints(State);
						return;
					}
					State->Completion.ExecuteIfBound(
						true,
						State->Values,
						FExtendedAtlassianError());
				}));
	}

	struct FIssuesState
	{
		FString SprintId;
		int32 StartAt = 0;
		int32 MaxResults = 200;
		TArray<FExtendedAtlassianIssue> Values;
		FExtendedAtlassianIssuesDelegate Completion;
	};

	void CompleteIssues(
		const TSharedRef<FIssuesState>& State,
		bool bSuccess,
		const FExtendedAtlassianError& Error,
		bool bTruncated = false)
	{
		FExtendedAtlassianIssueQueryResult Result;
		Result.bSuccess = bSuccess;
		Result.Error = Error;
		Result.bTruncated = bTruncated;
		Result.Issues = MoveTemp(State->Values);
		State->Completion.ExecuteIfBound(Result);
	}

	void FetchIssues(const TSharedRef<FIssuesState>& State)
	{
		const TSharedPtr<FExtendedAtlassianClient> Client =
			FUnrealExtendedAtlassianModule::GetClient();
		if (!Client.IsValid())
		{
			CompleteIssues(State, false, NotReady());
			return;
		}
		const int32 Count = FMath::Min(PageSize, State->MaxResults - State->Values.Num());
		FString Fields =
			TEXT("summary,status,issuetype,priority,assignee,reporter,labels,created,updated,")
			TEXT("description,parent,comment");
		if (const UExtendedAtlassianSettings* Settings =
			UExtendedAtlassianSettings::Get())
		{
			if (!Settings->DiscoveredEstimateFieldId.IsEmpty())
			{
				Fields += TEXT(",") + Settings->DiscoveredEstimateFieldId;
			}
			if (!Settings->DiscoveredRankFieldId.IsEmpty())
			{
				Fields += TEXT(",") + Settings->DiscoveredRankFieldId;
			}
		}
		const FString Url = FString::Printf(
			TEXT("%s/sprint/%s/issue?startAt=%d&maxResults=%d&fields=%s"),
			*BaseUrl(),
			*State->SprintId,
			State->StartAt,
			Count,
			*FGenericPlatformHttp::UrlEncode(Fields));
		Client->Get(
			Url,
			FExtendedAtlassianResponseDelegate::CreateLambda(
				[State](const FExtendedAtlassianResponse& Response)
				{
					if (!Response.IsSuccess() || !Response.Object.IsValid())
					{
						CompleteIssues(State, false, Response.Error);
						return;
					}
					const TArray<TSharedPtr<FJsonValue>>* Issues = nullptr;
					if (Response.Object->TryGetArrayField(TEXT("issues"), Issues))
					{
						for (const TSharedPtr<FJsonValue>& Value : *Issues)
						{
							const TSharedPtr<FJsonObject>* Object = nullptr;
							if (Value->TryGetObject(Object) && Object->IsValid())
							{
								State->Values.Add(FExtendedAtlassianJira::ParseIssue(*Object));
							}
						}
					}
					int32 Total = State->Values.Num();
					Response.Object->TryGetNumberField(TEXT("total"), Total);
					const bool bMore = State->Values.Num() < Total;
					if (bMore && State->Values.Num() < State->MaxResults
						&& Issues && !Issues->IsEmpty())
					{
						State->StartAt += Issues->Num();
						FetchIssues(State);
						return;
					}
					CompleteIssues(
						State,
						true,
						FExtendedAtlassianError(),
						bMore);
				}));
	}
}

void FExtendedAtlassianJiraSoftware::ListBoards(
	const FString& ProjectKey,
	FExtendedAtlassianBoardsDelegate OnComplete)
{
	const TSharedRef<ExtendedAtlassianJiraSoftwarePrivate::FBoardsState> State =
		MakeShared<ExtendedAtlassianJiraSoftwarePrivate::FBoardsState>();
	State->ProjectKey = ProjectKey;
	State->Completion = MoveTemp(OnComplete);
	ExtendedAtlassianJiraSoftwarePrivate::FetchBoards(State);
}

void FExtendedAtlassianJiraSoftware::GetBoardConfiguration(
	const FString& BoardId,
	FExtendedAtlassianBoardConfigurationDelegate OnComplete)
{
	const TSharedPtr<FExtendedAtlassianClient> Client =
		FUnrealExtendedAtlassianModule::GetClient();
	if (!Client.IsValid())
	{
		OnComplete.ExecuteIfBound(
			false,
			FExtendedAtlassianBoardConfiguration(),
			ExtendedAtlassianJiraSoftwarePrivate::NotReady());
		return;
	}
	Client->Get(
		FString::Printf(
			TEXT("%s/board/%s/configuration"),
			*ExtendedAtlassianJiraSoftwarePrivate::BaseUrl(),
			*BoardId),
		FExtendedAtlassianResponseDelegate::CreateLambda(
			[OnComplete](const FExtendedAtlassianResponse& Response)
			{
				if (!Response.IsSuccess() || !Response.Object.IsValid())
				{
					OnComplete.ExecuteIfBound(
						false,
						FExtendedAtlassianBoardConfiguration(),
						Response.Error);
					return;
				}
				FExtendedAtlassianBoardConfiguration Configuration;
				FExtendedAtlassianError ParseError;
				if (!ParseBoardConfiguration(
						Response.Object,
						Configuration,
						ParseError))
				{
					OnComplete.ExecuteIfBound(
						false,
						FExtendedAtlassianBoardConfiguration(),
						ParseError);
					return;
				}
				OnComplete.ExecuteIfBound(
					true,
					Configuration,
					FExtendedAtlassianError());
			}));
}

void FExtendedAtlassianJiraSoftware::ListSprints(
	const FString& BoardId,
	FExtendedAtlassianSprintsDelegate OnComplete)
{
	const TSharedRef<ExtendedAtlassianJiraSoftwarePrivate::FSprintsState> State =
		MakeShared<ExtendedAtlassianJiraSoftwarePrivate::FSprintsState>();
	State->BoardId = BoardId;
	State->Completion = MoveTemp(OnComplete);
	ExtendedAtlassianJiraSoftwarePrivate::FetchSprints(State);
}

void FExtendedAtlassianJiraSoftware::GetSprintIssues(
	const FString& SprintId,
	int32 MaxResults,
	FExtendedAtlassianIssuesDelegate OnComplete)
{
	const TSharedRef<ExtendedAtlassianJiraSoftwarePrivate::FIssuesState> State =
		MakeShared<ExtendedAtlassianJiraSoftwarePrivate::FIssuesState>();
	State->SprintId = SprintId;
	State->MaxResults = FMath::Max(1, MaxResults);
	State->Completion = MoveTemp(OnComplete);
	ExtendedAtlassianJiraSoftwarePrivate::FetchIssues(State);
}

void FExtendedAtlassianJiraSoftware::RankIssue(
	const FString& IssueKey,
	const FString& RankRelativeToKey,
	bool bBefore,
	FExtendedAtlassianActionDelegate OnComplete)
{
	const TSharedPtr<FExtendedAtlassianClient> Client =
		FUnrealExtendedAtlassianModule::GetClient();
	if (!Client.IsValid())
	{
		OnComplete.ExecuteIfBound(
			false,
			ExtendedAtlassianJiraSoftwarePrivate::NotReady());
		return;
	}
	const FString Body = BuildRankBody(
		IssueKey,
		RankRelativeToKey,
		bBefore);
	Client->PutJson(
		ExtendedAtlassianJiraSoftwarePrivate::BaseUrl() + TEXT("/issue/rank"),
		Body,
		FExtendedAtlassianResponseDelegate::CreateLambda(
			[OnComplete](const FExtendedAtlassianResponse& Response)
			{
				OnComplete.ExecuteIfBound(Response.IsSuccess(), Response.Error);
			}));
}

void FExtendedAtlassianJiraSoftware::SetIssueEstimate(
	const FString& IssueKey,
	const FString& BoardId,
	const FString& Estimate,
	FExtendedAtlassianActionDelegate OnComplete)
{
	const TSharedPtr<FExtendedAtlassianClient> Client =
		FUnrealExtendedAtlassianModule::GetClient();
	if (!Client.IsValid())
	{
		OnComplete.ExecuteIfBound(
			false,
			ExtendedAtlassianJiraSoftwarePrivate::NotReady());
		return;
	}
	const FString Body = BuildEstimateBody(Estimate);
	Client->PutJson(
		FString::Printf(
			TEXT("%s/issue/%s/estimation?boardId=%s"),
			*ExtendedAtlassianJiraSoftwarePrivate::BaseUrl(),
			*IssueKey,
			*BoardId),
		Body,
		FExtendedAtlassianResponseDelegate::CreateLambda(
			[OnComplete](const FExtendedAtlassianResponse& Response)
			{
				OnComplete.ExecuteIfBound(Response.IsSuccess(), Response.Error);
			}));
}

void FExtendedAtlassianJiraSoftware::GetIssueEstimate(
	const FString& IssueKey,
	const FString& BoardId,
	FExtendedAtlassianEstimateDelegate OnComplete)
{
	const TSharedPtr<FExtendedAtlassianClient> Client =
		FUnrealExtendedAtlassianModule::GetClient();
	if (!Client.IsValid())
	{
		OnComplete.ExecuteIfBound(
			false,
			FString(),
			FString(),
			ExtendedAtlassianJiraSoftwarePrivate::NotReady());
		return;
	}
	Client->Get(
		FString::Printf(
			TEXT("%s/issue/%s/estimation?boardId=%s"),
			*ExtendedAtlassianJiraSoftwarePrivate::BaseUrl(),
			*IssueKey,
			*BoardId),
		FExtendedAtlassianResponseDelegate::CreateLambda(
			[OnComplete](const FExtendedAtlassianResponse& Response)
			{
				if (!Response.IsSuccess() || !Response.Object.IsValid())
				{
					OnComplete.ExecuteIfBound(
						false,
						FString(),
						FString(),
						Response.Error);
					return;
				}
				FString Value;
				FString FieldId;
				FExtendedAtlassianError ParseError;
				if (!ParseEstimate(
						Response.Object,
						Value,
						FieldId,
						ParseError))
				{
					OnComplete.ExecuteIfBound(
						false,
						FString(),
						FString(),
						ParseError);
					return;
				}
				OnComplete.ExecuteIfBound(
					true,
					Value,
					FieldId,
					FExtendedAtlassianError());
			}));
}

bool FExtendedAtlassianJiraSoftware::ParseBoardsPage(
	const TSharedPtr<FJsonObject>& Object,
	TArray<FExtendedAtlassianBoard>& OutBoards,
	bool& bOutIsLast,
	FExtendedAtlassianError& OutError)
{
	OutBoards.Reset();
	bOutIsLast = true;
	OutError = FExtendedAtlassianError();
	if (!Object.IsValid())
	{
		OutError.Code = TEXT("UnexpectedResponse");
		OutError.Message = TEXT("Jira Software returned an invalid boards page.");
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (Object->TryGetArrayField(TEXT("values"), Values))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TSharedPtr<FJsonObject>* BoardObject = nullptr;
			if (!Value->TryGetObject(BoardObject) || !BoardObject->IsValid())
			{
				continue;
			}
			FExtendedAtlassianBoard Board;
			double NumericId = 0.0;
			if ((*BoardObject)->TryGetNumberField(TEXT("id"), NumericId))
			{
				Board.Id = FString::Printf(TEXT("%.0f"), NumericId);
			}
			else
			{
				(*BoardObject)->TryGetStringField(TEXT("id"), Board.Id);
			}
			(*BoardObject)->TryGetStringField(TEXT("name"), Board.Name);
			(*BoardObject)->TryGetStringField(TEXT("type"), Board.Type);
			(*BoardObject)->TryGetStringField(TEXT("self"), Board.SelfUrl);
			const TSharedPtr<FJsonObject>* Location = nullptr;
			if ((*BoardObject)->TryGetObjectField(TEXT("location"), Location)
				&& Location->IsValid())
			{
				(*Location)->TryGetStringField(
					TEXT("projectKey"),
					Board.ProjectKey);
			}
			OutBoards.Add(MoveTemp(Board));
		}
	}
	Object->TryGetBoolField(TEXT("isLast"), bOutIsLast);
	return true;
}

bool FExtendedAtlassianJiraSoftware::ParseSprintsPage(
	const TSharedPtr<FJsonObject>& Object,
	TArray<FExtendedAtlassianSprint>& OutSprints,
	bool& bOutIsLast,
	FExtendedAtlassianError& OutError)
{
	OutSprints.Reset();
	bOutIsLast = true;
	OutError = FExtendedAtlassianError();
	if (!Object.IsValid())
	{
		OutError.Code = TEXT("UnexpectedResponse");
		OutError.Message = TEXT("Jira Software returned an invalid sprints page.");
		return false;
	}
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (Object->TryGetArrayField(TEXT("values"), Values))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TSharedPtr<FJsonObject>* SprintObject = nullptr;
			if (!Value->TryGetObject(SprintObject) || !SprintObject->IsValid())
			{
				continue;
			}
			FExtendedAtlassianSprint Sprint;
			double NumericId = 0.0;
			if ((*SprintObject)->TryGetNumberField(TEXT("id"), NumericId))
			{
				Sprint.Id = FString::Printf(TEXT("%.0f"), NumericId);
			}
			else
			{
				(*SprintObject)->TryGetStringField(TEXT("id"), Sprint.Id);
			}
			(*SprintObject)->TryGetStringField(TEXT("name"), Sprint.Name);
			(*SprintObject)->TryGetStringField(TEXT("state"), Sprint.State);
			(*SprintObject)->TryGetStringField(TEXT("goal"), Sprint.Goal);
			FString Date;
			if ((*SprintObject)->TryGetStringField(TEXT("startDate"), Date))
			{
				FDateTime::ParseIso8601(*Date, Sprint.StartDate);
			}
			if ((*SprintObject)->TryGetStringField(TEXT("endDate"), Date))
			{
				FDateTime::ParseIso8601(*Date, Sprint.EndDate);
			}
			if ((*SprintObject)->TryGetStringField(TEXT("completeDate"), Date))
			{
				FDateTime::ParseIso8601(*Date, Sprint.CompleteDate);
			}
			OutSprints.Add(MoveTemp(Sprint));
		}
	}
	Object->TryGetBoolField(TEXT("isLast"), bOutIsLast);
	return true;
}

bool FExtendedAtlassianJiraSoftware::ParseBoardConfiguration(
	const TSharedPtr<FJsonObject>& Object,
	FExtendedAtlassianBoardConfiguration& OutConfiguration,
	FExtendedAtlassianError& OutError)
{
	OutConfiguration = FExtendedAtlassianBoardConfiguration();
	OutError = FExtendedAtlassianError();
	if (!Object.IsValid())
	{
		OutError.Code = TEXT("UnexpectedResponse");
		OutError.Message =
			TEXT("Jira Software returned an invalid board configuration.");
		return false;
	}
	const TSharedPtr<FJsonObject>* ColumnConfig = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
	if (Object->TryGetObjectField(TEXT("columnConfig"), ColumnConfig)
		&& ColumnConfig->IsValid()
		&& (*ColumnConfig)->TryGetArrayField(TEXT("columns"), Values))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Values)
		{
			const TSharedPtr<FJsonObject>* ColumnObject = nullptr;
			if (!Value->TryGetObject(ColumnObject) || !ColumnObject->IsValid())
			{
				continue;
			}
			FExtendedAtlassianBoardColumn Column;
			(*ColumnObject)->TryGetStringField(TEXT("name"), Column.DisplayName);
			Column.Id = Column.DisplayName;
			double WipLimit = 0.0;
			if ((*ColumnObject)->TryGetNumberField(TEXT("max"), WipLimit))
			{
				Column.WipLimit = FMath::Max(0, FMath::RoundToInt(WipLimit));
			}
			const TArray<TSharedPtr<FJsonValue>>* Statuses = nullptr;
			if ((*ColumnObject)->TryGetArrayField(TEXT("statuses"), Statuses))
			{
				for (const TSharedPtr<FJsonValue>& StatusValue : *Statuses)
				{
					const TSharedPtr<FJsonObject>* Status = nullptr;
					FString StatusId;
					if (StatusValue->TryGetObject(Status) && Status->IsValid()
						&& (*Status)->TryGetStringField(TEXT("id"), StatusId))
					{
						Column.StatusIds.Add(MoveTemp(StatusId));
					}
				}
			}
			OutConfiguration.Columns.Add(MoveTemp(Column));
		}
	}
	const TSharedPtr<FJsonObject>* Estimation = nullptr;
	const TSharedPtr<FJsonObject>* EstimateField = nullptr;
	if (Object->TryGetObjectField(TEXT("estimation"), Estimation)
		&& Estimation->IsValid()
		&& (*Estimation)->TryGetObjectField(TEXT("field"), EstimateField)
		&& EstimateField->IsValid())
	{
		(*EstimateField)->TryGetStringField(
			TEXT("fieldId"),
			OutConfiguration.EstimateFieldId);
		(*EstimateField)->TryGetStringField(
			TEXT("displayName"),
			OutConfiguration.EstimateFieldName);
	}
	const TSharedPtr<FJsonObject>* Ranking = nullptr;
	if (Object->TryGetObjectField(TEXT("ranking"), Ranking)
		&& Ranking->IsValid())
	{
		if (!(*Ranking)->TryGetStringField(
			TEXT("rankCustomFieldId"),
			OutConfiguration.RankFieldId))
		{
			double NumericRankId = 0.0;
			if ((*Ranking)->TryGetNumberField(
				TEXT("rankCustomFieldId"),
				NumericRankId))
			{
				OutConfiguration.RankFieldId = FString::Printf(
					TEXT("customfield_%.0f"),
					NumericRankId);
			}
		}
		else if (OutConfiguration.RankFieldId.IsNumeric())
		{
			OutConfiguration.RankFieldId =
				TEXT("customfield_") + OutConfiguration.RankFieldId;
		}
	}
	return true;
}

FString FExtendedAtlassianJiraSoftware::BuildRankBody(
	const FString& IssueKey,
	const FString& RankRelativeToKey,
	bool bBefore)
{
	FString Body;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
	Writer->WriteObjectStart();
	Writer->WriteArrayStart(TEXT("issues"));
	Writer->WriteValue(IssueKey);
	Writer->WriteArrayEnd();
	Writer->WriteValue(
		bBefore ? TEXT("rankBeforeIssue") : TEXT("rankAfterIssue"),
		RankRelativeToKey);
	Writer->WriteObjectEnd();
	Writer->Close();
	return Body;
}

FString FExtendedAtlassianJiraSoftware::BuildEstimateBody(
	const FString& Estimate)
{
	FString Body;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
	Writer->WriteObjectStart();
	Writer->WriteValue(TEXT("value"), Estimate);
	Writer->WriteObjectEnd();
	Writer->Close();
	return Body;
}

bool FExtendedAtlassianJiraSoftware::ParseEstimate(
	const TSharedPtr<FJsonObject>& Object,
	FString& OutValue,
	FString& OutFieldId,
	FExtendedAtlassianError& OutError)
{
	OutValue.Reset();
	OutFieldId.Reset();
	OutError = FExtendedAtlassianError();
	if (!Object.IsValid())
	{
		OutError.Code = TEXT("UnexpectedResponse");
		OutError.Message = TEXT("Jira Software returned an invalid estimate.");
		return false;
	}
	Object->TryGetStringField(TEXT("value"), OutValue);
	Object->TryGetStringField(TEXT("fieldId"), OutFieldId);
	return true;
}
