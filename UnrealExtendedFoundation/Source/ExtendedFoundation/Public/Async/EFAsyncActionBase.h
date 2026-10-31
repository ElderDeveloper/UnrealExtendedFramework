// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/EFCallContext.h"
#include "Containers/Ticker.h"
#include "Engine/CancellableAsyncAction.h"
#include "Result/EFResult.h"
#include "Subsystems/EFSubsystems.h"
#include "EFAsyncActionBase.generated.h"

/**
 * Base for Extended async Blueprint nodes.
 *
 * Handles what every node otherwise repeats: keeping the node alive, remembering the world context,
 * finishing exactly once, an optional timeout, cancellation through the node's Async Task pin, and
 * reporting failures that are found before Blueprint has bound the output pins.
 *
 * A node subclass:
 *
 *     UCLASS()
 *     class UEPFAsyncGetSharedData : public UEFAsyncActionBase
 *     {
 *         UPROPERTY(BlueprintAssignable) FOnSharedData OnSuccess;
 *         UPROPERTY(BlueprintAssignable) FEFAsyncFailurePin OnFailure;
 *
 *         static UEPFAsyncGetSharedData* GetSharedGroupData(UObject* WorldContext, const FString& GroupId)
 *         {
 *             UEPFAsyncGetSharedData* Action = NewObject<UEPFAsyncGetSharedData>();
 *             Action->InitAsyncAction(WorldContext);
 *             Action->GroupId = GroupId;
 *             return Action;
 *         }
 *
 *         virtual void ExecuteAction() override
 *         {
 *             UEPFSharedDataSubsystem* Data = RequireGameInstanceSubsystem<UEPFSharedDataSubsystem>();
 *             if (!Data) return;                                   // already failed with NotAvailable
 *             Data->GetSharedGroupData(GroupId, MakeWeakCallback<const FEFResult&, const FData&>(
 *                 [this](const FEFResult& Result, const FData& Data)
 *                 {
 *                     if (FinishAction()) { Result.IsSuccess() ? OnSuccess.Broadcast(Data) : OnFailure.Broadcast(Result); }
 *                 }));
 *         }
 *
 *         virtual void BroadcastFailure(const FEFResult& Result) override { OnFailure.Broadcast(Result); }
 *     };
 *
 * Results should come back to the call that asked for them (a per-call callback, as above). When a
 * subsystem only reports through a delegate every caller shares, start the call inside
 * CallWithContext() and return early from the handler unless IsResultForThisCall(); with a subsystem
 * that propagates EFCallContext, two nodes running at once then each finish on their own result.
 */
UCLASS(Abstract)
class EXTENDEDFOUNDATION_API UEFAsyncActionBase : public UCancellableAsyncAction
{
	GENERATED_BODY()

public:
	virtual void Activate() override;

	/** Stops the node. Its failure pin fires with a Cancelled result. */
	virtual void Cancel() override;

	virtual bool IsActive() const override;

	bool IsFinished() const { return bFinished; }

	UObject* GetWorldContextObject() const { return WorldContext.Get(); }

protected:
	/**
	 * Call from the node's static factory. Keeps the node alive with the context's game instance and
	 * arms an optional timeout, which starts when the node activates.
	 */
	void InitAsyncAction(UObject* WorldContextObject, float InTimeoutSeconds = 0.f);

	/** Starts the work. Runs once, from Activate(), unless the node already failed. */
	virtual void ExecuteAction() PURE_VIRTUAL(UEFAsyncActionBase::ExecuteAction, );

	/** Fires the node's failure pin. Called once for cancellation, timeouts and FailAction(). */
	virtual void BroadcastFailure(const FEFResult& Result) PURE_VIRTUAL(UEFAsyncActionBase::BroadcastFailure, );

	/** Stops the underlying work on cancellation or timeout: cancel a request, unbind a delegate. */
	virtual void CancelWork() {}

	/** The Source used for the node's own Cancelled, TimedOut and NotAvailable results. */
	virtual FName GetResultSource() const { return TEXT("Async"); }

	/**
	 * Marks the node finished and ready to be destroyed. Returns false when it had already finished,
	 * in which case the caller must not broadcast. Broadcast after a true return.
	 */
	bool FinishAction();

	/**
	 * Finishes the node and fires its failure pin. Safe before activation: the failure is held until
	 * Activate(), when Blueprint has bound the pins.
	 */
	void FailAction(const FEFResult& Result);

	/** Convenience for FailAction(FEFResult::InvalidArgument(...)). */
	void FailInvalidArgument(const FString& Problem);

	/**
	 * Runs Call with this node's own call context active. A subsystem that propagates
	 * EFCallContext then tags the result of that call as this node's. Use it around the call that
	 * starts the work, and check IsResultForThisCall() in the delegate handler.
	 */
	void CallWithContext(TFunctionRef<void()> Call);

	/**
	 * False when the broadcast being handled belongs to another node's call. Broadcasts that carry no
	 * context (from code that does not propagate one) are accepted.
	 */
	bool IsResultForThisCall() const;

	template <typename TSubsystem>
	TSubsystem* GetGameInstanceSubsystem() const
	{
		return EF::GetGameInstanceSubsystem<TSubsystem>(WorldContext.Get());
	}

	/** The subsystem, or null after failing the node with a NotAvailable result. */
	template <typename TSubsystem>
	TSubsystem* RequireGameInstanceSubsystem()
	{
		TSubsystem* Subsystem = GetGameInstanceSubsystem<TSubsystem>();
		if (!Subsystem)
		{
			FailAction(FEFResult::NotAvailable(GetResultSource(), TSubsystem::StaticClass()->GetName()));
		}
		return Subsystem;
	}

	/**
	 * Wraps a completion callback so it runs only while this node exists and has not finished.
	 * Pass the callback's parameter types explicitly: MakeWeakCallback<const FEFResult&>(...).
	 */
	template <typename... ArgTypes, typename FunctorType>
	TFunction<void(ArgTypes...)> MakeWeakCallback(FunctorType&& Functor)
	{
		return [WeakThis = TWeakObjectPtr<UEFAsyncActionBase>(this), Callback = Forward<FunctorType>(Functor)](ArgTypes... Args)
		{
			if (WeakThis.IsValid() && !WeakThis->IsFinished())
			{
				Callback(Args...);
			}
		};
	}

private:
	void HandleTimeout();

	TWeakObjectPtr<UObject> WorldContext;
	TOptional<FEFResult> PendingFailure;
	int64 CallContext = 0;
	FTSTicker::FDelegateHandle TimeoutHandle;
	float TimeoutSeconds = 0.f;
	bool bActivated = false;
	bool bFinished = false;
};

/** Failure pin shared by Extended async nodes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEFAsyncFailurePin, const FEFResult&, Result);
