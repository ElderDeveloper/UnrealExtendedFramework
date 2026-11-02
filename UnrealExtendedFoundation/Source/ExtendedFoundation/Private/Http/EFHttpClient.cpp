// Copyright Moon Punch Games. All Rights Reserved.

#include "Http/EFHttpClient.h"

#include "CoreGlobals.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"

DEFINE_LOG_CATEGORY(LogEFHttp);

namespace EFHttpPrivate
{
	constexpr float MaxBackoffSeconds = 30.f;
	constexpr float MaxRetryAfterSeconds = 60.f;

	/** Operations started and not yet finished, for Extended.Status. Game thread only. */
	int32 ActiveOperationCount = 0;

	FString DecodeUtf8(const TArray<uint8>& Bytes)
	{
		const FUTF8ToTCHAR Text(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
		return FString::ConstructFromPtrSize(Text.Get(), Text.Length());
	}
}

int32 FEFHttpOperation::GetActiveCount()
{
	return EFHttpPrivate::ActiveOperationCount;
}

// ─── FEFHttpOperation ───────────────────────────────────────────────────────────

FEFHttpOperation::FEFHttpOperation(const FEFHttpRequest& InRequest, FOnComplete InOnComplete)
	: Request(InRequest)
	, OnComplete(MoveTemp(InOnComplete))
{
}

FEFHttpOperation::~FEFHttpOperation()
{
	// Tickers hold a strong reference, so none can be pending here; an in-flight request can be
	// if its completion delegate was already unbound.
	ReleaseHttpRequest();
}

TSharedRef<FEFHttpOperation> FEFHttpOperation::Start(const FEFHttpRequest& InRequest, FOnComplete InOnComplete)
{
	TSharedRef<FEFHttpOperation> Operation = MakeShared<FEFHttpOperation>(InRequest, MoveTemp(InOnComplete));
	Operation->StartTimeSeconds = FPlatformTime::Seconds();
	++EFHttpPrivate::ActiveOperationCount;
	Operation->FinalUrl = EFHttp::BuildUrl(InRequest.Url, InRequest.QueryParameters);

	// Anything that completes while Start() is still running is deferred to the next tick, so
	// callers can always store the returned operation before its callback runs.
	Operation->bStarting = true;

	if (!EFHttp::HasHttpScheme(Operation->FinalUrl))
	{
		FEFHttpResponse Response;
		Response.Result = EEFHttpResult::InvalidRequest;
		Response.Url = Operation->FinalUrl;
		Response.ErrorMessage = InRequest.Url.IsEmpty()
			? FString(TEXT("The request has no URL."))
			: FString::Printf(TEXT("'%s' is not an http:// or https:// URL."), *EFHttp::RedactUrlForLog(InRequest.Url));
		Operation->Finish(MoveTemp(Response));
	}
	else if (InRequest.Verb == EEFHttpVerb::Custom && !EFHttp::IsValidCustomVerb(InRequest.CustomVerb))
	{
		FEFHttpResponse Response;
		Response.Result = EEFHttpResult::InvalidRequest;
		Response.Url = Operation->FinalUrl;
		Response.ErrorMessage = InRequest.CustomVerb.IsEmpty()
			? FString(TEXT("The request's verb is Custom but it has no CustomVerb."))
			: FString::Printf(TEXT("'%s' is not a valid HTTP verb."), *InRequest.CustomVerb);
		Operation->Finish(MoveTemp(Response));
	}
	else
	{
		Operation->BeginAttempt();
	}

	Operation->bStarting = false;
	return Operation;
}

void FEFHttpOperation::Cancel(bool bNotify)
{
	if (bFinished)
	{
		return;
	}

	const TSharedRef<FEFHttpOperation> KeepAlive = AsShared();

	if (PendingTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PendingTickerHandle);
		PendingTickerHandle.Reset();
	}
	ReleaseHttpRequest();

	if (!bNotify)
	{
		bFinished = true;
		--EFHttpPrivate::ActiveOperationCount;
		OnComplete = nullptr;
		UE_LOG(LogEFHttp, Verbose, TEXT("%s %s cancelled."), *EFHttp::GetVerbString(Request), *EFHttp::RedactUrlForLog(FinalUrl));
		return;
	}

	FEFHttpResponse Response;
	Response.Result = EEFHttpResult::Cancelled;
	Response.Url = FinalUrl;
	Response.ErrorMessage = TEXT("The request was cancelled.");
	Finish(MoveTemp(Response));
}

void FEFHttpOperation::BeginAttempt()
{
	if (bFinished)
	{
		return;
	}

	++AttemptCount;
	const int32 AttemptNumber = AttemptCount;

	// A transport override (tests, offline mocks) replaces the engine's HTTP module entirely.
	if (const TSharedPtr<IEFHttpTransport> Transport = EFHttp::GetTransportOverride())
	{
		UE_LOG(LogEFHttp, Verbose, TEXT("%s %s (attempt %d, transport override)"), *EFHttp::GetVerbString(Request), *EFHttp::RedactUrlForLog(FinalUrl), AttemptNumber);

		const TSharedRef<FEFHttpOperation> Self = AsShared();
		Transport->Send(Request, FinalUrl, [Self, AttemptNumber](const FEFHttpResponse& Response)
		{
			Self->HandleTransportResponse(AttemptNumber, Response);
		});
		return;
	}

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> NewRequest = FHttpModule::Get().CreateRequest();
	NewRequest->SetURL(FinalUrl);
	NewRequest->SetVerb(EFHttp::GetVerbString(Request));

	for (const TPair<FString, FString>& Header : Request.Headers)
	{
		NewRequest->SetHeader(Header.Key, Header.Value);
	}

	const bool bHasBody = Request.BodyBytes.Num() > 0 || !Request.Body.IsEmpty();
	if (bHasBody && !Request.ContentType.IsEmpty() && !Request.Headers.Contains(TEXT("Content-Type")))
	{
		NewRequest->SetHeader(TEXT("Content-Type"), Request.ContentType);
	}
	if (Request.BodyBytes.Num() > 0)
	{
		NewRequest->SetContent(Request.BodyBytes);
	}
	else if (!Request.Body.IsEmpty())
	{
		NewRequest->SetContentAsString(Request.Body);
	}

	if (Request.TimeoutSeconds > 0.f)
	{
		NewRequest->SetTimeout(Request.TimeoutSeconds);
	}

	// The delegate holds the operation alive while the request is in flight. HandleAttemptComplete
	// drops the request pointer, which breaks the cycle once the HTTP module releases the request.
	const TSharedRef<FEFHttpOperation> Self = AsShared();
	NewRequest->OnProcessRequestComplete().BindLambda(
		[Self, AttemptNumber](FHttpRequestPtr CompletedRequest, FHttpResponsePtr Response, bool bConnectedSuccessfully)
		{
			Self->HandleAttemptComplete(AttemptNumber, CompletedRequest, Response, bConnectedSuccessfully);
		});

	HttpRequest = NewRequest;

	UE_LOG(LogEFHttp, Verbose, TEXT("%s %s (attempt %d)"), *EFHttp::GetVerbString(Request), *EFHttp::RedactUrlForLog(FinalUrl), AttemptNumber);

	if (!NewRequest->ProcessRequest())
	{
		// Platforms differ on whether a request that fails to start still reports through its
		// completion delegate. If nothing has arrived by the next tick, fail the attempt here.
		if (!bFinished && AttemptCount == AttemptNumber && HttpRequest.Get() == &NewRequest.Get())
		{
			PendingTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
				[Self, AttemptNumber](float)
				{
					Self->PendingTickerHandle.Reset();
					if (!Self->bFinished && Self->AttemptCount == AttemptNumber && Self->HttpRequest.IsValid())
					{
						const FHttpRequestPtr FailedRequest = Self->HttpRequest;
						Self->ReleaseHttpRequest();
						Self->HandleAttemptComplete(AttemptNumber, FailedRequest, nullptr, false);
					}
					return false;
				}));
		}
	}
}

void FEFHttpOperation::HandleAttemptComplete(int32 AttemptNumber, FHttpRequestPtr CompletedRequest, FHttpResponsePtr HttpResponse, bool bConnectedSuccessfully)
{
	if (bFinished || AttemptNumber != AttemptCount)
	{
		return;
	}

	const TSharedRef<FEFHttpOperation> KeepAlive = AsShared();

	// Only drop the pointer: this may run inside the request's own completion delegate, and
	// unbinding it here would destroy the lambda that is executing.
	HttpRequest.Reset();

	FEFHttpResponse Response;
	Response.Url = FinalUrl;

	if (bConnectedSuccessfully && HttpResponse.IsValid())
	{
		Response.StatusCode = HttpResponse->GetResponseCode();
		Response.ContentType = HttpResponse->GetContentType();
		const EEFHttpResponseBodyMode BodyMode = EFHttp::ResolveResponseBodyMode(Request.ResponseBodyMode, Response.ContentType);
		if (BodyMode != EEFHttpResponseBodyMode::Bytes)
		{
			Response.Body = HttpResponse->GetContentAsString();
		}
		if (BodyMode != EEFHttpResponseBodyMode::Text)
		{
			Response.BodyBytes = HttpResponse->GetContent();
		}
		Response.Headers = EFHttp::ParseHeaderLines(HttpResponse->GetAllHeaders());

		if (EFHttp::IsSuccessStatus(Response.StatusCode))
		{
			Response.Result = EEFHttpResult::Success;
		}
		else
		{
			Response.Result = EEFHttpResult::HttpError;
			Response.ErrorMessage = FString::Printf(TEXT("The server answered HTTP %d."), Response.StatusCode);
		}
	}
	else
	{
		const EHttpFailureReason Reason = CompletedRequest.IsValid() ? CompletedRequest->GetFailureReason() : EHttpFailureReason::ConnectionError;
		switch (Reason)
		{
		case EHttpFailureReason::TimedOut:
			Response.Result = EEFHttpResult::TimedOut;
			Response.ErrorMessage = FString::Printf(TEXT("No response within %.1f seconds."), Request.TimeoutSeconds);
			break;
		case EHttpFailureReason::Cancelled:
			Response.Result = EEFHttpResult::Cancelled;
			Response.ErrorMessage = TEXT("The request was cancelled.");
			break;
		default:
			Response.Result = EEFHttpResult::ConnectionError;
			Response.ErrorMessage = TEXT("Could not connect to the server.");
			break;
		}
	}

	ProcessAttemptResponse(MoveTemp(Response));
}

void FEFHttpOperation::HandleTransportResponse(int32 AttemptNumber, FEFHttpResponse Response)
{
	if (bFinished || AttemptNumber != AttemptCount)
	{
		return;
	}

	const TSharedRef<FEFHttpOperation> KeepAlive = AsShared();

	Response.Url = FinalUrl;

	// A transport may answer with text or with bytes; keep the forms the request asked for, as a real response does.
	const EEFHttpResponseBodyMode BodyMode = EFHttp::ResolveResponseBodyMode(Request.ResponseBodyMode, Response.ContentType);
	const bool bKeepText = BodyMode != EEFHttpResponseBodyMode::Bytes;
	const bool bKeepBytes = BodyMode != EEFHttpResponseBodyMode::Text;
	if (bKeepText && Response.Body.IsEmpty() && Response.BodyBytes.Num() > 0)
	{
		Response.Body = EFHttpPrivate::DecodeUtf8(Response.BodyBytes);
	}
	if (bKeepBytes && Response.BodyBytes.Num() == 0 && !Response.Body.IsEmpty())
	{
		const FTCHARToUTF8 Utf8(*Response.Body);
		Response.BodyBytes.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	}
	if (!bKeepText)
	{
		Response.Body.Empty();
	}
	if (!bKeepBytes)
	{
		Response.BodyBytes.Empty();
	}

	if (Response.Result == EEFHttpResult::Success || Response.Result == EEFHttpResult::HttpError)
	{
		// A transport that produced a status decides nothing else: the status does.
		if (EFHttp::IsSuccessStatus(Response.StatusCode))
		{
			Response.Result = EEFHttpResult::Success;
		}
		else
		{
			Response.Result = EEFHttpResult::HttpError;
			if (Response.ErrorMessage.IsEmpty())
			{
				Response.ErrorMessage = FString::Printf(TEXT("The server answered HTTP %d."), Response.StatusCode);
			}
		}
	}

	ProcessAttemptResponse(MoveTemp(Response));
}

void FEFHttpOperation::ProcessAttemptResponse(FEFHttpResponse Response)
{
	const bool bRetryableResult =
		Response.Result == EEFHttpResult::ConnectionError
		|| Response.Result == EEFHttpResult::TimedOut
		|| (Request.bRetryOnServerErrors && Response.Result == EEFHttpResult::HttpError && EFHttp::IsRetryableStatus(Response.StatusCode));
	const bool bVerbAllowsRetry = EFHttp::IsIdempotent(Request.Verb) || Request.bAllowRetryForNonIdempotentVerbs;

	if (bRetryableResult && bVerbAllowsRetry && AttemptCount <= Request.MaxRetries)
	{
		ScheduleRetry(Response);
		return;
	}

	Finish(MoveTemp(Response));
}

void FEFHttpOperation::ScheduleRetry(const FEFHttpResponse& FailedResponse)
{
	const FString* RetryAfter = FailedResponse.Headers.Find(TEXT("Retry-After"));
	const float Delay = EFHttp::ComputeRetryDelay(AttemptCount, Request.RetryDelaySeconds, RetryAfter ? EFHttp::ParseRetryAfterSeconds(*RetryAfter) : 0.f);

	UE_LOG(LogEFHttp, Log, TEXT("%s %s: %s Retrying in %.2fs (retry %d of %d)."),
		*EFHttp::GetVerbString(Request), *EFHttp::RedactUrlForLog(FinalUrl), *FailedResponse.ErrorMessage, Delay, AttemptCount, Request.MaxRetries);

	const TSharedRef<FEFHttpOperation> Self = AsShared();
	PendingTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[Self](float)
		{
			Self->PendingTickerHandle.Reset();
			Self->BeginAttempt();
			return false;
		}), Delay);
}

void FEFHttpOperation::FinishOnNextTick(FEFHttpResponse Response)
{
	const TSharedRef<FEFHttpOperation> Self = AsShared();
	PendingTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[Self, Response](float)
		{
			Self->PendingTickerHandle.Reset();
			Self->Finish(Response);
			return false;
		}));
}

void FEFHttpOperation::Finish(FEFHttpResponse Response)
{
	if (bFinished)
	{
		return;
	}

	// Never call back from inside Start(), nor from inside package loading: MoviePlayer ticks the
	// HTTP manager while a blocking load runs on the game thread, where game code can assert. The
	// next tick checks again, so a callback waits until loading has let go of the game thread.
	if (bStarting || IsInAsyncLoadingThread())
	{
		FinishOnNextTick(MoveTemp(Response));
		return;
	}

	bFinished = true;
	--EFHttpPrivate::ActiveOperationCount;

	Response.Attempts = AttemptCount;
	Response.ElapsedSeconds = static_cast<float>(FPlatformTime::Seconds() - StartTimeSeconds);
	Response.Tags = Request.Tags;
	if (Response.Url.IsEmpty())
	{
		Response.Url = FinalUrl;
	}

	const FString Verb = EFHttp::GetVerbString(Request);
	const FString LogUrl = EFHttp::RedactUrlForLog(FinalUrl);
	switch (Response.Result)
	{
	case EEFHttpResult::Success:
		UE_LOG(LogEFHttp, Verbose, TEXT("%s %s -> %d (%.2fs)"), *Verb, *LogUrl, Response.StatusCode, Response.ElapsedSeconds);
		break;
	case EEFHttpResult::Cancelled:
		UE_LOG(LogEFHttp, Verbose, TEXT("%s %s cancelled."), *Verb, *LogUrl);
		break;
	default:
		if (Request.bLogFailuresAsWarnings)
		{
			UE_LOG(LogEFHttp, Warning, TEXT("%s %s failed: %s (attempts: %d, %.2fs)"), *Verb, *LogUrl, *Response.ErrorMessage, Response.Attempts, Response.ElapsedSeconds);
		}
		else
		{
			UE_LOG(LogEFHttp, Verbose, TEXT("%s %s failed: %s (attempts: %d, %.2fs)"), *Verb, *LogUrl, *Response.ErrorMessage, Response.Attempts, Response.ElapsedSeconds);
		}
		break;
	}

	// Move the callback out first: it may release the last reference to this operation.
	FOnComplete Callback = MoveTemp(OnComplete);
	OnComplete = nullptr;
	if (Callback)
	{
		Callback(Response);
	}
}

void FEFHttpOperation::ReleaseHttpRequest()
{
	if (!HttpRequest.IsValid())
	{
		return;
	}

	const FHttpRequestPtr Released = MoveTemp(HttpRequest);
	HttpRequest.Reset();
	Released->OnProcessRequestComplete().Unbind();
	Released->CancelRequest();
}

// ─── Transport override ─────────────────────────────────────────────────────────

namespace EFHttpPrivate
{
	TSharedPtr<IEFHttpTransport>& TransportOverride()
	{
		static TSharedPtr<IEFHttpTransport> Override;
		return Override;
	}
}

void EFHttp::SetTransportOverride(TSharedPtr<IEFHttpTransport> Transport)
{
	check(IsInGameThread());
	EFHttpPrivate::TransportOverride() = MoveTemp(Transport);
}

TSharedPtr<IEFHttpTransport> EFHttp::GetTransportOverride()
{
	return EFHttpPrivate::TransportOverride();
}

// ─── EFHttp helpers ─────────────────────────────────────────────────────────────

const TCHAR* EFHttp::VerbToString(EEFHttpVerb Verb)
{
	switch (Verb)
	{
	case EEFHttpVerb::Get:     return TEXT("GET");
	case EEFHttpVerb::Post:    return TEXT("POST");
	case EEFHttpVerb::Put:     return TEXT("PUT");
	case EEFHttpVerb::Patch:   return TEXT("PATCH");
	case EEFHttpVerb::Delete:  return TEXT("DELETE");
	case EEFHttpVerb::Head:    return TEXT("HEAD");
	case EEFHttpVerb::Options: return TEXT("OPTIONS");
	case EEFHttpVerb::Custom:  return TEXT("CUSTOM");
	default:                   return TEXT("GET");
	}
}

FString EFHttp::GetVerbString(const FEFHttpRequest& Request)
{
	return Request.Verb == EEFHttpVerb::Custom ? Request.CustomVerb.ToUpper() : FString(VerbToString(Request.Verb));
}

bool EFHttp::IsValidCustomVerb(const FString& CustomVerb)
{
	if (CustomVerb.IsEmpty())
	{
		return false;
	}
	for (const TCHAR Character : CustomVerb)
	{
		if (!FChar::IsAlnum(Character) && Character != TEXT('-') && Character != TEXT('_'))
		{
			return false;
		}
	}
	return true;
}

bool EFHttp::IsIdempotent(EEFHttpVerb Verb)
{
	return Verb != EEFHttpVerb::Post && Verb != EEFHttpVerb::Patch && Verb != EEFHttpVerb::Custom;
}

bool EFHttp::IsSuccessStatus(int32 StatusCode)
{
	return StatusCode >= 200 && StatusCode <= 299;
}

bool EFHttp::IsRetryableStatus(int32 StatusCode)
{
	return StatusCode == 408 || StatusCode == 429 || (StatusCode >= 500 && StatusCode <= 599);
}

bool EFHttp::HasHttpScheme(const FString& Url)
{
	static const FString Http = TEXT("http://");
	static const FString Https = TEXT("https://");
	return (Url.StartsWith(Https, ESearchCase::IgnoreCase) && Url.Len() > Https.Len())
		|| (Url.StartsWith(Http, ESearchCase::IgnoreCase) && Url.Len() > Http.Len());
}

FString EFHttp::UrlEncode(const FString& Value)
{
	return FGenericPlatformHttp::UrlEncode(Value);
}

FString EFHttp::BuildFormBody(const TMap<FString, FString>& Fields)
{
	FString Body;
	for (const TPair<FString, FString>& Field : Fields)
	{
		if (!Body.IsEmpty())
		{
			Body += TEXT("&");
		}
		Body += UrlEncode(Field.Key);
		Body += TEXT("=");
		Body += UrlEncode(Field.Value);
	}
	return Body;
}

FString EFHttp::BuildUrl(const FString& BaseUrl, const TMap<FString, FString>& QueryParameters)
{
	if (QueryParameters.Num() == 0)
	{
		return BaseUrl;
	}

	FString Url = BaseUrl;
	FString Fragment;
	int32 FragmentStart = INDEX_NONE;
	if (Url.FindChar(TEXT('#'), FragmentStart))
	{
		Fragment = Url.Mid(FragmentStart);
		Url.LeftInline(FragmentStart);
	}

	int32 QueryStart = INDEX_NONE;
	const TCHAR* Separator = TEXT("?");
	if (Url.FindChar(TEXT('?'), QueryStart))
	{
		Separator = (Url.EndsWith(TEXT("?")) || Url.EndsWith(TEXT("&"))) ? TEXT("") : TEXT("&");
	}

	for (const TPair<FString, FString>& Parameter : QueryParameters)
	{
		Url += Separator;
		Url += UrlEncode(Parameter.Key);
		Url += TEXT("=");
		Url += UrlEncode(Parameter.Value);
		Separator = TEXT("&");
	}

	return Url + Fragment;
}

FString EFHttp::ResolveUrl(const FString& BaseUrl, const FString& Url)
{
	if (BaseUrl.IsEmpty() || HasHttpScheme(Url))
	{
		return Url;
	}
	if (Url.IsEmpty())
	{
		return BaseUrl;
	}

	FString Base = BaseUrl;
	while (Base.EndsWith(TEXT("/")))
	{
		Base.LeftChopInline(1);
	}

	FString Path = Url;
	while (Path.StartsWith(TEXT("/")))
	{
		Path.RightChopInline(1);
	}

	return Base + TEXT("/") + Path;
}

FString EFHttp::RedactUrlForLog(const FString& Url)
{
	FString Out = Url;

	int32 Cut = INDEX_NONE;
	int32 Position = INDEX_NONE;
	if (Out.FindChar(TEXT('?'), Position))
	{
		Cut = Position;
	}
	if (Out.FindChar(TEXT('#'), Position))
	{
		Cut = Cut == INDEX_NONE ? Position : FMath::Min(Cut, Position);
	}
	if (Cut != INDEX_NONE)
	{
		Out.LeftInline(Cut);
	}

	const int32 SchemeEnd = Out.Find(TEXT("://"));
	if (SchemeEnd != INDEX_NONE)
	{
		const int32 AuthorityStart = SchemeEnd + 3;
		int32 AuthorityEnd = Out.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, AuthorityStart);
		if (AuthorityEnd == INDEX_NONE)
		{
			AuthorityEnd = Out.Len();
		}

		const int32 UserInfoEnd = Out.Find(TEXT("@"), ESearchCase::CaseSensitive, ESearchDir::FromStart, AuthorityStart);
		if (UserInfoEnd != INDEX_NONE && UserInfoEnd < AuthorityEnd)
		{
			Out.RemoveAt(AuthorityStart, UserInfoEnd + 1 - AuthorityStart);
		}
	}

	return Out;
}

float EFHttp::ComputeRetryDelay(int32 RetryNumber, float BaseDelaySeconds, float RetryAfterSeconds)
{
	if (RetryAfterSeconds > 0.f)
	{
		return FMath::Min(RetryAfterSeconds, EFHttpPrivate::MaxRetryAfterSeconds);
	}

	const int32 Exponent = FMath::Clamp(RetryNumber - 1, 0, 16);
	return FMath::Min(FMath::Max(BaseDelaySeconds, 0.f) * static_cast<float>(1 << Exponent), EFHttpPrivate::MaxBackoffSeconds);
}

bool EFHttp::IsTextContentType(const FString& ContentType)
{
	FString MediaType = ContentType;
	FString Parameters;
	ContentType.Split(TEXT(";"), &MediaType, &Parameters);
	MediaType.TrimStartAndEndInline();
	MediaType.ToLowerInline();

	if (MediaType.IsEmpty() || MediaType.StartsWith(TEXT("text/")) || Parameters.Contains(TEXT("charset="), ESearchCase::IgnoreCase))
	{
		return true;
	}

	static const TCHAR* const TextSuffixes[] = { TEXT("+json"), TEXT("+xml"), TEXT("+yaml") };
	for (const TCHAR* Suffix : TextSuffixes)
	{
		if (MediaType.EndsWith(Suffix))
		{
			return true;
		}
	}

	static const TCHAR* const TextTypes[] = {
		TEXT("application/json"), TEXT("application/x-ndjson"), TEXT("application/xml"), TEXT("application/yaml"),
		TEXT("application/x-yaml"), TEXT("application/javascript"), TEXT("application/ecmascript"),
		TEXT("application/x-www-form-urlencoded"), TEXT("application/graphql"), TEXT("application/sql")
	};
	for (const TCHAR* TextType : TextTypes)
	{
		if (MediaType == TextType)
		{
			return true;
		}
	}
	return false;
}

EEFHttpResponseBodyMode EFHttp::ResolveResponseBodyMode(EEFHttpResponseBodyMode Mode, const FString& ContentType)
{
	if (Mode != EEFHttpResponseBodyMode::Auto)
	{
		return Mode;
	}
	return IsTextContentType(ContentType) ? EEFHttpResponseBodyMode::Text : EEFHttpResponseBodyMode::Bytes;
}

FString EFHttp::GetResponseText(const FEFHttpResponse& Response)
{
	if (!Response.Body.IsEmpty() || Response.BodyBytes.Num() == 0)
	{
		return Response.Body;
	}
	return EFHttpPrivate::DecodeUtf8(Response.BodyBytes);
}

TMap<FString, FString> EFHttp::ParseHeaderLines(const TArray<FString>& HeaderLines)
{
	TMap<FString, FString> Headers;
	for (const FString& Line : HeaderLines)
	{
		int32 Colon = INDEX_NONE;
		if (!Line.FindChar(TEXT(':'), Colon) || Colon == 0)
		{
			continue;
		}

		const FString Name = Line.Left(Colon).TrimStartAndEnd();
		const FString Value = Line.Mid(Colon + 1).TrimStartAndEnd();
		if (Name.IsEmpty())
		{
			continue;
		}

		if (FString* Existing = Headers.Find(Name))
		{
			Existing->Append(TEXT(", "));
			Existing->Append(Value);
		}
		else
		{
			Headers.Add(Name, Value);
		}
	}
	return Headers;
}

float EFHttp::ParseRetryAfterSeconds(const FString& HeaderValue)
{
	const FString Trimmed = HeaderValue.TrimStartAndEnd();
	if (Trimmed.IsEmpty() || !Trimmed.IsNumeric())
	{
		return 0.f;
	}

	const float Seconds = FCString::Atof(*Trimmed);
	return Seconds > 0.f ? Seconds : 0.f;
}
