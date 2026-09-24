// Copyright Moon Punch Games. All Rights Reserved.

#include "EFLogViewerSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EFLogViewerSettings)

FSimpleMulticastDelegate& UEFLogViewerSettings::OnHiddenColumnsChanged()
{
	static FSimpleMulticastDelegate Delegate;
	return Delegate;
}
