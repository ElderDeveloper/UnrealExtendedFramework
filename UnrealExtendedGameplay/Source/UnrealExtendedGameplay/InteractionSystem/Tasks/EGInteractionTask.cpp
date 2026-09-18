// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGInteractionTask.h"

void UEGInteractionTask::OnDestroy(bool bInOwnerFinished)
{
	bOwnerFinished = bInOwnerFinished;
	Super::OnDestroy(bInOwnerFinished);
}
