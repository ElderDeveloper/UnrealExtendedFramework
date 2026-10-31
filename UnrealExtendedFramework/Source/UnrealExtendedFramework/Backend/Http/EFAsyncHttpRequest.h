// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Backend/Http/EFHttpClient.h"
#include "Backend/Http/EFHttpTypes.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "EFAsyncHttpRequest.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEFAsyncHttpResponsePin, const FEFHttpResponse&, Response);

/**
 * Blueprint node that sends one HTTP request and continues on On Success or On Failure.
 *
 * Goes through the game instance's UEFHttpSubsystem when there is one, so its base URL and default
 * headers apply. Without a game instance (editor utilities) the request is sent directly.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFAsyncHttpRequest : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/** The server answered with a 2xx status. */
	UPROPERTY(BlueprintAssignable)
	FEFAsyncHttpResponsePin OnSuccess;

	/** Any other outcome: an HTTP error status, no connection, a timeout, cancellation or an invalid request. Check Result. */
	UPROPERTY(BlueprintAssignable)
	FEFAsyncHttpResponsePin OnFailure;

	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Http", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Http Request"))
	static UEFAsyncHttpRequest* SendHttpRequestAsync(UObject* WorldContextObject, const FEFHttpRequest& Request);

	/** Cancels the request. On Failure fires with a Cancelled result. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Backend|Http")
	void Cancel();

	virtual void Activate() override;

private:
	void HandleResponse(const FEFHttpResponse& Response);

	FEFHttpRequest Request;
	TWeakObjectPtr<UObject> WorldContext;
	FEFHttpRequestHandle SubsystemHandle;
	TSharedPtr<FEFHttpOperation> DirectOperation;
	bool bCompleted = false;
};
