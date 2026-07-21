// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Semantic UI command/event names shared by runtime UI and the editor UI Lab.
 *
 * Interaction scripts record these semantic operations instead of raw keys, and runtime widgets
 * may emit them as analytics/telemetry events. Kept as plain FNames (not gameplay tags) so the
 * runtime module gains no new dependencies.
 */
namespace EFUISemantics
{
	// Commands (input intent)
	inline const FName Accept(TEXT("UI.Command.Accept"));
	inline const FName Back(TEXT("UI.Command.Back"));
	inline const FName NavigateUp(TEXT("UI.Command.NavigateUp"));
	inline const FName NavigateDown(TEXT("UI.Command.NavigateDown"));
	inline const FName NavigateLeft(TEXT("UI.Command.NavigateLeft"));
	inline const FName NavigateRight(TEXT("UI.Command.NavigateRight"));
	inline const FName NextTab(TEXT("UI.Command.NextTab"));
	inline const FName PreviousTab(TEXT("UI.Command.PreviousTab"));

	// Events (widget-emitted)
	inline const FName FocusChanged(TEXT("UI.Event.FocusChanged"));
	inline const FName ScreenActivated(TEXT("UI.Event.ScreenActivated"));
	inline const FName ScreenDeactivated(TEXT("UI.Event.ScreenDeactivated"));
	inline const FName ValueChanged(TEXT("UI.Event.ValueChanged"));
	inline const FName SelectionChanged(TEXT("UI.Event.SelectionChanged"));
}
