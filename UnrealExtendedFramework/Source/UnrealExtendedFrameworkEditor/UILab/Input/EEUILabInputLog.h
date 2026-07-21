// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "InputCoreTypes.h"

/** One observed input event in the UI Lab session. */
struct FEEUILabInputEvent
{
	double Timestamp = 0.0;
	/** "KeyDown", "KeyUp", "Analog", "Capture", ... */
	FName EventType;
	FKey Key;
	float AnalogValue = 0.0f;
	/** True until the bubble phase reports the event escaped the hosted widget unhandled. */
	bool bHandledByWidget = true;
	/** Extra info (focus widget name, capture state changes, ...). */
	FString Detail;
};

/**
 * Ring-buffer log of input events flowing through the hosted widget, shared by the
 * event scope (writer), capture processor (writer), and input inspector (reader).
 */
class FEEUILabInputLog
{
public:
	DECLARE_MULTICAST_DELEGATE(FOnChanged);

	static constexpr int32 MaxEvents = 256;

	void Add(FEEUILabInputEvent Event)
	{
		Event.Timestamp = FPlatformTime::Seconds();
		Events.Add(MakeShared<FEEUILabInputEvent>(MoveTemp(Event)));
		if (Events.Num() > MaxEvents)
		{
			Events.RemoveAt(0, Events.Num() - MaxEvents);
		}
		OnChanged.Broadcast();
	}

	/** Marks the most recent matching entry as unhandled (bubble phase escaped the widget). */
	void MarkUnhandled(const FName EventType, const FKey& Key)
	{
		for (int32 i = Events.Num() - 1; i >= 0; --i)
		{
			if (Events[i]->EventType == EventType && Events[i]->Key == Key)
			{
				Events[i]->bHandledByWidget = false;
				OnChanged.Broadcast();
				return;
			}
		}
	}

	void Clear()
	{
		Events.Reset();
		OnChanged.Broadcast();
	}

	const TArray<TSharedPtr<FEEUILabInputEvent>>& GetEvents() const { return Events; }

	FOnChanged OnChanged;

private:
	TArray<TSharedPtr<FEEUILabInputEvent>> Events;
};

#endif // WITH_EDITOR
