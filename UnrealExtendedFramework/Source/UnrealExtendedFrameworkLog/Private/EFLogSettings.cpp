// Copyright Moon Punch Games. All Rights Reserved.

#include "EFLogSettings.h"

#include "EFLogRegistry.h"
#include "EFLogWriter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EFLogSettings)

void UEFLogSettings::ApplyToRuntime() const
{
	FEFLogRegistry::Get().SetConfiguredVerbosity(DefaultVerbosity, CategoryVerbosity);
	FEFLogWriter::Get().SetRuntimeOptions(FlushIntervalSeconds, MaxFileSizeMB);
}

void UEFLogSettings::PostInitProperties()
{
	// Super loads the ini into the CDO, so this is the first point the configured values exist.
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		ApplyToRuntime();
	}
}

#if WITH_EDITOR
void UEFLogSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// An edit in Project Settings takes effect at once, without a restart.
	ApplyToRuntime();
}
#endif
