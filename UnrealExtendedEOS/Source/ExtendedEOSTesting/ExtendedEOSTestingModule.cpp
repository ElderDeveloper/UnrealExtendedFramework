// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Modules/ModuleManager.h"

// UncookedOnly module: automation tests + the live integration test actor. Excluded from cooked
// builds by construction, so live-network test code can never ship.
IMPLEMENT_MODULE(FDefaultModuleImpl, ExtendedEOSTesting);
