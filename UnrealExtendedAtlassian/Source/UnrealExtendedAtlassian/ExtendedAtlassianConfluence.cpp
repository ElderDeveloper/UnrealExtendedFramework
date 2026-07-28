// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianConfluence.h"

#include "ExtendedAtlassianClient.h"
#include "ExtendedAtlassianHtml.h"
#include "ExtendedAtlassianLog.h"
#include "ExtendedAtlassianSettings.h"
#include "UnrealExtendedAtlassian.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "GenericPlatform/GenericPlatformHttp.h"

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
	using namespace ExtendedAtlassianConfluencePrivate;

	TSharedPtr<FExtendedAtlassianClient> Client;
	const UExtendedAtlassianSettings* Settings = nullptr;
	FExtendedAtlassianError Error;

	if (!GetClientAndSettings(Client, Settings, Error))
	{
		OnComplete.ExecuteIfBound(false, FExtendedAtlassianPage(), Error);
		return;
	}

	const FString BaseUrl = Settings->GetConfluenceApiBaseUrl();

	// Parses a page response, pulling the body out of whichever representation came back.
	auto ParseFullPage = [](const TSharedPtr<FJsonObject>& Object, FString& OutRawHtml) -> FExtendedAtlassianPage
	{
		FExtendedAtlassianPage Page = ParsePageSummary(Object);
		OutRawHtml.Reset();

		if (!Object.IsValid())
		{
			return Page;
		}

		const TSharedPtr<FJsonObject>* Body = nullptr;
		if (!Object->TryGetObjectField(TEXT("body"), Body) || !Body->IsValid())
		{
			return Page;
		}

		for (const TCHAR* Representation : { TEXT("view"), TEXT("export_view"), TEXT("storage") })
		{
			const TSharedPtr<FJsonObject>* Rendered = nullptr;
			if ((*Body)->TryGetObjectField(Representation, Rendered) && Rendered->IsValid())
			{
				FString Value;
				if ((*Rendered)->TryGetStringField(TEXT("value"), Value) && !Value.IsEmpty())
				{
					OutRawHtml = Value;
					break;
				}
			}
		}

		return Page;
	};

	const FString ViewUrl = FString::Printf(TEXT("%s/pages/%s?body-format=view"), *BaseUrl, *PageId);
	const FString StorageUrl = FString::Printf(TEXT("%s/pages/%s?body-format=storage"), *BaseUrl, *PageId);

	Client->Get(ViewUrl, FExtendedAtlassianResponseDelegate::CreateLambda(
		[OnComplete, ParseFullPage, StorageUrl](const FExtendedAtlassianResponse& Response)
		{
			if (!Response.IsSuccess())
			{
				OnComplete.ExecuteIfBound(false, FExtendedAtlassianPage(), Response.Error);
				return;
			}

			FString RawHtml;
			FExtendedAtlassianPage Page = ParseFullPage(Response.Object, RawHtml);

			if (!RawHtml.IsEmpty())
			{
				Page.Body = FExtendedAtlassianHtml::ToPlainText(RawHtml);
				Page.Blocks = FExtendedAtlassianHtml::ToBlocks(RawHtml);
				OnComplete.ExecuteIfBound(true, Page, FExtendedAtlassianError());
				return;
			}

			// No view rendering for this page; retry once asking for storage format.
			const TSharedPtr<FExtendedAtlassianClient> RetryClient = FUnrealExtendedAtlassianModule::GetClient();
			if (!RetryClient.IsValid())
			{
				OnComplete.ExecuteIfBound(true, Page, FExtendedAtlassianError());
				return;
			}

			UE_LOG(LogExtendedAtlassian, Verbose, TEXT("Page %s returned no view body; retrying as storage format."), *Page.Id);

			RetryClient->Get(StorageUrl, FExtendedAtlassianResponseDelegate::CreateLambda(
				[OnComplete, ParseFullPage](const FExtendedAtlassianResponse& RetryResponse)
				{
					if (!RetryResponse.IsSuccess())
					{
						OnComplete.ExecuteIfBound(false, FExtendedAtlassianPage(), RetryResponse.Error);
						return;
					}

					FString RetryHtml;
					FExtendedAtlassianPage RetryPage = ParseFullPage(RetryResponse.Object, RetryHtml);
					RetryPage.Body = FExtendedAtlassianHtml::ToPlainText(RetryHtml);

					OnComplete.ExecuteIfBound(true, RetryPage, FExtendedAtlassianError());
				}));
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
