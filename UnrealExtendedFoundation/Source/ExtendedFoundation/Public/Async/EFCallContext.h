// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Tells results apart when a subsystem reports them on a delegate every caller listens to.
 *
 * Many subsystems finish work by broadcasting a multicast delegate (OnStatsQueried, OnDataReceived).
 * An async node that starts a call and waits for that broadcast cannot tell its own result from one
 * caused by a second node doing the same thing at the same time. A call context fixes that without
 * changing the subsystem's public API:
 *
 * 1. The node makes its call inside a scope with a fresh context id (UEFAsyncActionBase::
 *    CallWithContext does this).
 * 2. The subsystem captures the active id when it starts asynchronous work and makes it active
 *    again while it reports the result:
 *
 *        const int64 Context = EFCallContext::GetActive();
 *        Sdk->Query(..., [this, Context](...)
 *        {
 *            EFCallContext::FScope Scope(Context);
 *            OnQueried.Broadcast(...);
 *        });
 *
 *    Work started while handling a result inherits its context, so chained calls keep it.
 * 3. The node's handler ignores broadcasts carrying another node's context
 *    (UEFAsyncActionBase::IsResultForThisCall).
 *
 * Broadcasts from code that does not propagate contexts carry 0 and are accepted, as before, so
 * adopting this is never a regression. Game thread only.
 */
namespace EFCallContext
{
	/** A new, unique, non-zero id. */
	EXTENDEDFOUNDATION_API int64 New();

	/** The id active on the game thread right now, or 0 for none. */
	EXTENDEDFOUNDATION_API int64 GetActive();

	/** Makes an id active for the lifetime of the scope and restores the previous one afterwards. */
	class EXTENDEDFOUNDATION_API FScope
	{
	public:
		explicit FScope(int64 InContext);
		~FScope();

		FScope(const FScope&) = delete;
		FScope& operator=(const FScope&) = delete;

	private:
		int64 Previous = 0;
	};

	/**
	 * Wraps a callback so it runs with the context that was active when it was wrapped. Use it when
	 * handing a completion lambda to code that calls it later:
	 *
	 *     Request.OnComplete = EFCallContext::Bind<const FResult&>([this](const FResult& R) { OnDone.Broadcast(R); });
	 */
	template <typename... ArgTypes, typename FunctorType>
	TFunction<void(ArgTypes...)> Bind(FunctorType&& Functor)
	{
		return [Context = GetActive(), Callback = Forward<FunctorType>(Functor)](ArgTypes... Args)
		{
			FScope Scope(Context);
			Callback(Args...);
		};
	}
}
