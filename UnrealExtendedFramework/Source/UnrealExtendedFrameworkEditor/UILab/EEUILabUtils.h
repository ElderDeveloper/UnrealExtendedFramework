// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

class SWidget;
class UUserWidget;
class UWidget;

namespace EEUILabUtils
{
	/**
	 * Finds a widget in the tree (including nested UserWidgets) whose IEFWidgetAutomationIdentity
	 * matches the given identity. Returns null when not found.
	 */
	UNREALEXTENDEDFRAMEWORKEDITOR_API UWidget* FindWidgetByAutomationIdentity(UUserWidget& Root, FName Identity);

	/**
	 * Reverse lookup: the automation identity of the UMG widget whose Slate widget matches
	 * (or is an ancestor of) the given one. NAME_None when nothing matches.
	 */
	UNREALEXTENDEDFRAMEWORKEDITOR_API FName FindAutomationIdentityForSlateWidget(UUserWidget& Root, const TSharedPtr<SWidget>& SlateWidget);
}

#endif // WITH_EDITOR
