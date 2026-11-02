// Copyright Moon Punch Games. All Rights Reserved.

#include "Http/EFHttpMock.h"

#include "Containers/Ticker.h"

FEFHttpMockTransport& FEFHttpMockTransport::AddResponse(EEFHttpVerb Verb, const FString& UrlContains, int32 Status, const FString& Body,
	const TMap<FString, FString>& Headers, bool bOnce)
{
	FRule& Rule = Rules.AddDefaulted_GetRef();
	Rule.Verb = Verb;
	Rule.UrlContains = UrlContains;
	Rule.bOnce = bOnce;
	Rule.Response.Result = EFHttp::IsSuccessStatus(Status) ? EEFHttpResult::Success : EEFHttpResult::HttpError;
	Rule.Response.StatusCode = Status;
	Rule.Response.Body = Body;
	Rule.Response.Headers = Headers;
	if (const FString* ContentType = Headers.Find(TEXT("Content-Type")))
	{
		Rule.Response.ContentType = *ContentType;
	}
	return *this;
}

FEFHttpMockTransport& FEFHttpMockTransport::AddBinaryResponse(EEFHttpVerb Verb, const FString& UrlContains, int32 Status,
	const TArray<uint8>& BodyBytes, const FString& ContentType, bool bOnce)
{
	TMap<FString, FString> Headers;
	if (!ContentType.IsEmpty())
	{
		Headers.Add(TEXT("Content-Type"), ContentType);
	}
	AddResponse(Verb, UrlContains, Status, FString(), Headers, bOnce);
	Rules.Last().Response.BodyBytes = BodyBytes;
	return *this;
}

FEFHttpMockTransport& FEFHttpMockTransport::AddFailure(EEFHttpVerb Verb, const FString& UrlContains, bool bTimeout, bool bOnce)
{
	FRule& Rule = Rules.AddDefaulted_GetRef();
	Rule.Verb = Verb;
	Rule.UrlContains = UrlContains;
	Rule.bOnce = bOnce;
	Rule.Response.Result = bTimeout ? EEFHttpResult::TimedOut : EEFHttpResult::ConnectionError;
	Rule.Response.ErrorMessage = bTimeout ? TEXT("Mock timeout.") : TEXT("Mock connection failure.");
	return *this;
}

int32 FEFHttpMockTransport::CountRequests(const FString& UrlContains) const
{
	int32 Count = 0;
	for (const FEFHttpMockRecordedRequest& Recorded : Requests)
	{
		if (Recorded.Url.Contains(UrlContains))
		{
			++Count;
		}
	}
	return Count;
}

void FEFHttpMockTransport::Reset()
{
	Rules.Reset();
	Requests.Reset();
}

void FEFHttpMockTransport::Send(const FEFHttpRequest& Request, const FString& FinalUrl, TFunction<void(const FEFHttpResponse&)> OnResponse)
{
	FEFHttpMockRecordedRequest& Recorded = Requests.AddDefaulted_GetRef();
	Recorded.Verb = Request.Verb;
	Recorded.VerbString = EFHttp::GetVerbString(Request);
	Recorded.Url = FinalUrl;
	Recorded.Headers = Request.Headers;
	Recorded.Body = Request.Body;
	Recorded.BodyBytes = Request.BodyBytes;

	FEFHttpResponse Response;
	Response.Result = EEFHttpResult::HttpError;
	Response.StatusCode = 404;
	Response.Body = TEXT("no mock rule");

	for (int32 Index = 0; Index < Rules.Num(); ++Index)
	{
		const FRule& Rule = Rules[Index];
		if (Rule.Verb == Request.Verb && FinalUrl.Contains(Rule.UrlContains))
		{
			Response = Rule.Response;
			if (Rule.bOnce)
			{
				Rules.RemoveAt(Index);
			}
			break;
		}
	}

	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Response, OnResponse](float)
	{
		OnResponse(Response);
		return false;
	}));
}

FEFScopedHttpMock::FEFScopedHttpMock()
	: Mock(MakeShared<FEFHttpMockTransport>())
	, Previous(EFHttp::GetTransportOverride())
{
	EFHttp::SetTransportOverride(Mock);
}

FEFScopedHttpMock::~FEFScopedHttpMock()
{
	EFHttp::SetTransportOverride(Previous);
}
