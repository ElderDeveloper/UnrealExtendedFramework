// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedAtlassianConfluenceProperties.h"

#include "ExtendedAtlassianClient.h"
#include "ExtendedAtlassianSettings.h"
#include "UnrealExtendedAtlassian.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace ExtendedAtlassianConfluencePropertiesPrivate
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

	FExtendedAtlassianContentProperty Parse(const TSharedPtr<FJsonObject>& Object)
	{
		FExtendedAtlassianContentProperty Property;
		if (!Object.IsValid())
		{
			return Property;
		}
		Object->TryGetStringField(TEXT("id"), Property.Id);
		Object->TryGetStringField(TEXT("key"), Property.Key);
		const TSharedPtr<FJsonObject>* Value = nullptr;
		if (Object->TryGetObjectField(TEXT("value"), Value) && Value->IsValid())
		{
			Property.Value = *Value;
		}
		const TSharedPtr<FJsonObject>* Version = nullptr;
		if (Object->TryGetObjectField(TEXT("version"), Version) && Version->IsValid())
		{
			(*Version)->TryGetNumberField(TEXT("number"), Property.Version);
		}
		return Property;
	}

	FString MakeBody(
		const FString& Key,
		const TSharedRef<FJsonObject>& Value,
		int32 Version)
	{
		FString Body;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
		Writer->WriteObjectStart();
		Writer->WriteValue(TEXT("key"), Key);
		Writer->WriteIdentifierPrefix(TEXT("value"));
		FJsonSerializer::Serialize(Value, Writer, false);
		if (Version > 0)
		{
			Writer->WriteObjectStart(TEXT("version"));
			Writer->WriteValue(TEXT("number"), Version);
			Writer->WriteValue(TEXT("message"), TEXT("Updated from Backlot"));
			Writer->WriteObjectEnd();
		}
		Writer->WriteObjectEnd();
		Writer->Close();
		return Body;
	}

	void GetById(
		const FString& PageId,
		const FString& PropertyId,
		FExtendedAtlassianContentPropertyDelegate OnComplete)
	{
		const TSharedPtr<FExtendedAtlassianClient> Client =
			FUnrealExtendedAtlassianModule::GetClient();
		if (!Client.IsValid())
		{
			OnComplete.ExecuteIfBound(
				false,
				FExtendedAtlassianContentProperty(),
				NotReady());
			return;
		}
		Client->Get(
			FString::Printf(
				TEXT("%s/pages/%s/properties/%s"),
				*BaseUrl(),
				*PageId,
				*PropertyId),
			FExtendedAtlassianResponseDelegate::CreateLambda(
				[OnComplete](const FExtendedAtlassianResponse& Response)
				{
					if (!Response.IsSuccess() || !Response.Object.IsValid())
					{
						OnComplete.ExecuteIfBound(
							false,
							FExtendedAtlassianContentProperty(),
							Response.Error);
						return;
					}
					OnComplete.ExecuteIfBound(
						true,
						Parse(Response.Object),
						FExtendedAtlassianError());
				}));
	}
}

void FExtendedAtlassianConfluenceProperties::GetPageProperty(
	const FString& PageId,
	const FString& Key,
	FExtendedAtlassianContentPropertyDelegate OnComplete)
{
	using namespace ExtendedAtlassianConfluencePropertiesPrivate;
	const TSharedPtr<FExtendedAtlassianClient> Client =
		FUnrealExtendedAtlassianModule::GetClient();
	if (!Client.IsValid())
	{
		OnComplete.ExecuteIfBound(
			false,
			FExtendedAtlassianContentProperty(),
			NotReady());
		return;
	}
	const FString Url = FString::Printf(
		TEXT("%s/pages/%s/properties?key=%s&limit=1"),
		*BaseUrl(),
		*PageId,
		*FGenericPlatformHttp::UrlEncode(Key));
	Client->Get(
		Url,
		FExtendedAtlassianResponseDelegate::CreateLambda(
			[PageId, OnComplete](const FExtendedAtlassianResponse& Response)
			{
				if (!Response.IsSuccess() || !Response.Object.IsValid())
				{
					OnComplete.ExecuteIfBound(
						false,
						FExtendedAtlassianContentProperty(),
						Response.Error);
					return;
				}
				const TArray<TSharedPtr<FJsonValue>>* Results = nullptr;
				if (!Response.Object->TryGetArrayField(TEXT("results"), Results)
					|| Results->IsEmpty())
				{
					OnComplete.ExecuteIfBound(
						true,
						FExtendedAtlassianContentProperty(),
						FExtendedAtlassianError());
					return;
				}
				const TSharedPtr<FJsonObject>* Object = nullptr;
				if (!(*Results)[0]->TryGetObject(Object) || !Object->IsValid())
				{
					FExtendedAtlassianError Error;
					Error.Code = TEXT("UnexpectedResponse");
					Error.Message = TEXT("Confluence returned an invalid content property.");
					OnComplete.ExecuteIfBound(
						false,
						FExtendedAtlassianContentProperty(),
						Error);
					return;
				}
				FExtendedAtlassianContentProperty Summary = Parse(*Object);
				if (Summary.Id.IsEmpty())
				{
					FExtendedAtlassianError Error;
					Error.Code = TEXT("UnexpectedResponse");
					Error.Message = TEXT("Confluence omitted the content property id.");
					OnComplete.ExecuteIfBound(
						false,
						FExtendedAtlassianContentProperty(),
						Error);
					return;
				}
				GetById(PageId, Summary.Id, OnComplete);
			}));
}

FExtendedAtlassianContentProperty
FExtendedAtlassianConfluenceProperties::ParseProperty(
	const TSharedPtr<FJsonObject>& Object)
{
	return ExtendedAtlassianConfluencePropertiesPrivate::Parse(Object);
}

FString FExtendedAtlassianConfluenceProperties::BuildPropertyBody(
	const FString& Key,
	const TSharedRef<FJsonObject>& Value,
	int32 Version)
{
	return ExtendedAtlassianConfluencePropertiesPrivate::MakeBody(
		Key,
		Value,
		Version);
}

void FExtendedAtlassianConfluenceProperties::CreatePageProperty(
	const FString& PageId,
	const FString& Key,
	const TSharedRef<FJsonObject>& Value,
	FExtendedAtlassianContentPropertyDelegate OnComplete)
{
	using namespace ExtendedAtlassianConfluencePropertiesPrivate;
	const TSharedPtr<FExtendedAtlassianClient> Client =
		FUnrealExtendedAtlassianModule::GetClient();
	if (!Client.IsValid())
	{
		OnComplete.ExecuteIfBound(
			false,
			FExtendedAtlassianContentProperty(),
			NotReady());
		return;
	}
	Client->PostJson(
		FString::Printf(TEXT("%s/pages/%s/properties"), *BaseUrl(), *PageId),
		MakeBody(Key, Value, 0),
		FExtendedAtlassianResponseDelegate::CreateLambda(
			[OnComplete](const FExtendedAtlassianResponse& Response)
			{
				if (!Response.IsSuccess() || !Response.Object.IsValid())
				{
					OnComplete.ExecuteIfBound(
						false,
						FExtendedAtlassianContentProperty(),
						Response.Error);
					return;
				}
				OnComplete.ExecuteIfBound(
					true,
					Parse(Response.Object),
					FExtendedAtlassianError());
			}));
}

void FExtendedAtlassianConfluenceProperties::UpdatePageProperty(
	const FString& PageId,
	const FExtendedAtlassianContentProperty& Current,
	const TSharedRef<FJsonObject>& Value,
	FExtendedAtlassianContentPropertyDelegate OnComplete)
{
	using namespace ExtendedAtlassianConfluencePropertiesPrivate;
	const TSharedPtr<FExtendedAtlassianClient> Client =
		FUnrealExtendedAtlassianModule::GetClient();
	if (!Client.IsValid())
	{
		OnComplete.ExecuteIfBound(
			false,
			FExtendedAtlassianContentProperty(),
			NotReady());
		return;
	}
	Client->PutJson(
		FString::Printf(
			TEXT("%s/pages/%s/properties/%s"),
			*BaseUrl(),
			*PageId,
			*Current.Id),
		MakeBody(Current.Key, Value, Current.Version + 1),
		FExtendedAtlassianResponseDelegate::CreateLambda(
			[OnComplete](const FExtendedAtlassianResponse& Response)
			{
				if (!Response.IsSuccess() || !Response.Object.IsValid())
				{
					OnComplete.ExecuteIfBound(
						false,
						FExtendedAtlassianContentProperty(),
						Response.Error);
					return;
				}
				OnComplete.ExecuteIfBound(
					true,
					Parse(Response.Object),
					FExtendedAtlassianError());
			}));
}

void FExtendedAtlassianConfluenceProperties::UpsertPageProperty(
	const FString& PageId,
	const FString& Key,
	const TSharedRef<FJsonObject>& Value,
	FExtendedAtlassianContentPropertyDelegate OnComplete)
{
	GetPageProperty(
		PageId,
		Key,
		FExtendedAtlassianContentPropertyDelegate::CreateLambda(
			[PageId, Key, Value, OnComplete](
				bool bSuccess,
				const FExtendedAtlassianContentProperty& Current,
				const FExtendedAtlassianError& Error)
			{
				if (!bSuccess)
				{
					OnComplete.ExecuteIfBound(
						false,
						FExtendedAtlassianContentProperty(),
						Error);
					return;
				}
				if (Current.IsValid())
				{
					UpdatePageProperty(PageId, Current, Value, OnComplete);
				}
				else
				{
					CreatePageProperty(PageId, Key, Value, OnComplete);
				}
			}));
}
