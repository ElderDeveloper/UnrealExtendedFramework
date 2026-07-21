// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSSearchCoordinator.h"
#include "UnrealExtendedEOS.h"

bool UEEOSSearchCoordinator::TryAcquire(FName OwnerTag)
{
	if (OwnerTag.IsNone())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSearchCoordinator::TryAcquire — NAME_None is not a valid owner tag"));
		return false;
	}

	if (!CurrentOwner.IsNone())
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSearchCoordinator::TryAcquire — '%s' rejected: the search slot is held by '%s' (the EOS OSS cannot run concurrent session/lobby searches)"),
			*OwnerTag.ToString(), *CurrentOwner.ToString());
		return false;
	}

	CurrentOwner = OwnerTag;
	return true;
}

void UEEOSSearchCoordinator::Release(FName OwnerTag)
{
	if (CurrentOwner.IsNone())
	{
		// Already free — terminal paths release unconditionally, so this is benign.
		return;
	}

	if (CurrentOwner != OwnerTag)
	{
		UE_LOG(LogExtendedEOS, Warning, TEXT("EEOSSearchCoordinator::Release — '%s' tried to release the slot held by '%s'; ignored"),
			*OwnerTag.ToString(), *CurrentOwner.ToString());
		return;
	}

	CurrentOwner = NAME_None;
}
