// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GfxComponent.h"
#include "Widget.h"

class FUIEvent;

// Dialog (widget container)
class FDialog : public IWidget
{
public:
	FDialog(Vector2 Position, Vector2 Size, UILayer Layer) :IWidget(Position, Size, Layer) {}
	FDialog(const IWidget&) = delete;
	FDialog& operator=(const IWidget&) = delete;

	void AddWidget(WidgetPtr Widget)
	{
		if (Widget)
		{
			Widgets.push_back(Widget);
			//TODO: move all AddWidget calls from constructors to be able use this:
			//Widget->SetParent(std::weak_ptr<FDialog>(std::static_pointer_cast<FDialog>(shared_from_this())));
		}
	}

	template <typename WidgetType>
	void RemoveWidgets(const std::vector<std::shared_ptr<WidgetType>>& WidgetsToRemove)
	{
		for (WidgetPtr& NextCandidate : Widgets)
		{
			if (std::find(WidgetsToRemove.begin(), WidgetsToRemove.end(), NextCandidate) != WidgetsToRemove.end())
			{
				NextCandidate.reset();
			}
		}

		//Clear empty pointers from vector
		std::vector<WidgetPtr>::iterator iter = std::remove_if(Widgets.begin(), Widgets.end(), [](const WidgetPtr& Ptr) { return !Ptr; });
		if (iter != Widgets.end())
		{
			Widgets.erase(iter, Widgets.end());
		}
	}

	size_t GetNumWidgets() const
	{
		return Widgets.size();
	}

	WidgetPtr GetWidget(size_t Index)
	{
		if (Index < Widgets.size())
		{
			return Widgets[Index];
		}

		return nullptr;
	}

	virtual void Create() override
	{
		for (WidgetPtr Widget : Widgets)
		{
			if (Widget)
			{
				Widget->Create();
			}
		}
	}

	virtual void Release() override
	{
		for (WidgetPtr Widget : Widgets)
		{
			if (Widget)
			{
				Widget->Release();
			}
		}
	}

	virtual void Update() override
	{
		if (!bShown)
		{
			return;
		}

		for (WidgetPtr Widget : Widgets)
		{
			if (Widget)
			{
				Widget->Update();
			}
		}
	}

	virtual void Render(FSpriteBatchPtr& Batch) override
	{
		if (!bShown)
		{
			return;
		}

		IWidget::Render(Batch);

		for (WidgetPtr Widget : Widgets)
		{
			if (Widget && Widget->IsShown())
			{
				Widget->Render(Batch);
			}
		}
	}

#ifdef _DEBUG
	virtual void DebugRender() override
	{
		if (!bShown)
		{
			return;
		}

		IWidget::DebugRender();

		for (WidgetPtr Widget : Widgets)
		{
			if (Widget)
			{
				Widget->DebugRender();
			}
		}
	}
#endif

	virtual void OnUIEvent(const FUIEvent& Event) override;
	virtual void OnEscapePressed();

 	bool IsWidgetFocused(WidgetPtr Widget)
 	{
 		if (FocusedWidget && FocusedWidget == Widget)
 		{
 			return true;
 		}
 		return false;
 	}

	virtual void SetFocused(bool bValue) override
	{
		IWidget::SetFocused(bValue);

		if (!bValue)
		{
			FocusedWidget.reset();
			for (WidgetPtr Widget : Widgets)
			{
				Widget->SetFocused(false);
			}
		}
	}

	virtual void Enable() override
	{
		IWidget::Enable();

		//Enabling dialog enables all its child widgets
		EnableWidgets();
	}

	virtual void Disable() override
	{
		IWidget::Disable();

		//Disabling dialog disables all its child widgets
		DisableWidgets();
	}

	void DisableWidgets()
	{
		for (WidgetPtr Widget : Widgets)
		{
			if (Widget)
			{
				Widget->Disable();
			}
		}
	}

	void EnableWidgets()
	{
		for (WidgetPtr Widget : Widgets)
		{
			if (Widget)
			{
				Widget->Enable();
			}
		}
		SavedEnabledWidgets.clear();
	}

	void DisableOtherWidgets(WidgetPtr TheWidget)
	{
		for (WidgetPtr Widget : Widgets)
		{
			if (Widget != TheWidget)
			{
				Widget->Disable();
			}
		}
	}

	void DisableWidgetsAndSave()
	{
		SavedEnabledWidgets.clear();
		for (WidgetPtr Widget : Widgets)
		{
			if (Widget)
			{
				if (Widget->IsEnabled())
				{
					SavedEnabledWidgets.emplace_back(Widget);
				}
				Widget->Disable();
			}
		}
	}

	void DisableOtherWidgetsAndSave(WidgetPtr TheWidget)
	{
		SavedEnabledWidgets.clear();
		for (WidgetPtr Widget : Widgets)
		{
			if (Widget != TheWidget)
			{
				if (Widget->IsEnabled())
				{
					SavedEnabledWidgets.emplace_back(Widget);
				}
				Widget->Disable();
			}
		}
	}

	void ReEnableWidgets()
	{
		for (WidgetPtr Widget : SavedEnabledWidgets)
		{
			if (Widget)
			{
				Widget->Enable();
			}
		}
		SavedEnabledWidgets.clear();
	}

	virtual bool CheckCollision(Vector2 Position) const override
	{
		for (WidgetPtr Widget : Widgets)
		{
			if (Widget)
			{
				if (Widget->CheckCollision(Position))
				{
					return true;
				}
			}
		}

		return false;
	}

protected:
	std::vector<WidgetPtr> Widgets;
	std::vector<WidgetPtr> SavedEnabledWidgets;
	WidgetPtr FocusedWidget;
};

using DialogPtr = std::shared_ptr<FDialog>;