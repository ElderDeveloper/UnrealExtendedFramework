// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Completion callbacks for the calls a subsystem has in flight, keyed by request id.
 *
 * Lets a subsystem hand each result back to the call that asked for it instead of broadcasting on a
 * delegate every caller listens to. Store the id wherever the SDK lets you carry context (an HTTP
 * lambda capture, EOS ClientData, a Steam call handle) and complete it when the result arrives:
 *
 *     TEFPendingCalls<const FEFResult&, const FMyData&> PendingQueries;
 *
 *     void UMySubsystem::Query(FOnQueried OnDone)
 *     {
 *         const int64 Id = PendingQueries.Add(MoveTemp(OnDone));
 *         StartSdkQuery(..., [this, Id](...) { PendingQueries.Complete(Id, Result, Data); });
 *     }
 *
 *     void UMySubsystem::Deinitialize() { PendingQueries.Reset(); }
 *
 * Game thread only.
 */
template <typename... ArgTypes>
class TEFPendingCalls
{
public:
	using FCallback = TFunction<void(ArgTypes...)>;

	/** Registers a callback and returns its id. An empty callback still gets an id, so callers need no special case. */
	int64 Add(FCallback Callback)
	{
		const int64 Id = ++LastId;
		if (Callback)
		{
			Callbacks.Add(Id, MoveTemp(Callback));
		}
		return Id;
	}

	/** Runs and removes the callback for Id. Returns false if there was none. The callback may add new calls. */
	template <typename... CallArgTypes>
	bool Complete(int64 Id, CallArgTypes&&... Args)
	{
		FCallback Callback;
		if (!Callbacks.RemoveAndCopyValue(Id, Callback))
		{
			return false;
		}
		Callback(Forward<CallArgTypes>(Args)...);
		return true;
	}

	/** Runs every pending callback with the same arguments, e.g. a Cancelled result on shutdown, and clears them. */
	template <typename... CallArgTypes>
	void CompleteAll(const CallArgTypes&... Args)
	{
		TMap<int64, FCallback> All = MoveTemp(Callbacks);
		Callbacks.Reset();
		for (TPair<int64, FCallback>& Pair : All)
		{
			Pair.Value(Args...);
		}
	}

	/** Drops every pending callback without running it. */
	void Reset() { Callbacks.Reset(); }

	bool Contains(int64 Id) const { return Callbacks.Contains(Id); }
	int32 Num() const { return Callbacks.Num(); }

private:
	TMap<int64, FCallback> Callbacks;
	int64 LastId = 0;
};
