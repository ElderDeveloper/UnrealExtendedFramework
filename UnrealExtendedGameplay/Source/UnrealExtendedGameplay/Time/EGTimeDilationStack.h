// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * FEGTimeDilationStack
 *
 * The resolution rules behind UEGTimeDilationSubsystem, kept free of worlds and actors so they can
 * be tested on their own. One stack controls one value (the world's time dilation, an actor's
 * custom time dilation, a mesh's anim rate):
 *
 *  - The request with the highest priority wins; on a tie, the newest.
 *  - The value the target had before the first request is the base, restored after the last.
 *  - A request that takes over blends in over its BlendIn; removing the winning request blends
 *    to the next one (or the base) over its BlendOut.
 *  - Requests with an expiry time remove themselves.
 *
 * Times are absolute seconds on any clock that only moves forward; the subsystem uses real time.
 */
struct UNREALEXTENDEDGAMEPLAY_API FEGTimeDilationStack
{
	struct FRequest
	{
		int32 Id = INDEX_NONE;
		float Value = 1.0f;
		int32 Priority = 0;
		uint64 Order = 0;
		float BlendIn = 0.0f;
		float BlendOut = 0.0f;
		/** Negative: until removed. */
		double ExpireTime = -1.0;
	};

	/** Start from this base value with no requests. */
	void Reset(float InBase);

	void Push(const FRequest& Request, double Now);
	bool Remove(int32 Id, double Now);
	bool Contains(int32 Id) const;

	/**
	 * Change the value restored after the last request. With no requests it blends there over
	 * BlendTime right away.
	 */
	void SetBase(float InBase, double Now, float BlendTime = 0.0f);

	/** Expires due requests, advances the blend and returns the value to apply now. */
	float Advance(double Now);

	float GetCurrent() const { return Current; }
	float GetBase() const { return Base; }
	int32 Num() const { return Requests.Num(); }

	/** Requests remain, or the value is still on its way back to the base. */
	bool IsActive() const;

private:
	const FRequest* FindTop() const;
	void UpdateBlend(double Now);
	void Retarget(double Now, float BlendTime);

	TArray<FRequest> Requests;
	float Base = 1.0f;
	float Current = 1.0f;
	float BlendFrom = 1.0f;
	float BlendTo = 1.0f;
	double BlendStart = 0.0;
	float BlendDuration = 0.0f;
	int32 TopId = INDEX_NONE;
	uint64 NextOrder = 0;
};
