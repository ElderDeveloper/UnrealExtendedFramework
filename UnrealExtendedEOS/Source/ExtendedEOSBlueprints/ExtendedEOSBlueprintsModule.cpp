// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "Modules/ModuleManager.h"

// Blueprint async-action nodes (UEEOSAsyncActionBase, on the foundation's UEFAsyncActionBase) over the EOS subsystems. Kept in its
// own module so the node layer's include fan-out doesn't force a rebuild of every subsystem, and
// vice-versa.
IMPLEMENT_MODULE(FDefaultModuleImpl, ExtendedEOSBlueprints);
