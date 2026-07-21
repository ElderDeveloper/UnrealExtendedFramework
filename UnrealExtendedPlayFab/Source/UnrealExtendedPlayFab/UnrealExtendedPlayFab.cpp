// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "UnrealExtendedPlayFab.h"

#define LOCTEXT_NAMESPACE "FUnrealExtendedPlayFabModule"

DEFINE_LOG_CATEGORY(LogExtendedPlayFab);

void FUnrealExtendedPlayFabModule::StartupModule()
{
	UE_LOG(LogExtendedPlayFab, Log, TEXT("UnrealExtendedPlayFab module started"));
}

void FUnrealExtendedPlayFabModule::ShutdownModule()
{
	UE_LOG(LogExtendedPlayFab, Log, TEXT("UnrealExtendedPlayFab module shutdown"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUnrealExtendedPlayFabModule, UnrealExtendedPlayFab)
