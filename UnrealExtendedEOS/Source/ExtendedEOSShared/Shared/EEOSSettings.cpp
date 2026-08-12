// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EEOSSettings.h"
#include "EEOSLog.h"

UEEOSSettings::UEEOSSettings()
{
}

void UEEOSSettings::PostInitProperties()
{
	// Super loads the config for the CDO of a config class — read bEnableVerboseLogging only
	// after it has run, or the ini value is not in yet.
	Super::PostInitProperties();
	ApplyLogVerbosity();
}

#if WITH_EDITOR
void UEEOSSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	ApplyLogVerbosity();
}
#endif

void UEEOSSettings::ApplyLogVerbosity() const
{
	// LogExtendedEOS is declared DefaultVerbosity=Log / CompileTimeVerbosity=All, so Verbose
	// output is compiled in on every configuration but suppressed until raised here. This
	// setting is the single owner of the category's runtime verbosity (see the property
	// comment) — it is pushed both ways so turning the toggle back off restores the default.
	const ELogVerbosity::Type Target = bEnableVerboseLogging ? ELogVerbosity::Verbose : ELogVerbosity::Log;
	if (LogExtendedEOS.GetVerbosity() != Target)
	{
		LogExtendedEOS.SetVerbosity(Target);
		UE_LOG(LogExtendedEOS, Log, TEXT("Extended EOS: LogExtendedEOS verbosity set to %s (bEnableVerboseLogging=%s)"),
			ToString(Target), bEnableVerboseLogging ? TEXT("true") : TEXT("false"));
	}
}
