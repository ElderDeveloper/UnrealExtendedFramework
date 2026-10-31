// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGTimeDilationStack.h"

void FEGTimeDilationStack::Reset(float InBase)
{
	Requests.Reset();
	Base = InBase;
	Current = InBase;
	BlendFrom = InBase;
	BlendTo = InBase;
	BlendStart = 0.0;
	BlendDuration = 0.0f;
	TopId = INDEX_NONE;
}

const FEGTimeDilationStack::FRequest* FEGTimeDilationStack::FindTop() const
{
	const FRequest* Top = nullptr;
	for (const FRequest& Request : Requests)
	{
		if (!Top || Request.Priority > Top->Priority || (Request.Priority == Top->Priority && Request.Order > Top->Order))
		{
			Top = &Request;
		}
	}
	return Top;
}

void FEGTimeDilationStack::UpdateBlend(double Now)
{
	if (BlendDuration <= 0.0f)
	{
		Current = BlendTo;
		return;
	}

	const float Alpha = FMath::Clamp(static_cast<float>((Now - BlendStart) / BlendDuration), 0.0f, 1.0f);
	Current = FMath::Lerp(BlendFrom, BlendTo, Alpha);
}

void FEGTimeDilationStack::Retarget(double Now, float BlendTime)
{
	// Start the new blend from wherever the old one had got to.
	UpdateBlend(Now);

	const FRequest* Top = FindTop();
	TopId = Top ? Top->Id : INDEX_NONE;
	BlendFrom = Current;
	BlendTo = Top ? Top->Value : Base;
	BlendStart = Now;
	BlendDuration = FMath::Max(0.0f, BlendTime);
	UpdateBlend(Now);
}

void FEGTimeDilationStack::Push(const FRequest& Request, double Now)
{
	// Replace rather than duplicate an id that is pushed again.
	Requests.RemoveAll([&Request](const FRequest& Existing) { return Existing.Id == Request.Id; });

	FRequest& Added = Requests.Add_GetRef(Request);
	Added.Order = ++NextOrder;

	// Retarget when the new request wins, or when re-pushing an id changed who wins.
	const FRequest* Top = FindTop();
	if (Top && (Top->Id == Added.Id || Top->Id != TopId))
	{
		Retarget(Now, Added.BlendIn);
	}
}

bool FEGTimeDilationStack::Remove(int32 Id, double Now)
{
	const int32 Index = Requests.IndexOfByPredicate([Id](const FRequest& Request) { return Request.Id == Id; });
	if (Index == INDEX_NONE)
	{
		return false;
	}

	const bool bWasTop = (Id == TopId);
	const float BlendOut = Requests[Index].BlendOut;
	Requests.RemoveAt(Index);

	if (bWasTop)
	{
		Retarget(Now, BlendOut);
	}
	return true;
}

bool FEGTimeDilationStack::Contains(int32 Id) const
{
	return Requests.ContainsByPredicate([Id](const FRequest& Request) { return Request.Id == Id; });
}

void FEGTimeDilationStack::SetBase(float InBase, double Now, float BlendTime)
{
	Base = InBase;
	if (Requests.Num() == 0)
	{
		Retarget(Now, BlendTime);
	}
}

float FEGTimeDilationStack::Advance(double Now)
{
	// Expire in order of expiry so the blend-outs chain the way they would have live.
	for (;;)
	{
		const FRequest* Due = nullptr;
		for (const FRequest& Request : Requests)
		{
			if (Request.ExpireTime >= 0.0 && Request.ExpireTime <= Now && (!Due || Request.ExpireTime < Due->ExpireTime))
			{
				Due = &Request;
			}
		}
		if (!Due)
		{
			break;
		}
		// Never move the blend clock backwards when Advance has not been called for a while.
		const int32 DueId = Due->Id;
		const double DueTime = FMath::Max(Due->ExpireTime, BlendStart);
		Remove(DueId, DueTime);
	}

	UpdateBlend(Now);
	return Current;
}

bool FEGTimeDilationStack::IsActive() const
{
	return Requests.Num() > 0 || !FMath::IsNearlyEqual(Current, Base) || !FMath::IsNearlyEqual(BlendTo, Base);
}
