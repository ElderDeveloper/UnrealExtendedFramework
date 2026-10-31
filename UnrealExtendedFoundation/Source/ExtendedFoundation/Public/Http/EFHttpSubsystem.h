// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Http/EFHttpClient.h"
#include "Http/EFHttpTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EFHttpSubsystem.generated.h"

/**
 * Sends HTTP requests for the game instance.
 *
 * On top of FEFHttpOperation it adds a base URL and default headers shared by every request, and
 * handles that let Blueprints cancel a request. Requests still running when the game instance shuts
 * down are cancelled without calling back.
 *
 * Base URL and default headers live only in memory. Never put secrets that must not ship (server
 * keys, publisher tokens) in a client build: anything a client sends can be read by its player.
 */
UCLASS()
class EXTENDEDFOUNDATION_API UEFHttpSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	/** The subsystem of the game instance that owns WorldContextObject, or null. */
	static UEFHttpSubsystem* Get(const UObject* WorldContextObject);

	virtual void Deinitialize() override;

	/** Sends a request. OnComplete runs once with the result, including failures and cancellation. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Http", meta = (AutoCreateRefTerm = "OnComplete"))
	FEFHttpRequestHandle SendRequest(const FEFHttpRequest& Request, const FEFHttpResponseDelegate& OnComplete);

	/** C++ form of SendRequest. */
	FEFHttpRequestHandle Send(const FEFHttpRequest& Request, FEFHttpOperation::FOnComplete OnComplete);

	/** Cancels a running request. Its callback runs with a Cancelled result. Returns false if it already finished. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Http")
	bool CancelRequest(FEFHttpRequestHandle Handle);

	/** Cancels every running request; each callback runs with a Cancelled result. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Http")
	void CancelAllRequests();

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Http")
	bool IsRequestActive(FEFHttpRequestHandle Handle) const;

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Http")
	int32 GetActiveRequestCount() const;

	/** Prefix for request URLs that do not start with http:// or https://, e.g. "https://api.example.com/v1". */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Http")
	void SetBaseUrl(const FString& InBaseUrl);

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Http")
	FString GetBaseUrl() const { return BaseUrl; }

	/** Added to every request that does not set the same header itself. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Http")
	void SetDefaultHeader(const FString& Name, const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Http")
	void RemoveDefaultHeader(const FString& Name);

	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Http")
	void ClearDefaultHeaders();

	/** The request as it will be sent: base URL applied, default headers merged in (the request's own headers win). */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Backend|Http")
	FEFHttpRequest PrepareRequest(const FEFHttpRequest& Request) const;

	// ─── Deprecated: kept so Blueprints made with the old Backend Http nodes keep loading ──

	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Http|Deprecated", meta = (DeprecatedFunction, DeprecationMessage = "Use Send Request or the Http Request node; they report the response."))
	void SendHttpRequest(const FString& Url, const FString& RequestContent);

	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Http|Deprecated", meta = (DeprecatedFunction, DeprecationMessage = "Build the body with Make Json Object and Set String Field, then Json Object To String."))
	FString JsonString(TArray<FString> StringArray);

private:
	TMap<int64, TSharedRef<FEFHttpOperation>> ActiveOperations;
	TMap<FString, FString> DefaultHeaders;
	FString BaseUrl;
	int64 NextRequestId = 1;
};
