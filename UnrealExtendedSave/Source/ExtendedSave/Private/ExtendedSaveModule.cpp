// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "ExtendedSaveModule.h"

#include "EFFoundationVersion.h"
#include "Modules/ModuleManager.h"

// Fails the build with a readable message if this copy is paired with an older foundation.
EF_REQUIRE_FOUNDATION_VERSION(1, 1, "UnrealExtendedSave");

DEFINE_LOG_CATEGORY(LogExtendedSave);

void FExtendedSaveModule::StartupModule()
{
}

void FExtendedSaveModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FExtendedSaveModule, ExtendedSave)
