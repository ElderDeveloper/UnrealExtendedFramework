// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Widgets/SCompoundWidget.h"

class FEEUILabInputLog;

/**
 * Wraps the hosted widget to observe input routing.
 *
 * Preview (tunnel) overrides record that an event entered the hosted subtree; bubble overrides
 * fire only when no child handled the event, which downgrades the log entry to "unhandled".
 * The scope itself never consumes events.
 */
class SEEUILabEventScope : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEEUILabEventScope) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FEEUILabInputLog>& InLog);

	virtual bool SupportsKeyboardFocus() const override { return false; }
	virtual FReply OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnAnalogValueChanged(const FGeometry& MyGeometry, const FAnalogInputEvent& InAnalogInputEvent) override;

private:
	TSharedPtr<FEEUILabInputLog> Log;
};

#endif // WITH_EDITOR
