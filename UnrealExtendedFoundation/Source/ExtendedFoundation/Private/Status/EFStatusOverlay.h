// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** The on-screen Extended.Status overlay (Extended.Status.Overlay 1). Development builds only. */
namespace EFStatusOverlay
{
	void SetVisible(bool bVisible);
	bool IsVisible();

	/** Removes the overlay's draw hook; called when the foundation module shuts down. */
	void Shutdown();
}
