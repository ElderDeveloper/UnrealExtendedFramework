// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "Framework/Application/IInputProcessor.h"

class FEEUILabInputLog;
class SWidget;

/**
 * UL-4 physical controller capture.
 *
 * While capture is enabled AND the UI Lab's window is active, gamepad key events are steered
 * into the hosted widget by forcing Slate user focus there before normal routing. The processor
 * never consumes gameplay-relevant events itself; it only redirects focus and logs.
 *
 * Escape chord: holding Gamepad Special Left + Special Right releases capture, so a pad can
 * never permanently steal editor navigation.
 */
class FEEUILabCaptureProcessor final
	: public TSharedFromThis<FEEUILabCaptureProcessor>
	, public IInputProcessor
{
public:
	FEEUILabCaptureProcessor(const TSharedRef<FEEUILabInputLog>& InLog);

	/** FocusTarget: the hosted widget content that should receive gamepad focus. */
	void SetCaptureContext(const TSharedPtr<SWidget>& InPanelRoot, const TSharedPtr<SWidget>& InFocusTarget);
	void SetCaptureEnabled(bool bEnabled);
	bool IsCaptureEnabled() const { return bCaptureEnabled; }

	DECLARE_MULTICAST_DELEGATE(FOnCaptureReleased);
	FOnCaptureReleased OnCaptureReleased;

	//~ Begin IInputProcessor Interface
	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	//~ End IInputProcessor Interface

private:
	bool IsLabWindowActive() const;

	TSharedPtr<FEEUILabInputLog> Log;
	TWeakPtr<SWidget> PanelRoot;
	TWeakPtr<SWidget> FocusTarget;
	TSet<FKey> HeldKeys;
	bool bCaptureEnabled = false;
};

#endif // WITH_EDITOR
