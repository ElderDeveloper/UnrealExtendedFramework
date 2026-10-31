// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Http/EFHttpClient.h"
#include "Http/EFHttpTypes.h"

/** One request the mock received. */
struct FEFHttpMockRecordedRequest
{
	EEFHttpVerb Verb = EEFHttpVerb::Get;
	FString Url;
	TMap<FString, FString> Headers;
	FString Body;
	TArray<uint8> BodyBytes;
};

/**
 * Canned HTTP answers for offline tests and demos.
 *
 * Rules are checked in the order they were added; the first rule whose verb matches and whose text
 * appears in the URL answers. A request no rule matches gets 404 with body "no mock rule". Answers
 * arrive on the next core ticker tick, like a real response would.
 *
 *     FEFScopedHttpMock Mock;
 *     Mock->AddResponse(EEFHttpVerb::Post, TEXT("/Client/LoginWithCustomID"), 200, TEXT("{\"code\":200,\"data\":{...}}"));
 *     ... run the code under test, wait a frame or two ...
 *     TestEqual(TEXT("one login call"), Mock->CountRequests(TEXT("LoginWithCustomID")), 1);
 */
class EXTENDEDFOUNDATION_API FEFHttpMockTransport : public IEFHttpTransport
{
public:
	/** Answers with Status, Body and Headers. With bOnce the rule is used up after one match. */
	FEFHttpMockTransport& AddResponse(EEFHttpVerb Verb, const FString& UrlContains, int32 Status, const FString& Body = FString(),
		const TMap<FString, FString>& Headers = TMap<FString, FString>(), bool bOnce = false);

	/** Answers with no response at all: Result is ConnectionError, or TimedOut with bTimeout. */
	FEFHttpMockTransport& AddFailure(EEFHttpVerb Verb, const FString& UrlContains, bool bTimeout = false, bool bOnce = false);

	/** Every request received so far, oldest first. */
	const TArray<FEFHttpMockRecordedRequest>& GetRequests() const { return Requests; }

	/** How many received requests have UrlContains in their URL. */
	int32 CountRequests(const FString& UrlContains) const;

	void Reset();

	virtual void Send(const FEFHttpRequest& Request, const FString& FinalUrl, TFunction<void(const FEFHttpResponse&)> OnResponse) override;

private:
	struct FRule
	{
		EEFHttpVerb Verb = EEFHttpVerb::Get;
		FString UrlContains;
		FEFHttpResponse Response;
		bool bOnce = false;
	};

	TArray<FRule> Rules;
	TArray<FEFHttpMockRecordedRequest> Requests;
};

/** Installs an FEFHttpMockTransport for its lifetime and restores the previous transport afterwards. */
class EXTENDEDFOUNDATION_API FEFScopedHttpMock
{
public:
	FEFScopedHttpMock();
	~FEFScopedHttpMock();

	FEFScopedHttpMock(const FEFScopedHttpMock&) = delete;
	FEFScopedHttpMock& operator=(const FEFScopedHttpMock&) = delete;

	FEFHttpMockTransport* operator->() const { return &Mock.Get(); }
	FEFHttpMockTransport& Get() const { return Mock.Get(); }
	TSharedRef<FEFHttpMockTransport> GetShared() const { return Mock; }

private:
	TSharedRef<FEFHttpMockTransport> Mock;
	TSharedPtr<IEFHttpTransport> Previous;
};
