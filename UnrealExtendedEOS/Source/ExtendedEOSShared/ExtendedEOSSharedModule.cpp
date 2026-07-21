// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Shared/EEOSLog.h"

// Base module (PreDefault): shared types, settings, the UEEOSSubsystem base class, and the log
// category. Loads before the domain subsystems so they can derive from and reference these.
DEFINE_LOG_CATEGORY(LogExtendedEOS);

IMPLEMENT_MODULE(FDefaultModuleImpl, ExtendedEOSShared);
