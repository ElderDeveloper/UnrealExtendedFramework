// Copyright Moon Punch Games. All Rights Reserved.

#include "Backend/Http/EFHttpLibrary.h"

#include "Backend/Http/EFHttpClient.h"

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

bool UEFHttpLibrary::GetResponseJson(const FEFHttpResponse& Response, FEFJsonObject& OutObject)
{
	TSharedPtr<FJsonObject> Parsed;
	const bool bParsed = EFJson::ParseObject(Response.Body, Parsed);
	OutObject = FEFJsonObject(Parsed);
	return bParsed;
}

bool UEFHttpLibrary::GetResponseJsonValue(const FEFHttpResponse& Response, FEFJsonValue& OutValue)
{
	TSharedPtr<FJsonValue> Parsed;
	const bool bParsed = EFJson::ParseValue(Response.Body, Parsed);
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

FString UEFHttpLibrary::BuildUrl(const FString& BaseUrl, const TMap<FString, FString>& QueryParameters)
{
	return EFHttp::BuildUrl(BaseUrl, QueryParameters);
}
