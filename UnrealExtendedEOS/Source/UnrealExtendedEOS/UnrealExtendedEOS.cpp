// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "UnrealExtendedEOS.h"

#define LOCTEXT_NAMESPACE "FUnrealExtendedEOSModule"

// LogExtendedEOS is now defined in ExtendedEOSShared (see ExtendedEOSSharedModule.cpp).

void FUnrealExtendedEOSModule::StartupModule()
{
	UE_LOG(LogExtendedEOS, Log, TEXT("UnrealExtendedEOS module started"));
}

void FUnrealExtendedEOSModule::ShutdownModule()
{
	UE_LOG(LogExtendedEOS, Log, TEXT("UnrealExtendedEOS module shutdown"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUnrealExtendedEOSModule, UnrealExtendedEOS)
