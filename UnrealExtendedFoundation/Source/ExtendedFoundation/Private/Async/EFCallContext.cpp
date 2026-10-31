// Copyright Moon Punch Games. All Rights Reserved.

#include "Async/EFCallContext.h"

namespace EFCallContextPrivate
{
	int64 LastIssued = 0;
	int64 Active = 0;
}

int64 EFCallContext::New()
{
	check(IsInGameThread());
	return ++EFCallContextPrivate::LastIssued;
}

int64 EFCallContext::GetActive()
{
	return IsInGameThread() ? EFCallContextPrivate::Active : 0;
}

EFCallContext::FScope::FScope(int64 InContext)
{
	check(IsInGameThread());
	Previous = EFCallContextPrivate::Active;
	EFCallContextPrivate::Active = InContext;
}

EFCallContext::FScope::~FScope()
{
	EFCallContextPrivate::Active = Previous;
}
