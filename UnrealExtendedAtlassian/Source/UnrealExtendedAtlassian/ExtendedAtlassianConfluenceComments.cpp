// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianConfluenceComments.h"

#include "ExtendedAtlassianClient.h"
#include "ExtendedAtlassianHtml.h"
#include "ExtendedAtlassianSettings.h"
#include "ExtendedAtlassianStorage.h"
#include "UnrealExtendedAtlassian.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"

namespace ExtendedAtlassianConfluenceCommentsPrivate
{
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
		return Settings ? Settings->GetConfluenceApiBaseUrl() : FString();
	}

	FString AbsoluteNext(const TSharedPtr<FJsonObject>& Object)
	{
		const TSharedPtr<FJsonObject>* Links = nullptr;
		FString Next;
		if (!Object.IsValid()
			|| !Object->TryGetObjectField(TEXT("_links"), Links)
			|| !Links->IsValid()
			|| !(*Links)->TryGetStringField(TEXT("next"), Next)
			|| Next.IsEmpty())
		{
			return FString();
		}
		if (Next.StartsWith(TEXT("http://")) || Next.StartsWith(TEXT("https://")))
		{
			return Next;
		}
		const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();
		const FString Site = Settings ? Settings->GetNormalizedSiteUrl() : FString();
		return Next.StartsWith(TEXT("/")) ? Site + Next : Site + TEXT("/") + Next;
	}

	FString ReadBody(const TSharedPtr<FJsonObject>& Object)
	{
		const TSharedPtr<FJsonObject>* Body = nullptr;
		if (!Object->TryGetObjectField(TEXT("body"), Body) || !Body->IsValid())
		{
			return FString();
		}
		for (const TCHAR* Representation : {TEXT("view"), TEXT("storage")})
		{
			const TSharedPtr<FJsonObject>* ValueObject = nullptr;
			FString Value;
			if ((*Body)->TryGetObjectField(Representation, ValueObject)
				&& ValueObject->IsValid()
				&& (*ValueObject)->TryGetStringField(TEXT("value"), Value))
			{
				return FExtendedAtlassianHtml::ToPlainText(Value);
			}
		}
		return FString();
	}

	FExtendedAtlassianComment Parse(
		const TSharedPtr<FJsonObject>& Object,
		const FString& ContainerId,
		bool bInline)
	{
		FExtendedAtlassianComment Comment;
		if (!Object.IsValid())
		{
			return Comment;
		}
		Object->TryGetStringField(TEXT("id"), Comment.Id);
		Object->TryGetStringField(TEXT("parentCommentId"), Comment.ParentId);
		Comment.ContainerId = ContainerId;
		Comment.Body = ReadBody(Object);
		Comment.bInline = bInline;

		const TSharedPtr<FJsonObject>* Version = nullptr;
		if (Object->TryGetObjectField(TEXT("version"), Version) && Version->IsValid())
		{
			(*Version)->TryGetNumberField(TEXT("number"), Comment.Version);
			(*Version)->TryGetStringField(TEXT("authorId"), Comment.AuthorAccountId);
			Comment.AuthorDisplayName = Comment.AuthorAccountId;
			FString CreatedAt;
			if ((*Version)->TryGetStringField(TEXT("createdAt"), CreatedAt))
			{
				FDateTime::ParseIso8601(*CreatedAt, Comment.Updated);
				Comment.Created = Comment.Updated;
			}
		}

		if (bInline)
		{
			FString Resolution;
			Object->TryGetStringField(TEXT("resolutionStatus"), Resolution);
			Comment.bResolved =
				Resolution.Equals(TEXT("resolved"), ESearchCase::IgnoreCase);
			const TSharedPtr<FJsonObject>* Properties = nullptr;
			if (Object->TryGetObjectField(TEXT("properties"), Properties)
				&& Properties->IsValid())
			{
				(*Properties)->TryGetStringField(
					TEXT("inlineOriginalSelection"),
					Comment.Quote);
			}
		}

		const TSharedPtr<FJsonObject>* Operations = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (Object->TryGetObjectField(TEXT("operations"), Operations)
			&& Operations->IsValid()
			&& (*Operations)->TryGetArrayField(TEXT("results"), Values))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Values)
			{
				const TSharedPtr<FJsonObject>* Operation = nullptr;
				FString Name;
				if (Value->TryGetObject(Operation) && Operation->IsValid()
					&& (*Operation)->TryGetStringField(TEXT("operation"), Name))
				{
					Comment.bCanEdit |= Name.Equals(TEXT("update"), ESearchCase::IgnoreCase);
					Comment.bCanDelete |= Name.Equals(TEXT("delete"), ESearchCase::IgnoreCase);
				}
			}
		}
		return Comment;
	}

	struct FLoadState : public TSharedFromThis<FLoadState>
	{
		FString PageId;
		TArray<FExtendedAtlassianComment> Roots;
		FExtendedAtlassianCommentsDelegate Completion;
		FExtendedAtlassianError FirstError;
		int32 OutstandingRoots = 2;
		int32 SuccessfulRoots = 0;
		int32 OutstandingChildren = 0;
	};

	void FinishChildren(const TSharedRef<FLoadState>& State)
	{
		if (State->OutstandingChildren > 0)
		{
			return;
		}
		const bool bSuccess = State->SuccessfulRoots > 0;
		State->Completion.ExecuteIfBound(
			bSuccess,
			State->Roots,
			bSuccess ? FExtendedAtlassianError() : State->FirstError);
	}

	void FetchChildren(
		const TSharedRef<FLoadState>& State,
		int32 RootIndex,
		FString Url);

	void BeginChildren(const TSharedRef<FLoadState>& State)
	{
		State->OutstandingChildren = State->Roots.Num();
		if (State->OutstandingChildren == 0)
		{
			FinishChildren(State);
			return;
		}
		for (int32 Index = 0; Index < State->Roots.Num(); ++Index)
		{
			const FExtendedAtlassianComment& Root = State->Roots[Index];
			FetchChildren(
				State,
				Index,
				FString::Printf(
					TEXT("%s/%s-comments/%s/children?body-format=storage&limit=100&sort=created-date"),
					*BaseUrl(),
					Root.bInline ? TEXT("inline") : TEXT("footer"),
					*Root.Id));
		}
	}

	void FinishRoot(
		const TSharedRef<FLoadState>& State,
		bool bSuccess,
		const FExtendedAtlassianError& Error)
	{
		if (bSuccess)
		{
			++State->SuccessfulRoots;
		}
		else if (!State->FirstError.IsSet())
		{
			State->FirstError = Error;
		}
		if (--State->OutstandingRoots == 0)
		{
			BeginChildren(State);
		}
	}

	void FetchRoots(
		const TSharedRef<FLoadState>& State,
		bool bInline,
		FString Url)
	{
		const TSharedPtr<FExtendedAtlassianClient> Client =
			FUnrealExtendedAtlassianModule::GetClient();
		if (!Client.IsValid())
		{
			FinishRoot(State, false, NotReady());
			return;
		}
		Client->Get(
			Url,
			FExtendedAtlassianResponseDelegate::CreateLambda(
				[State, bInline](const FExtendedAtlassianResponse& Response)
				{
					if (!Response.IsSuccess() || !Response.Object.IsValid())
					{
						FinishRoot(State, false, Response.Error);
						return;
					}
					const TArray<TSharedPtr<FJsonValue>>* Results = nullptr;
					if (Response.Object->TryGetArrayField(TEXT("results"), Results))
					{
						for (const TSharedPtr<FJsonValue>& Value : *Results)
						{
							const TSharedPtr<FJsonObject>* Object = nullptr;
							if (Value->TryGetObject(Object) && Object->IsValid())
							{
								State->Roots.Add(Parse(*Object, State->PageId, bInline));
							}
						}
					}
					const FString Next = AbsoluteNext(Response.Object);
					if (!Next.IsEmpty())
					{
						FetchRoots(State, bInline, Next);
						return;
					}
					FinishRoot(State, true, FExtendedAtlassianError());
				}));
	}

	void FetchChildren(
		const TSharedRef<FLoadState>& State,
		int32 RootIndex,
		FString Url)
	{
		const TSharedPtr<FExtendedAtlassianClient> Client =
			FUnrealExtendedAtlassianModule::GetClient();
		if (!Client.IsValid())
		{
			--State->OutstandingChildren;
			FinishChildren(State);
			return;
		}
		Client->Get(
			Url,
			FExtendedAtlassianResponseDelegate::CreateLambda(
				[State, RootIndex](const FExtendedAtlassianResponse& Response)
				{
					if (Response.IsSuccess() && Response.Object.IsValid())
					{
						const TArray<TSharedPtr<FJsonValue>>* Results = nullptr;
						if (Response.Object->TryGetArrayField(TEXT("results"), Results))
						{
							for (const TSharedPtr<FJsonValue>& Value : *Results)
							{
								const TSharedPtr<FJsonObject>* Object = nullptr;
								if (Value->TryGetObject(Object) && Object->IsValid())
								{
									FExtendedAtlassianComment Reply = Parse(
										*Object,
										State->PageId,
										State->Roots[RootIndex].bInline);
									State->Roots[RootIndex].Replies.Add(MoveTemp(Reply));
								}
							}
						}
						const FString Next = AbsoluteNext(Response.Object);
						if (!Next.IsEmpty())
						{
							FetchChildren(State, RootIndex, Next);
							return;
						}
					}
					--State->OutstandingChildren;
					FinishChildren(State);
				}));
	}

	FString MakeBody(
		const FString& Body,
		int32 Version,
		TOptional<bool> Resolved = TOptional<bool>())
	{
		FString Json;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
		Writer->WriteObjectStart();
		if (Version > 0)
		{
			Writer->WriteObjectStart(TEXT("version"));
			Writer->WriteValue(TEXT("number"), Version);
			Writer->WriteValue(TEXT("message"), TEXT("Updated from Backlot"));
			Writer->WriteObjectEnd();
		}
		if (!Body.IsEmpty())
		{
			Writer->WriteObjectStart(TEXT("body"));
			Writer->WriteValue(TEXT("representation"), TEXT("storage"));
			Writer->WriteValue(TEXT("value"), FExtendedAtlassianStorage::FromMarkdown(Body));
			Writer->WriteObjectEnd();
		}
		if (Resolved.IsSet())
		{
			Writer->WriteValue(TEXT("resolved"), Resolved.GetValue());
		}
		Writer->WriteObjectEnd();
		Writer->Close();
		return Json;
	}
}

void FExtendedAtlassianConfluenceComments::GetPageComments(
	const FString& PageId,
	FExtendedAtlassianCommentsDelegate OnComplete)
{
	using namespace ExtendedAtlassianConfluenceCommentsPrivate;
	const TSharedRef<FLoadState> State = MakeShared<FLoadState>();
	State->PageId = PageId;
	State->Completion = MoveTemp(OnComplete);
	FetchRoots(
		State,
		false,
		FString::Printf(
			TEXT("%s/pages/%s/footer-comments?body-format=storage&limit=100&sort=created-date"),
			*BaseUrl(),
			*PageId));
	FetchRoots(
		State,
		true,
		FString::Printf(
			TEXT("%s/pages/%s/inline-comments?body-format=storage&limit=100&sort=created-date"),
			*BaseUrl(),
			*PageId));
}

FExtendedAtlassianComment
FExtendedAtlassianConfluenceComments::ParseComment(
	const TSharedPtr<FJsonObject>& Object,
	const FString& ContainerId,
	bool bInline)
{
	return ExtendedAtlassianConfluenceCommentsPrivate::Parse(
		Object,
		ContainerId,
		bInline);
}

void FExtendedAtlassianConfluenceComments::CreateFooterComment(
	const FString& PageId,
	const FString& ParentCommentId,
	const FString& Body,
	FExtendedAtlassianActionDelegate OnComplete)
{
	using namespace ExtendedAtlassianConfluenceCommentsPrivate;
	const TSharedPtr<FExtendedAtlassianClient> Client =
		FUnrealExtendedAtlassianModule::GetClient();
	if (!Client.IsValid())
	{
		OnComplete.ExecuteIfBound(false, NotReady());
		return;
	}
	FString Json;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
	Writer->WriteObjectStart();
	Writer->WriteValue(
		ParentCommentId.IsEmpty() ? TEXT("pageId") : TEXT("parentCommentId"),
		ParentCommentId.IsEmpty() ? PageId : ParentCommentId);
	Writer->WriteObjectStart(TEXT("body"));
	Writer->WriteValue(TEXT("representation"), TEXT("storage"));
	Writer->WriteValue(TEXT("value"), FExtendedAtlassianStorage::FromMarkdown(Body));
	Writer->WriteObjectEnd();
	Writer->WriteObjectEnd();
	Writer->Close();
	Client->PostJson(
		BaseUrl() + TEXT("/footer-comments"),
		Json,
		FExtendedAtlassianResponseDelegate::CreateLambda(
			[OnComplete](const FExtendedAtlassianResponse& Response)
			{
				OnComplete.ExecuteIfBound(Response.IsSuccess(), Response.Error);
			}));
}

void FExtendedAtlassianConfluenceComments::UpdateComment(
	const FExtendedAtlassianComment& Comment,
	const FString& Body,
	FExtendedAtlassianActionDelegate OnComplete)
{
	using namespace ExtendedAtlassianConfluenceCommentsPrivate;
	const TSharedPtr<FExtendedAtlassianClient> Client =
		FUnrealExtendedAtlassianModule::GetClient();
	if (!Client.IsValid())
	{
		OnComplete.ExecuteIfBound(false, NotReady());
		return;
	}
	Client->PutJson(
		FString::Printf(
			TEXT("%s/%s-comments/%s"),
			*BaseUrl(),
			Comment.bInline ? TEXT("inline") : TEXT("footer"),
			*Comment.Id),
		MakeBody(Body, Comment.Version + 1),
		FExtendedAtlassianResponseDelegate::CreateLambda(
			[OnComplete](const FExtendedAtlassianResponse& Response)
			{
				OnComplete.ExecuteIfBound(Response.IsSuccess(), Response.Error);
			}));
}

void FExtendedAtlassianConfluenceComments::DeleteComment(
	const FExtendedAtlassianComment& Comment,
	FExtendedAtlassianActionDelegate OnComplete)
{
	using namespace ExtendedAtlassianConfluenceCommentsPrivate;
	const TSharedPtr<FExtendedAtlassianClient> Client =
		FUnrealExtendedAtlassianModule::GetClient();
	if (!Client.IsValid())
	{
		OnComplete.ExecuteIfBound(false, NotReady());
		return;
	}
	Client->Delete(
		FString::Printf(
			TEXT("%s/%s-comments/%s"),
			*BaseUrl(),
			Comment.bInline ? TEXT("inline") : TEXT("footer"),
			*Comment.Id),
		FExtendedAtlassianResponseDelegate::CreateLambda(
			[OnComplete](const FExtendedAtlassianResponse& Response)
			{
				OnComplete.ExecuteIfBound(Response.IsSuccess(), Response.Error);
			}));
}

void FExtendedAtlassianConfluenceComments::SetInlineResolved(
	const FExtendedAtlassianComment& Comment,
	bool bResolved,
	FExtendedAtlassianActionDelegate OnComplete)
{
	using namespace ExtendedAtlassianConfluenceCommentsPrivate;
	if (!Comment.bInline)
	{
		FExtendedAtlassianError Error;
		Error.Code = TEXT("SharedMetadataRequired");
		Error.Message =
			TEXT("Footer comment resolution requires Backlot companion metadata.");
		OnComplete.ExecuteIfBound(false, Error);
		return;
	}
	const TSharedPtr<FExtendedAtlassianClient> Client =
		FUnrealExtendedAtlassianModule::GetClient();
	if (!Client.IsValid())
	{
		OnComplete.ExecuteIfBound(false, NotReady());
		return;
	}
	Client->PutJson(
		FString::Printf(TEXT("%s/inline-comments/%s"), *BaseUrl(), *Comment.Id),
		MakeBody(Comment.Body, Comment.Version + 1, bResolved),
		FExtendedAtlassianResponseDelegate::CreateLambda(
			[OnComplete](const FExtendedAtlassianResponse& Response)
			{
				OnComplete.ExecuteIfBound(Response.IsSuccess(), Response.Error);
			}));
}
