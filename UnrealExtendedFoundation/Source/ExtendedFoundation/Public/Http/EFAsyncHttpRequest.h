// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/EFAsyncActionBase.h"
#include "Http/EFHttpClient.h"
#include "Http/EFHttpTypes.h"
#include "EFAsyncHttpRequest.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEFAsyncHttpResponsePin, const FEFHttpResponse&, Response);

/**
 * Blueprint node that sends one HTTP request and continues on On Success or On Failure.
 *
 * Goes through the game instance's UEFHttpSubsystem when there is one, so its base URL and default
 * headers apply. Without a game instance (editor utilities) the request is sent directly. Cancel it
 * through the node's Async Task pin; On Failure then fires with a Cancelled result.
 */
UCLASS()
class EXTENDEDFOUNDATION_API UEFAsyncHttpRequest : public UEFAsyncActionBase
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

protected:
	virtual void ExecuteAction() override;
	virtual void BroadcastFailure(const FEFResult& Result) override;
	virtual void CancelWork() override;
	virtual FName GetResultSource() const override { return TEXT("Http"); }

private:
	void HandleResponse(const FEFHttpResponse& Response);

	FEFHttpRequest Request;
	FEFHttpRequestHandle SubsystemHandle;
	TSharedPtr<FEFHttpOperation> DirectOperation;
};
