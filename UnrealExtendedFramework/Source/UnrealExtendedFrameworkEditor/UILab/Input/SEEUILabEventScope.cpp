// Copyright Moon Punch Games. All Rights Reserved.

#include "SEEUILabEventScope.h"

#if WITH_EDITOR

#include "UILab/Input/EEUILabInputLog.h"

void SEEUILabEventScope::Construct(const FArguments& InArgs, const TSharedRef<FEEUILabInputLog>& InLog)
{
	Log = InLog;

	ChildSlot
	[
		InArgs._Content.Widget
	];
}

FReply SEEUILabEventScope::OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	FEEUILabInputEvent Event;
	Event.EventType = TEXT("KeyDown");
	Event.Key = InKeyEvent.GetKey();
	Log->Add(MoveTemp(Event));
	return FReply::Unhandled();
}

FReply SEEUILabEventScope::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	// Bubble phase reached the scope: no hosted widget handled it.
	Log->MarkUnhandled(TEXT("KeyDown"), InKeyEvent.GetKey());
	return FReply::Unhandled();
}

FReply SEEUILabEventScope::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	FEEUILabInputEvent Event;
	Event.EventType = TEXT("KeyUp");
	Event.Key = InKeyEvent.GetKey();
	Event.bHandledByWidget = false;
	Log->Add(MoveTemp(Event));
	return FReply::Unhandled();
}

FReply SEEUILabEventScope::OnAnalogValueChanged(const FGeometry& MyGeometry, const FAnalogInputEvent& InAnalogInputEvent)
{
	// Only log meaningful deflection so stick noise doesn't flood the ring buffer.
	if (FMath::Abs(InAnalogInputEvent.GetAnalogValue()) > 0.25f)
	{
		FEEUILabInputEvent Event;
		Event.EventType = TEXT("Analog");
		Event.Key = InAnalogInputEvent.GetKey();
		Event.AnalogValue = InAnalogInputEvent.GetAnalogValue();
		Event.bHandledByWidget = false;
		Log->Add(MoveTemp(Event));
	}
	return FReply::Unhandled();
}

#endif // WITH_EDITOR
