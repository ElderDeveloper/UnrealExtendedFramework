// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

/**
 * UX-1 / UX-2 entry points (Update Plan v2):
 *  - "UI Lab" button in the UMG designer toolbar, next to Widget Reflector;
 *  - Content Browser context actions: "Open in Extended UI Lab" on Widget Blueprints,
 *    "Open Setup in UI Lab" on UEFUIFixture assets, and "Open in UI Lab Scripts" on
 *    UEFUIInteractionScript assets.
 *
 * Registered through the ToolMenus startup callback by the UI Lab feature.
 */
namespace EEUILabMenuExtensions
{
	void Register();
	void Unregister();
}

#endif // WITH_EDITOR
