// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "InputCoreTypes.h"
#include "Widgets/SCompoundWidget.h"

/**
 * UL-4 virtual controller: on-screen gamepad controls that synthesize real Slate key events
 * (down+up) for user 0 — D-pad, sticks (as digital nudges), face buttons, shoulders/triggers,
 * and menu/view. Focus is steered to the hosted widget before injection so events route there.
 */
class SEEUILabVirtualController : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEEUILabVirtualController) {}
		/** The hosted widget content that should receive the synthesized input (resolved live). */
		SLATE_ATTRIBUTE(TSharedPtr<SWidget>, FocusTarget)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> MakePadButton(const FText& Label, FKey Key, const FText& Tooltip);
	FReply InjectKey(FKey Key);

	TAttribute<TSharedPtr<SWidget>> FocusTarget;
};

#endif // WITH_EDITOR
