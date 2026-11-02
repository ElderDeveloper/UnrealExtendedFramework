// Copyright Moon Punch Games. All Rights Reserved.

#include "Http/EFHttpLibrary.h"

#include "Http/EFHttpClient.h"

FEFHttpRequest UEFHttpLibrary::MakeHttpRequest(const FString& Url, EEFHttpVerb Verb)
{
	FEFHttpRequest Request;
	Request.Url = Url;
	Request.Verb = Verb;
	return Request;
}

FEFHttpRequest UEFHttpLibrary::MakeJsonHttpRequest(const FString& Url, EEFHttpVerb Verb, const FEFJsonObject& Body)
{
	FEFHttpRequest Request = MakeHttpRequest(Url, Verb);
	Request.ContentType = TEXT("application/json");
	Request.Headers.Add(TEXT("Accept"), TEXT("application/json"));
	if (Body.IsValid())
	{
		Request.Body = EFJson::ObjectToString(Body.Object);
	}
	return Request;
}

FEFHttpRequest UEFHttpLibrary::MakeFormHttpRequest(const FString& Url, EEFHttpVerb Verb, const TMap<FString, FString>& Fields)
{
	FEFHttpRequest Request = MakeHttpRequest(Url, Verb);
	Request.ContentType = TEXT("application/x-www-form-urlencoded");
	Request.Body = EFHttp::BuildFormBody(Fields);
	return Request;
}

FEFHttpRequest UEFHttpLibrary::MakeCustomVerbHttpRequest(const FString& Url, const FString& CustomVerb)
{
	FEFHttpRequest Request = MakeHttpRequest(Url, EEFHttpVerb::Custom);
	Request.CustomVerb = CustomVerb.TrimStartAndEnd();
	return Request;
}

FEFHttpRequest UEFHttpLibrary::AddRequestTag(const FEFHttpRequest& Request, FName Tag)
{
	FEFHttpRequest Out = Request;
	if (!Tag.IsNone())
	{
		Out.Tags.AddUnique(Tag);
	}
	return Out;
}

bool UEFHttpLibrary::ResponseHasTag(const FEFHttpResponse& Response, FName Tag)
{
	return !Tag.IsNone() && Response.Tags.Contains(Tag);
}

FEFHttpRequest UEFHttpLibrary::SetRequestHeader(const FEFHttpRequest& Request, const FString& Name, const FString& Value)
{
	FEFHttpRequest Out = Request;
	if (!Name.IsEmpty())
	{
		Out.Headers.Add(Name, Value);
	}
	return Out;
}

FEFHttpRequest UEFHttpLibrary::SetBearerToken(const FEFHttpRequest& Request, const FString& Token)
{
	return SetRequestHeader(Request, TEXT("Authorization"), TEXT("Bearer ") + Token);
}

FString UEFHttpLibrary::GetResponseText(const FEFHttpResponse& Response)
{
	return EFHttp::GetResponseText(Response);
}

bool UEFHttpLibrary::GetResponseJson(const FEFHttpResponse& Response, FEFJsonObject& OutObject)
{
	TSharedPtr<FJsonObject> Parsed;
	const bool bParsed = EFJson::ParseObject(EFHttp::GetResponseText(Response), Parsed);
	OutObject = FEFJsonObject(Parsed);
	return bParsed;
}

bool UEFHttpLibrary::GetResponseJsonValue(const FEFHttpResponse& Response, FEFJsonValue& OutValue)
{
	TSharedPtr<FJsonValue> Parsed;
	const bool bParsed = EFJson::ParseValue(EFHttp::GetResponseText(Response), Parsed);
	OutValue = FEFJsonValue(Parsed);
	return bParsed;
}

bool UEFHttpLibrary::GetResponseHeader(const FEFHttpResponse& Response, const FString& Name, FString& OutValue)
{
	const FString* Value = Response.Headers.Find(Name);
	OutValue = Value ? *Value : FString();
	return Value != nullptr;
}

bool UEFHttpLibrary::IsHttpSuccess(const FEFHttpResponse& Response)
{
	return Response.IsSuccess();
}

bool UEFHttpLibrary::IsSuccessStatusCode(int32 StatusCode)
{
	return EFHttp::IsSuccessStatus(StatusCode);
}

FString UEFHttpLibrary::HttpVerbToString(EEFHttpVerb Verb)
{
	return EFHttp::VerbToString(Verb);
}

FString UEFHttpLibrary::UrlEncode(const FString& Value)
{
	return EFHttp::UrlEncode(Value);
}

FString UEFHttpLibrary::BuildFormBody(const TMap<FString, FString>& Fields)
{
	return EFHttp::BuildFormBody(Fields);
}

FString UEFHttpLibrary::BuildUrl(const FString& BaseUrl, const TMap<FString, FString>& QueryParameters)
{
	return EFHttp::BuildUrl(BaseUrl, QueryParameters);
}
