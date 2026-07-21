// Copyright Moon Punch Games. All Rights Reserved.

#include "EEUILabCaptureProcessor.h"

#if WITH_EDITOR

#include "Framework/Application/SlateApplication.h"
#include "UILab/Input/EEUILabInputLog.h"
#include "Widgets/SWidget.h"
#include "Widgets/SWindow.h"

FEEUILabCaptureProcessor::FEEUILabCaptureProcessor(const TSharedRef<FEEUILabInputLog>& InLog)
	: Log(InLog)
{
}

void FEEUILabCaptureProcessor::SetCaptureContext(const TSharedPtr<SWidget>& InPanelRoot, const TSharedPtr<SWidget>& InFocusTarget)
{
	PanelRoot = InPanelRoot;
	FocusTarget = InFocusTarget;
}

void FEEUILabCaptureProcessor::SetCaptureEnabled(const bool bEnabled)
{
	if (bCaptureEnabled == bEnabled)
	{
		return;
	}
	bCaptureEnabled = bEnabled;
	HeldKeys.Reset();

	FEEUILabInputEvent Event;
	Event.EventType = TEXT("Capture");
	Event.Detail = bEnabled ? TEXT("Gamepad capture enabled") : TEXT("Gamepad capture released");
	Log->Add(MoveTemp(Event));
}

bool FEEUILabCaptureProcessor::IsLabWindowActive() const
{
	const TSharedPtr<SWidget> Panel = PanelRoot.Pin();
	if (!Panel.IsValid())
	{
		return false;
	}
	const TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(Panel.ToSharedRef());
	return Window.IsValid() && Window->IsActive();
}

bool FEEUILabCaptureProcessor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (!bCaptureEnabled || !InKeyEvent.GetKey().IsGamepadKey() || !IsLabWindowActive())
	{
		return false;
	}

	HeldKeys.Add(InKeyEvent.GetKey());

	// Escape chord: Special Left + Special Right held together releases capture.
	if (HeldKeys.Contains(EKeys::Gamepad_Special_Left) && HeldKeys.Contains(EKeys::Gamepad_Special_Right))
	{
		SetCaptureEnabled(false);
		OnCaptureReleased.Broadcast();
		return true;
	}

	// Steer gamepad focus into the hosted widget so the event routes there, then let normal
	// Slate routing proceed (the event scope records entered/handled state).
	const TSharedPtr<SWidget> Target = FocusTarget.Pin();
	if (Target.IsValid())
	{
		const TSharedPtr<SWidget> CurrentFocus = SlateApp.GetUserFocusedWidget(InKeyEvent.GetUserIndex());

		bool bIsDescendant = false;
		for (TSharedPtr<SWidget> Walker = CurrentFocus; Walker.IsValid(); Walker = Walker->GetParentWidget())
		{
			if (Walker == Target)
			{
				bIsDescendant = true;
				break;
			}
		}

		if (!bIsDescendant)
		{
			SlateApp.SetUserFocus(InKeyEvent.GetUserIndex(), Target, EFocusCause::SetDirectly);
		}
	}

	return false;
}

bool FEEUILabCaptureProcessor::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	HeldKeys.Remove(InKeyEvent.GetKey());
	return false;
}

#endif // WITH_EDITOR
