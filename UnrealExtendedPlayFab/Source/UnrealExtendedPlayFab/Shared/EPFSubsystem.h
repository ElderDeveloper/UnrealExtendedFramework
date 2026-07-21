// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
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
		bool bRequiresAuth,
		const FOnPlayFabResponse& OnComplete
	);

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

	/** Legacy bool overload: true → SessionTicket, false → None. */
	void SendPlayFabRequestDetailed(
		const FString& ApiPath,
		const TSharedRef<FJsonObject>& RequestBody,
		bool bRequiresAuth,
		const FOnPlayFabResponseDetailed& OnComplete
	)
	{
		SendPlayFabRequestDetailed(ApiPath, RequestBody,
			bRequiresAuth ? EEPFAuthMode::SessionTicket : EEPFAuthMode::None,
			OnComplete);
	}

	const UEPFSettings* GetSettings() const;
	void LogNotConfigured(const FString& FunctionName) const;
	bool IsConfigured() const;
	void SetSharedAuthContext(const FEPFAuthContext& InAuthContext);
	void ClearSharedAuthContext();

	// Legacy compatibility fields for existing subsystem code paths.
	static FString SharedSessionTicket;
	static FString SharedPlayFabId;

	static FEPFAuthContext SharedAuthContext;

private:

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
		FHttpRequestPtr Request,
		FHttpResponsePtr Response,
		bool bConnectedSuccessfully,
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
	FTimerHandle RateLimitResetHandle;

	void ProcessRequestQueue();
	void ResetRateLimitCounter();

	FEPFError LastError;
};
