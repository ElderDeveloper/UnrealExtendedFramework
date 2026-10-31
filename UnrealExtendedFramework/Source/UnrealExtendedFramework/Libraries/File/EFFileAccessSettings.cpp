// Copyright Moon Punch Games. All Rights Reserved.

#include "EFFileAccessSettings.h"

bool UEFFileAccessSettings::IsRestrictionActive() const
{
	if (RestrictionMode == EEFFileAccessRestriction::Off)
	{
		return false;
	}
	if (RestrictionMode == EEFFileAccessRestriction::Always)
	{
		return true;
	}

	// ShippingOnly, and any value a hand-edited config file does not name.
#if UE_BUILD_SHIPPING
	return true;
#else
	return false;
#endif
}
