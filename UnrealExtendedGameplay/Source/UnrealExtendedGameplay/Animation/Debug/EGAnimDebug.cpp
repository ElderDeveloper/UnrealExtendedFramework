// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGAnimDebug.h"

#include "EngineDefines.h"
#include "HAL/IConsoleManager.h"
#include "UnrealExtendedGameplay/Animation/EGAnimationTags.h"

namespace
{
	TAutoConsoleVariable<int32> CVarEGAnimDebug(
		TEXT("eg.Anim.Debug"),
		0,
		TEXT("1: draw Extended Gameplay melee sweeps and warp targets."),
		ECVF_Cheat);
}

bool EGAnimDebug::IsEnabled()
{
#if ENABLE_DRAW_DEBUG
	return CVarEGAnimDebug.GetValueOnGameThread() > 0;
#else
	return false;
#endif
}

void EGAnimDebug::WarnOnce(const UObject* Source, const FString& Message)
{
	static TSet<FString> Reported;

	const FString Key = GetPathNameSafe(Source) + TEXT("|") + Message;
	if (Reported.Contains(Key))
	{
		return;
	}
	Reported.Add(Key);

	UE_LOG(LogEGAnimation, Warning, TEXT("%s: %s"), *GetNameSafe(Source), *Message);
}
