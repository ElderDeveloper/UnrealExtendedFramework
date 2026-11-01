// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Http/EFHttpTypes.h"
#include "Shared/EPFTypes.h"
#include "EPFSubsystem.generated.h"

class UEPFSettings;

UENUM(BlueprintType)
enum class EEPFAuthMode : uint8
{
	None,
	SessionTicket,
	EntityToken,
	SecretKey
};

/**
 * Base class for all Extended PlayFab subsystems.
 * Provides common HTTP request dispatch, token management, retry logic, and rate limiting.
 */
UCLASS(Abstract)
class UNREALEXTENDEDPLAYFAB_API UEPFSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "PlayFab")
	bool IsAuthenticated() const;

	UFUNCTION(BlueprintPure, Category = "PlayFab")
	FString GetPlayFabId() const;

	FString GetSessionTicket() const;
	FString GetEntityToken() const;
	FString GetEntityId() const;
	FString GetEntityType() const;

	UFUNCTION(BlueprintPure, Category = "PlayFab")
	FEPFError GetLastError() const;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab")
	FOnEPFOperationComplete OnRequestComplete;

	UPROPERTY(BlueprintAssignable, Category = "PlayFab")
	FOnEPFErrorReceived OnRequestError;

protected:

	DECLARE_DELEGATE_TwoParams(FOnPlayFabResponse, bool /*bSuccess*/, TSharedPtr<FJsonObject> /*JsonResponse*/);
	DECLARE_DELEGATE_TwoParams(FOnPlayFabResponseDetailed, const FEPFResult& /*Result*/, TSharedPtr<FJsonObject> /*JsonResponse*/);

	void SendPlayFabRequest(
		const FString& ApiPath,
		const TSharedRef<FJsonObject>& RequestBody,
		EEPFAuthMode AuthMode,
		const FOnPlayFabResponse& OnComplete
	);

	void SendPlayFabRequestDetailed(
		const FString& ApiPath,
		const TSharedRef<FJsonObject>& RequestBody,
		EEPFAuthMode AuthMode,
		const FOnPlayFabResponseDetailed& OnComplete
	);

	const UEPFSettings* GetSettings() const;
	void LogNotConfigured(const FString& FunctionName) const;
	bool IsConfigured() const;
	void SetSharedAuthContext(const FEPFAuthContext& InAuthContext);
	void ClearSharedAuthContext();

	static FEPFAuthContext SharedAuthContext;

	/** The plugin's automation tests install a session through it and put the previous one back (Tests/EPFSharedAuthTestScope.h). */
	friend struct FEPFSharedAuthTestScope;

private:

#if WITH_DEV_AUTOMATION_TESTS
	friend class FEPFHttpLoadingResponseTest;
#endif

	// UObject validity alone does not cover the interval between Deinitialize and GC.
	bool bDeinitialized = false;

	static constexpr int32 MaxRetries = 3;
	static constexpr float BaseRetryDelaySec = 1.0f;

	void ExecuteRequest(
		const FString& ApiPath,
		const FString& RequestBodyString,
		EEPFAuthMode AuthMode,
		const FOnPlayFabResponseDetailed& OnComplete,
		int32 RetryAttempt = 0
	);

	void HandleHttpResponse(
		const FEFHttpResponse& Response,
		const FString& ApiPath,
		const FString& RequestBodyString,
		EEPFAuthMode AuthMode,
		const FOnPlayFabResponseDetailed& OnComplete,
		int32 RetryAttempt
	);

	static constexpr int32 MaxRequestsPerSecond = 10;

	struct FPendingRequest
	{
		FString ApiPath;
		FString RequestBodyString;
		EEPFAuthMode AuthMode = EEPFAuthMode::None;
		FOnPlayFabResponseDetailed OnComplete;
	};

	TArray<FPendingRequest> PendingRequestQueue;
	int32 RequestsThisSecond = 0;
	FTSTicker::FDelegateHandle RateLimitResetHandle;

	void ProcessRequestQueue();
	void ResetRateLimitCounter();
	/** Sends the request again after Delay seconds, unless this subsystem is gone by then. */
	void ScheduleRetry(const FString& ApiPath, const FString& RequestBodyString, EEPFAuthMode AuthMode,
		const FOnPlayFabResponseDetailed& OnComplete, int32 RetryAttempt, float Delay);

	/**
	 * True when PlayFab rejected the call because the session is no longer good.
	 *
	 * Reads the *PlayFab* code, not the transport one: requests set X-ReportErrorAsSuccess, so
	 * a rejected call still arrives as HTTP 200 with the real status in the body's "code".
	 */
	static bool IsSessionRejection(int32 PlayFabCode, const FString& ErrorCode);

	/**
	 * On a rejected session, re-authenticate once and replay this request. Returns true when it
	 * has taken ownership of the completion — the caller must not also deliver the failure.
	 */
	bool TryReauthenticateAndReplay(
		const FString& ApiPath,
		const FString& RequestBodyString,
		EEPFAuthMode AuthMode,
		const FOnPlayFabResponseDetailed& OnComplete,
		int32 RetryAttempt,
		int32 PlayFabCode,
		const FString& ErrorCode,
		const FEPFError& Error,
		TSharedPtr<FJsonObject> JsonResponse
	);

	/** The one place a failed request is handed back, so the replay path cannot drift from it. */
	void DeliverFailure(const FEPFError& Error, TSharedPtr<FJsonObject> JsonResponse, const FOnPlayFabResponseDetailed& OnComplete);

	FEPFError LastError;
};
