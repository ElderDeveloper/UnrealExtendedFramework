// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianConfluence.h"

#include "ExtendedAtlassianClient.h"
#include "ExtendedAtlassianHtml.h"
#include "ExtendedAtlassianLog.h"
#include "ExtendedAtlassianMarkdown.h"
#include "ExtendedAtlassianSettings.h"
#include "ExtendedAtlassianStorage.h"
#include "UnrealExtendedAtlassian.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace ExtendedAtlassianConfluencePrivate
{
	/** Page size for cursor-paginated v2 endpoints. */
	constexpr int32 PageSize = 100;

	/** Backstop so a huge space cannot page indefinitely. */
	constexpr int32 MaxPagesFetched = 2000;

	bool GetClientAndSettings(TSharedPtr<FExtendedAtlassianClient>& OutClient, const UExtendedAtlassianSettings*& OutSettings, FExtendedAtlassianError& OutError)
	{
		OutClient = FUnrealExtendedAtlassianModule::GetClient();
		OutSettings = UExtendedAtlassianSettings::Get();

		if (!OutClient.IsValid() || !OutSettings)
		{
			OutError.Code = TEXT("NotReady");
			OutError.Message = TEXT("The Atlassian transport module is not available.");
			return false;
		}

		return true;
	}

	/** Reads _links.next and turns it into an absolute URL, or empty when there is no next page. */
	FString GetNextPageUrl(const TSharedPtr<FJsonObject>& Response)
	{
		if (!Response.IsValid())
		{
			return FString();
		}

		const TSharedPtr<FJsonObject>* Links = nullptr;
		if (!Response->TryGetObjectField(TEXT("_links"), Links) || !Links->IsValid())
		{
			return FString();
		}

		FString Next;
		if (!(*Links)->TryGetStringField(TEXT("next"), Next) || Next.IsEmpty())
		{
			return FString();
		}

		if (Next.StartsWith(TEXT("http://")) || Next.StartsWith(TEXT("https://")))
		{
			return Next;
		}

		const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();
		const FString Site = Settings ? Settings->GetNormalizedSiteUrl() : FString();
		if (Site.IsEmpty())
		{
			return FString();
		}

		return Next.StartsWith(TEXT("/")) ? Site + Next : Site + TEXT("/") + Next;
	}

	/**
	 * Reads an id field that may arrive as either a JSON string or a number.
	 *
	 * Confluence is inconsistent about this between endpoints and API revisions, and reading only
	 * strings would silently flatten the whole page hierarchy when ids come back numeric.
	 */
	FString GetIdField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
	{
		FString AsString;
		if (Object->TryGetStringField(FieldName, AsString))
		{
			return AsString;
		}

		double AsNumber = 0.0;
		if (Object->TryGetNumberField(FieldName, AsNumber))
		{
			return FString::Printf(TEXT("%lld"), static_cast<int64>(AsNumber));
		}

		return FString();
	}

	FExtendedAtlassianPage ParsePageSummary(const TSharedPtr<FJsonObject>& Object)
	{
		FExtendedAtlassianPage Page;
		if (!Object.IsValid())
		{
			return Page;
		}

		Page.Id = GetIdField(Object, TEXT("id"));
		Object->TryGetStringField(TEXT("title"), Page.Title);
		Page.SpaceId = GetIdField(Object, TEXT("spaceId"));

		// parentId is null on top-level pages, so this legitimately stays empty.
		Page.ParentId = GetIdField(Object, TEXT("parentId"));

		const TSharedPtr<FJsonObject>* Links = nullptr;
		if (Object->TryGetObjectField(TEXT("_links"), Links) && Links->IsValid())
		{
			FString WebUi;
			(*Links)->TryGetStringField(TEXT("webui"), WebUi);
			Page.WebUrl = FExtendedAtlassianConfluence::MakeWebUrl(WebUi);
		}

		return Page;
	}

	/** Follows the cursor across pages, accumulating results parsed by the supplied function. */
	struct FPagedFetch : public TSharedFromThis<FPagedFetch>
	{
		FString NextUrl;
		TArray<FExtendedAtlassianPage> Pages;
		TArray<FExtendedAtlassianSpace> Spaces;
		bool bIsSpaceQuery = false;
		FExtendedAtlassianPagesDelegate OnPages;
		FExtendedAtlassianSpacesDelegate OnSpaces;
	};

	void FetchNext(TSharedRef<FPagedFetch> Fetch);

	void CompletePaged(const TSharedRef<FPagedFetch>& Fetch, bool bSuccess, const FExtendedAtlassianError& Error)
	{
		if (Fetch->bIsSpaceQuery)
		{
			Fetch->OnSpaces.ExecuteIfBound(bSuccess, Fetch->Spaces, Error);
		}
		else
		{
			Fetch->OnPages.ExecuteIfBound(bSuccess, Fetch->Pages, Error);
		}
	}

	void HandlePagedResponse(TSharedRef<FPagedFetch> Fetch, const FExtendedAtlassianResponse& Response)
	{
		if (!Response.IsSuccess() || !Response.Object.IsValid())
		{
			CompletePaged(Fetch, false, Response.Error);
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* Results = nullptr;
		if (Response.Object->TryGetArrayField(TEXT("results"), Results))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Results)
			{
				const TSharedPtr<FJsonObject>* Object = nullptr;
				if (!Value->TryGetObject(Object) || !Object->IsValid())
				{
					continue;
				}

				if (Fetch->bIsSpaceQuery)
				{
					FExtendedAtlassianSpace Space;
					Space.Id = GetIdField(*Object, TEXT("id"));
					(*Object)->TryGetStringField(TEXT("key"), Space.Key);
					(*Object)->TryGetStringField(TEXT("name"), Space.Name);
					(*Object)->TryGetStringField(TEXT("type"), Space.Type);

					if (!Space.Id.IsEmpty())
					{
						Fetch->Spaces.Add(Space);
					}
				}
				else
				{
					const FExtendedAtlassianPage Page = ParsePageSummary(*Object);
					if (!Page.Id.IsEmpty())
					{
						Fetch->Pages.Add(Page);
					}
				}
			}
		}

		const int32 FetchedCount = Fetch->bIsSpaceQuery ? Fetch->Spaces.Num() : Fetch->Pages.Num();
		if (FetchedCount >= MaxPagesFetched)
		{
			UE_LOG(LogExtendedAtlassian, Warning,
				TEXT("Stopped paging Confluence results at %d entries. Narrow the space selection to see the rest."),
				FetchedCount);
			CompletePaged(Fetch, true, FExtendedAtlassianError());
			return;
		}

		Fetch->NextUrl = GetNextPageUrl(Response.Object);
		if (Fetch->NextUrl.IsEmpty())
		{
			CompletePaged(Fetch, true, FExtendedAtlassianError());
			return;
		}

		FetchNext(Fetch);
	}

	void FetchNext(TSharedRef<FPagedFetch> Fetch)
	{
		TSharedPtr<FExtendedAtlassianClient> Client;
		const UExtendedAtlassianSettings* Settings = nullptr;
		FExtendedAtlassianError Error;

		if (!GetClientAndSettings(Client, Settings, Error))
		{
			CompletePaged(Fetch, false, Error);
			return;
		}

		Client->Get(Fetch->NextUrl, FExtendedAtlassianResponseDelegate::CreateLambda(
			[Fetch](const FExtendedAtlassianResponse& Response)
			{
				HandlePagedResponse(Fetch, Response);
			}));
	}
}

FString FExtendedAtlassianConfluence::MakeWebUrl(const FString& WebUiLink)
{
	if (WebUiLink.IsEmpty())
	{
		return FString();
	}

	if (WebUiLink.StartsWith(TEXT("http://")) || WebUiLink.StartsWith(TEXT("https://")))
	{
		return WebUiLink;
	}

	const UExtendedAtlassianSettings* Settings = UExtendedAtlassianSettings::Get();
	const FString Site = Settings ? Settings->GetNormalizedSiteUrl() : FString();
	if (Site.IsEmpty())
	{
		return FString();
	}

	// webui links are relative to the wiki root, e.g. /spaces/KEY/pages/123/Title
	const FString Separator = WebUiLink.StartsWith(TEXT("/")) ? TEXT("") : TEXT("/");
	return Site + TEXT("/wiki") + Separator + WebUiLink;
}

void FExtendedAtlassianConfluence::ListSpaces(FExtendedAtlassianSpacesDelegate OnComplete)
{
	using namespace ExtendedAtlassianConfluencePrivate;

	TSharedPtr<FExtendedAtlassianClient> Client;
	const UExtendedAtlassianSettings* Settings = nullptr;
	FExtendedAtlassianError Error;

	if (!GetClientAndSettings(Client, Settings, Error))
	{
		OnComplete.ExecuteIfBound(false, TArray<FExtendedAtlassianSpace>(), Error);
		return;
	}

	FString Url = FString::Printf(TEXT("%s/spaces?limit=%d"), *Settings->GetConfluenceApiBaseUrl(), PageSize);

	// Personal spaces outnumber real ones on any sizeable team. They are still fetched by default,
	// but the browser groups them under a single collapsed node so they cannot swamp the list.
	if (!Settings->bIncludePersonalSpaces)
	{
		Url += TEXT("&type=global");
	}

	// Narrow server-side when the team has pinned specific spaces.
	if (Settings->SpaceKeys.Num() > 0)
	{
		TArray<FString> Keys;
		for (const FString& Key : Settings->SpaceKeys)
		{
			const FString Trimmed = Key.TrimStartAndEnd();
			if (!Trimmed.IsEmpty())
			{
				Keys.Add(Trimmed);
			}
		}

		if (Keys.Num() > 0)
		{
			Url += TEXT("&keys=") + FGenericPlatformHttp::UrlEncode(FString::Join(Keys, TEXT(",")));
		}
	}

	TSharedRef<FPagedFetch> Fetch = MakeShared<FPagedFetch>();
	Fetch->bIsSpaceQuery = true;
	Fetch->NextUrl = Url;
	Fetch->OnSpaces = OnComplete;

	FetchNext(Fetch);
}

void FExtendedAtlassianConfluence::ListPages(const FString& SpaceId, FExtendedAtlassianPagesDelegate OnComplete)
{
	using namespace ExtendedAtlassianConfluencePrivate;

	TSharedPtr<FExtendedAtlassianClient> Client;
	const UExtendedAtlassianSettings* Settings = nullptr;
	FExtendedAtlassianError Error;

	if (!GetClientAndSettings(Client, Settings, Error) || SpaceId.IsEmpty())
	{
		if (SpaceId.IsEmpty())
		{
			Error.Code = TEXT("BadRequest");
			Error.Message = TEXT("No space was specified.");
		}
		OnComplete.ExecuteIfBound(false, TArray<FExtendedAtlassianPage>(), Error);
		return;
	}

	TSharedRef<FPagedFetch> Fetch = MakeShared<FPagedFetch>();
	Fetch->bIsSpaceQuery = false;
	Fetch->NextUrl = FString::Printf(TEXT("%s/spaces/%s/pages?limit=%d&sort=title&status=current"),
		*Settings->GetConfluenceApiBaseUrl(), *SpaceId, PageSize);
	Fetch->OnPages = OnComplete;

	FetchNext(Fetch);
}

void FExtendedAtlassianConfluence::GetPage(const FString& PageId, FExtendedAtlassianPageDelegate OnComplete)
{
	// Confluence v2 only accepts STORAGE, ATLAS_DOC_FORMAT, or MARKDOWN here.
	// Hydrate the same storage-backed, versioned model used by editing so selecting a
	// page cannot display one representation and hand a different/empty one to edit.
	GetPageForEditing(PageId, OnComplete);
}

void FExtendedAtlassianConfluence::GetPageForEditing(const FString& PageId, FExtendedAtlassianPageDelegate OnComplete)
{
	using namespace ExtendedAtlassianConfluencePrivate;

	TSharedPtr<FExtendedAtlassianClient> Client;
	const UExtendedAtlassianSettings* Settings = nullptr;
	FExtendedAtlassianError Error;

	if (!GetClientAndSettings(Client, Settings, Error))
	{
		OnComplete.ExecuteIfBound(false, FExtendedAtlassianPage(), Error);
		return;
	}

	const FString Url = FString::Printf(TEXT("%s/pages/%s?body-format=storage"),
		*Settings->GetConfluenceApiBaseUrl(), *PageId);

	Client->Get(Url, FExtendedAtlassianResponseDelegate::CreateLambda(
		[OnComplete](const FExtendedAtlassianResponse& Response)
		{
			if (!Response.IsSuccess() || !Response.Object.IsValid())
			{
				OnComplete.ExecuteIfBound(false, FExtendedAtlassianPage(), Response.Error);
				return;
			}

			FExtendedAtlassianPage Page = ParsePageSummary(Response.Object);

			const TSharedPtr<FJsonObject>* Version = nullptr;
			if (Response.Object->TryGetObjectField(TEXT("version"), Version) && Version->IsValid())
			{
				(*Version)->TryGetNumberField(TEXT("number"), Page.Version);
			}

			FString Storage;
			const TSharedPtr<FJsonObject>* Body = nullptr;
			if (Response.Object->TryGetObjectField(TEXT("body"), Body) && Body->IsValid())
			{
				const TSharedPtr<FJsonObject>* Rendered = nullptr;
				if ((*Body)->TryGetObjectField(TEXT("storage"), Rendered) && Rendered->IsValid())
				{
					(*Rendered)->TryGetStringField(TEXT("value"), Storage);
				}
			}

			// Decided before anything is editable: a page we cannot rebuild must never be savable.
			Page.bCanRoundTrip = FExtendedAtlassianStorage::CanRoundTrip(Storage, Page.RoundTripBlockers);
			Page.Markdown = FExtendedAtlassianStorage::ToMarkdown(Storage);
			Page.Blocks = FExtendedAtlassianMarkdown::ToBlocks(Page.Markdown);
			Page.Body = Page.Markdown;

			if (!Page.bCanRoundTrip)
			{
				UE_LOG(LogExtendedAtlassian, Log,
					TEXT("Page %s is read-only in the editor; it contains %s."),
					*Page.Id, *FString::Join(Page.RoundTripBlockers, TEXT(", ")));
			}

			OnComplete.ExecuteIfBound(true, Page, FExtendedAtlassianError());
		}));
}

void FExtendedAtlassianConfluence::CreatePage(
	const FString& SpaceId,
	const FString& ParentId,
	const FString& Title,
	const FString& Markdown,
	FExtendedAtlassianPageDelegate OnComplete)
{
	using namespace ExtendedAtlassianConfluencePrivate;

	TSharedPtr<FExtendedAtlassianClient> Client;
	const UExtendedAtlassianSettings* Settings = nullptr;
	FExtendedAtlassianError Error;

	if (!GetClientAndSettings(Client, Settings, Error))
	{
		OnComplete.ExecuteIfBound(false, FExtendedAtlassianPage(), Error);
		return;
	}

	if (SpaceId.IsEmpty() || Title.TrimStartAndEnd().IsEmpty())
	{
		Error.Code = TEXT("BadRequest");
		Error.Message = TEXT("A space and a title are required to create a page.");
		OnComplete.ExecuteIfBound(false, FExtendedAtlassianPage(), Error);
		return;
	}

	FString Body;
	{
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);

		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("spaceId"), SpaceId);
		Writer->WriteValue(TEXT("status"), TEXT("current"));
		Writer->WriteValue(TEXT("title"), Title);

		if (!ParentId.IsEmpty())
		{
			Writer->WriteValue(TEXT("parentId"), ParentId);
		}

		Writer->WriteObjectStart(TEXT("body"));
		Writer->WriteValue(TEXT("representation"), TEXT("storage"));
		Writer->WriteValue(TEXT("value"), FExtendedAtlassianStorage::FromMarkdown(Markdown));
		Writer->WriteObjectEnd();

		Writer->WriteObjectEnd();
		Writer->Close();
	}

	Client->PostJson(Settings->GetConfluenceApiBaseUrl() + TEXT("/pages"), Body,
		FExtendedAtlassianResponseDelegate::CreateLambda(
			[OnComplete](const FExtendedAtlassianResponse& Response)
			{
				if (!Response.IsSuccess() || !Response.Object.IsValid())
				{
					OnComplete.ExecuteIfBound(false, FExtendedAtlassianPage(), Response.Error);
					return;
				}

				FExtendedAtlassianPage Page = ParsePageSummary(Response.Object);
				Page.Version = 1;
				OnComplete.ExecuteIfBound(true, Page, FExtendedAtlassianError());
			}));
}

void FExtendedAtlassianConfluence::UpdatePage(
	const FString& PageId,
	const FString& Title,
	const FString& Markdown,
	int32 ExpectedVersion,
	FExtendedAtlassianPageDelegate OnComplete)
{
	using namespace ExtendedAtlassianConfluencePrivate;

	TSharedPtr<FExtendedAtlassianClient> Client;
	const UExtendedAtlassianSettings* Settings = nullptr;
	FExtendedAtlassianError Error;

	if (!GetClientAndSettings(Client, Settings, Error))
	{
		OnComplete.ExecuteIfBound(false, FExtendedAtlassianPage(), Error);
		return;
	}

	if (ExpectedVersion <= 0)
	{
		// Without the base version there is no conflict detection, and a save could silently
		// discard someone else's edit. Refuse rather than guess.
		Error.Code = TEXT("BadRequest");
		Error.Message = TEXT("The page version is unknown. Reload the page before saving.");
		OnComplete.ExecuteIfBound(false, FExtendedAtlassianPage(), Error);
		return;
	}

	FString Body;
	{
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);

		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("id"), PageId);
		Writer->WriteValue(TEXT("status"), TEXT("current"));
		Writer->WriteValue(TEXT("title"), Title);

		Writer->WriteObjectStart(TEXT("body"));
		Writer->WriteValue(TEXT("representation"), TEXT("storage"));
		Writer->WriteValue(TEXT("value"), FExtendedAtlassianStorage::FromMarkdown(Markdown));
		Writer->WriteObjectEnd();

		Writer->WriteObjectStart(TEXT("version"));
		Writer->WriteValue(TEXT("number"), ExpectedVersion + 1);
		Writer->WriteValue(TEXT("message"), TEXT("Edited from Unreal Editor (Extended Atlassian)"));
		Writer->WriteObjectEnd();

		Writer->WriteObjectEnd();
		Writer->Close();
	}

	const FString Url = FString::Printf(TEXT("%s/pages/%s"), *Settings->GetConfluenceApiBaseUrl(), *PageId);

	Client->PutJson(Url, Body, FExtendedAtlassianResponseDelegate::CreateLambda(
		[OnComplete, ExpectedVersion](const FExtendedAtlassianResponse& Response)
		{
			if (!Response.IsSuccess())
			{
				FExtendedAtlassianError Failure = Response.Error;

				// 409 means someone else saved after this edit began; say so in those terms.
				if (Failure.HttpStatus == 409)
				{
					Failure.Code = TEXT("VersionConflict");
					Failure.Message = FString::Printf(
						TEXT("This page changed in Confluence since you loaded version %d. Reload before saving, or your edit would overwrite theirs."),
						ExpectedVersion);
				}

				OnComplete.ExecuteIfBound(false, FExtendedAtlassianPage(), Failure);
				return;
			}

			FExtendedAtlassianPage Page = ParsePageSummary(Response.Object);
			Page.Version = ExpectedVersion + 1;
			OnComplete.ExecuteIfBound(true, Page, FExtendedAtlassianError());
		}));
}

void FExtendedAtlassianConfluence::DeletePage(const FString& PageId, FExtendedAtlassianActionDelegate OnComplete)
{
	using namespace ExtendedAtlassianConfluencePrivate;

	TSharedPtr<FExtendedAtlassianClient> Client;
	const UExtendedAtlassianSettings* Settings = nullptr;
	FExtendedAtlassianError Error;

	if (!GetClientAndSettings(Client, Settings, Error))
	{
		OnComplete.ExecuteIfBound(false, Error);
		return;
	}

	const FString Url = FString::Printf(TEXT("%s/pages/%s"), *Settings->GetConfluenceApiBaseUrl(), *PageId);

	Client->Delete(Url, FExtendedAtlassianResponseDelegate::CreateLambda(
		[OnComplete](const FExtendedAtlassianResponse& Response)
		{
			OnComplete.ExecuteIfBound(Response.IsSuccess(), Response.Error);
		}));
}

void FExtendedAtlassianConfluence::Search(const FString& Cql, FExtendedAtlassianPagesDelegate OnComplete)
{
	using namespace ExtendedAtlassianConfluencePrivate;

	TSharedPtr<FExtendedAtlassianClient> Client;
	const UExtendedAtlassianSettings* Settings = nullptr;
	FExtendedAtlassianError Error;

	if (!GetClientAndSettings(Client, Settings, Error))
	{
		OnComplete.ExecuteIfBound(false, TArray<FExtendedAtlassianPage>(), Error);
		return;
	}

	// Search has no v2 equivalent yet.
	const FString Url = FString::Printf(TEXT("%s/search?cql=%s&limit=50"),
		*Settings->GetConfluenceV1ApiBaseUrl(),
		*FGenericPlatformHttp::UrlEncode(Cql));

	Client->Get(Url, FExtendedAtlassianResponseDelegate::CreateLambda(
		[OnComplete](const FExtendedAtlassianResponse& Response)
		{
			if (!Response.IsSuccess() || !Response.Object.IsValid())
			{
				OnComplete.ExecuteIfBound(false, TArray<FExtendedAtlassianPage>(), Response.Error);
				return;
			}

			TArray<FExtendedAtlassianPage> Pages;

			const TArray<TSharedPtr<FJsonValue>>* Results = nullptr;
			if (Response.Object->TryGetArrayField(TEXT("results"), Results))
			{
				for (const TSharedPtr<FJsonValue>& Value : *Results)
				{
					const TSharedPtr<FJsonObject>* Object = nullptr;
					if (!Value->TryGetObject(Object) || !Object->IsValid())
					{
						continue;
					}

					FExtendedAtlassianPage Page;

					// v1 search nests the page under "content" and repeats the title alongside it.
					const TSharedPtr<FJsonObject>* Content = nullptr;
					if ((*Object)->TryGetObjectField(TEXT("content"), Content) && Content->IsValid())
					{
						Page.Id = GetIdField(*Content, TEXT("id"));
						(*Content)->TryGetStringField(TEXT("title"), Page.Title);
					}

					if (Page.Title.IsEmpty())
					{
						FString Title;
						(*Object)->TryGetStringField(TEXT("title"), Title);
						// Search titles carry @@@hl@@@ highlight markers; strip them.
						Title.ReplaceInline(TEXT("@@@hl@@@"), TEXT(""));
						Title.ReplaceInline(TEXT("@@@endhl@@@"), TEXT(""));
						Page.Title = FExtendedAtlassianHtml::DecodeEntities(Title);
					}

					FString RelativeUrl;
					(*Object)->TryGetStringField(TEXT("url"), RelativeUrl);
					Page.WebUrl = FExtendedAtlassianConfluence::MakeWebUrl(RelativeUrl);

					if (!Page.Id.IsEmpty())
					{
						Pages.Add(Page);
					}
				}
			}

			OnComplete.ExecuteIfBound(true, Pages, FExtendedAtlassianError());
		}));
}
