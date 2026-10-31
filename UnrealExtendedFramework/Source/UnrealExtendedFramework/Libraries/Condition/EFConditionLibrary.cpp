// Copyright Moon Punch Games. All Rights Reserved.


#include "EFConditionLibrary.h"

bool UEFConditionLibrary::IsPlayingInEditor()
{
	#if WITH_EDITOR
		return true;
	#else
		return false;
	#endif
}
