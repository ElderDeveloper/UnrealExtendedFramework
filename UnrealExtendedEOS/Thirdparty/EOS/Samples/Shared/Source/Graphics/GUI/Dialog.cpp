// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "Dialog.h"
#include "GfxComponent.h"
#include "UIEvent.h"
#include "Widget.h"
#include "VectorRender.h"
#include "Game.h"


void FDialog::OnUIEvent(const FUIEvent& Event)
{
	if (!bShown || !bEnabled)
	{
		return;
	}

	if (Event.GetType() == EUIEventType::KeyPressed && Event.GetKey() == FInput::Escape)
	{
		OnEscapePressed();
	}

	if (Event.GetType() == EUIEventType::KeyPressed && Event.GetKey() == FInput::Tab)
	{
		if (FocusedWidget && Widgets.size() > 1)
		{
			size_t FoundIndex = 0;
			for (size_t Index = 0; Index < Widgets.size(); ++Index)
			{
				if (FocusedWidget == Widgets[Index])
				{
					FoundIndex = Index;
					break;
				}
			}
			FoundIndex++;
			if (FoundIndex >= Widgets.size())
			{
				FoundIndex = 0;
			}

			FocusedWidget->SetFocused(false);
			FocusedWidget = Widgets[FoundIndex];
			FocusedWidget->SetFocused(true);
		}
	}

	if (Event.GetType() == EUIEventType::MousePressed)
	{
		// Search for widget (multiple targets are possible if widgets overlap)
		bool bFoundTarget = false;
		for (WidgetPtr Widget : Widgets)
		{
			if (Widget && Widget->IsShown())
			{
				if (Widget->CheckCollision(Event.GetVector()) && Widget->IsEnabled())
				{
					Widget->OnUIEvent(Event);
					FocusedWidget = Widget;
					bFoundTarget = true;
					Widget->SetFocused(true);
				}
				else
				{
					Widget->SetFocused(false);
				}
			}
		}

		if (!bFoundTarget)
		{
			FocusedWidget = nullptr;
		}
	}
	else if (Event.GetType() == EUIEventType::KeyPressed ||
				Event.GetType() == EUIEventType::MouseWheelScrolled ||
				Event.GetType() == EUIEventType::MouseReleased ||
				Event.GetType() == EUIEventType::TextInput)
	{
		if (FocusedWidget)
		{
			FocusedWidget->OnUIEvent(Event);
		}
	}
	else
	{
		// We just broadcast other events
		for (WidgetPtr Widget : Widgets)
		{
			if (Widget)
			{
				Widget->OnUIEvent(Event);
			}
		}
	}
}

void FDialog::OnEscapePressed()
{
	//Do nothing
}
