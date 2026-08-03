// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianCredentials.h"
#include "ExtendedAtlassianTypes.h"

class FJsonObject;
class FJsonValue;
struct FExtendedAtlassianPendingRequest;

/** A completed request. Exactly one of Object/Array is populated when the body parsed as JSON. */
struct UNREALEXTENDEDATLASSIAN_API FExtendedAtlassianResponse
{
	int32 HttpStatus = 0;

	/** Raw response body, always populated when one was received. */
	FString Body;

	/** Parsed body when the top level is a JSON object. */
	TSharedPtr<FJsonObject> Object;

	/** Parsed body when the top level is a JSON array (e.g. /rest/api/3/priority). */
	TArray<TSharedPtr<FJsonValue>> Array;

	bool bIsArray = false;

	FExtendedAtlassianError Error;

	bool IsSuccess() const { return !Error.IsSet(); }
};

DECLARE_DELEGATE_OneParam(FExtendedAtlassianResponseDelegate, const FExtendedAtlassianResponse&);

/**
 * Async REST transport for Atlassian Cloud.
 *
 * Owns the credential cache and the verified-account state, applies HTTP Basic auth to every
 * request, retries rate-limited and transient failures with backoff, and normalises Jira's and
 * Confluence's differing error shapes into FExtendedAtlassianError.
 *
 * Completion delegates always fire on the game thread, so Slate callers need no marshalling.
 */
class UNREALEXTENDEDATLASSIAN_API FExtendedAtlassianClient : public TSharedFromThis<FExtendedAtlassianClient>
{
public:
	FExtendedAtlassianClient();

	DECLARE_MULTICAST_DELEGATE(FOnAuthStateChanged);

	// --- Credentials -------------------------------------------------------

	/** Reads credentials from the per-user store into the cache. Called once at module startup. */
	void ReloadCredentials();

	/** Caches and persists credentials, resetting the auth state to Unverified. */
	bool SetCredentials(const FExtendedAtlassianCredentials& InCredentials);

	/** Forgets and deletes stored credentials. */
	void ClearCredentials();

	bool HasCredentials() const { return Credentials.IsValid(); }
	const FExtendedAtlassianCredentials& GetCredentials() const { return Credentials; }

	EExtendedAtlassianAuthState GetAuthState() const { return AuthState; }
	const FExtendedAtlassianUser& GetVerifiedUser() const { return VerifiedUser; }
	const FExtendedAtlassianError& GetLastAuthError() const { return LastAuthError; }

	/** Fires whenever the auth state changes, so settings UI and tabs can refresh. */
	FOnAuthStateChanged& OnAuthStateChanged() { return AuthStateChanged; }

	/** True once a site URL and credentials are both present. Does not imply they are valid. */
	bool IsReady() const;

	/**
	 * Starts one guarded background refresh of the cached account identity.
	 * A previously verified user remains available immediately while the request is in flight.
	 */
	void EnsureUserInfo();

	/**
	 * Verifies the cached credentials against GET /rest/api/3/myself and updates the auth state.
	 * OnDone runs on the game thread.
	 */
	void TestConnection(TFunction<void(bool /*bSuccess*/, const FExtendedAtlassianUser&, const FExtendedAtlassianError&)> OnDone);

	// --- Requests ----------------------------------------------------------

	void Get(const FString& Url, FExtendedAtlassianResponseDelegate OnComplete);
	void PostJson(const FString& Url, const FString& JsonBody, FExtendedAtlassianResponseDelegate OnComplete);
	void PutJson(const FString& Url, const FString& JsonBody, FExtendedAtlassianResponseDelegate OnComplete);
	void Delete(const FString& Url, FExtendedAtlassianResponseDelegate OnComplete);

	/** JSON convenience over RequestRaw. Pass an empty body for verbs that take none. */
	void Request(const FString& Verb, const FString& Url, const FString& JsonBody, FExtendedAtlassianResponseDelegate OnComplete);

	/** Full control, used by attachment upload to send multipart/form-data. */
	void RequestRaw(
		const FString& Verb,
		const FString& Url,
		TArray<uint8> Body,
		const FString& ContentType,
		const TMap<FString, FString>& ExtraHeaders,
		FExtendedAtlassianResponseDelegate OnComplete);

private:
	void SendPending(TSharedRef<FExtendedAtlassianPendingRequest> Pending);
	void ScheduleRetry(TSharedRef<FExtendedAtlassianPendingRequest> Pending, float DelaySeconds);
	FString BuildAuthorizationHeader() const;
	void SetAuthState(EExtendedAtlassianAuthState NewState, const FExtendedAtlassianError& InError);

	FExtendedAtlassianCredentials Credentials;
	FExtendedAtlassianUser VerifiedUser;
	FExtendedAtlassianError LastAuthError;
	EExtendedAtlassianAuthState AuthState = EExtendedAtlassianAuthState::NotConfigured;
	FOnAuthStateChanged AuthStateChanged;
	bool bAutomaticUserRefreshStarted = false;
	bool bAutomaticUserRefreshInFlight = false;
	/** Invalidates callbacks started for credentials that have since changed. */
	uint64 CredentialGeneration = 0;
};
