// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "UnrealExtendedGameplay.h"

#include "EFFoundationVersion.h"

// Asset preloading (EFAssets) arrived in foundation 1.1.
EF_REQUIRE_FOUNDATION_VERSION(1, 1, "UnrealExtendedGameplay");

#define LOCTEXT_NAMESPACE "FUnrealExtendedGameplayModule"

void FUnrealExtendedGameplayModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FUnrealExtendedGameplayModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealExtendedGameplayModule, UnrealExtendedGameplay)
