// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "UnrealExtendedPlayFab.h"

#define LOCTEXT_NAMESPACE "FUnrealExtendedPlayFabModule"

void FUnrealExtendedPlayFabModule::StartupModule()
{
	EF_LOG(ExtendedPlayFab, Log, TEXT("UnrealExtendedPlayFab module started"));
}

void FUnrealExtendedPlayFabModule::ShutdownModule()
{
	EF_LOG(ExtendedPlayFab, Log, TEXT("UnrealExtendedPlayFab module shutdown"));
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FUnrealExtendedPlayFabModule, UnrealExtendedPlayFab)
