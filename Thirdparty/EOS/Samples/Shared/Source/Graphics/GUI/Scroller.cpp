// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "Main.h"
#include "Game.h"
#include "TextLabel.h"
#include "Button.h"
#include "Sprite.h"
#include "Scroller.h"

FScroller::FScroller(std::weak_ptr<IScrollable> scrollable, Vector2 position, Vector2 size, UILayer layer, std::wstring BarTextureFile) :
	IWidget(position, size, layer),
	Scrollable(scrollable)
{
	// create scroller buttons
	const float ButtonSize = Size.x;
	const std::wstring ScrollerUpButtonTexture = L"Assets/scroll_up_button.dds";
	const std::wstring ScrollerDownButtonTexture = L"Assets/scroll_down_button.dds";

	ScrollUpButton = std::make_unique<FButtonWidget>(
		Position,
		Vector2(ButtonSize, ButtonSize),
		layer,
		L"",
		std::vector<std::wstring>({ ScrollerUpButtonTexture }),
		nullptr,
		FColor(1.f, 1.f, 1.f, 1.f));

	ScrollDownButton = std::make_unique<FButtonWidget>(
		Vector2(Position.x, Position.y + Size.y - ButtonSize),
		Vector2(ButtonSize, ButtonSize),
		layer,
		L"",
		std::vector<std::wstring>({ ScrollerDownButtonTexture }),
		nullptr,
		FColor(1.f, 1.f, 1.f, 1.f));

	ScrollUpButton->SetOnPressedCallback([this]() { this->ScrollUp(); });
	ScrollDownButton->SetOnPressedCallback([this]() { this->ScrollDown(); });

	ScrollBarImage = std::make_shared<FSpriteWidget>(Vector2(0.f, 0.f), Vector2(size.x, size.x), layer, BarTextureFile.c_str());

	bBeingDraggedWithMouse = false;
}

void FScroller::Create()
{
	ScrollBarImage->Create();

	ScrollUpButton->Create();
	ScrollDownButton->Create();
}

void FScroller::Release()
{
	ScrollBarImage->Release();
	ScrollBarImage.reset();

	ScrollUpButton->Release();
	ScrollUpButton.reset();

	ScrollDownButton->Release();
	ScrollDownButton.reset();
}

void FScroller::Update()
{
	if (!IsShown())
		return;

	ScrollBarImage->Update();

	ScrollUpButton->Update();
	ScrollDownButton->Update();

	if (bBeingDraggedWithMouse)
	{
		Vector2 MousePosition = FGame::Get().GetInput()->GetMousePosition();
		float MouseY = MousePosition.y;
		float NewBarPosY = MouseY - DraggingInitialClickOffset;

		//clamp
		const float ButtonSize = Size.x;
		const float TopY = (Position.y + ButtonSize);
		const float BottomY = (Position.y + Size.y - ButtonSize);
		if (NewBarPosY < TopY)
		{
			NewBarPosY = TopY;
		}
		else if (NewBarPosY > BottomY)
		{
			NewBarPosY = BottomY;
		}

		if (auto ScrollableLocked = Scrollable.lock())
		{
			if (ScrollableLocked->NumEntries() > 0)
			{
				size_t NewFirstViewed = size_t((NewBarPosY - TopY) / (BottomY - TopY) * ScrollableLocked->NumEntries());
				if (NewFirstViewed != ScrollableLocked->FirstViewedEntry())
				{
					if (NewFirstViewed > ScrollableLocked->FirstViewedEntry())
					{
						ScrollableLocked->ScrollDown(NewFirstViewed - ScrollableLocked->FirstViewedEntry());
					}
					else
					{
						ScrollableLocked->ScrollUp(ScrollableLocked->FirstViewedEntry() - NewFirstViewed);
					}
				}
			}
		}
	}
}

void FScroller::Render(FSpriteBatchPtr& Batch)
{
	if (!IsShown())
		return;

	IWidget::Render(Batch);

	const float cButtonSize = Size.x;
	BarHeight = Size.y - cButtonSize * 2.0f;
	BarPosY = Position.y + cButtonSize;
	TotalScrollingHeight = (Size.y - cButtonSize * 2.0f);
	if (auto ScrollableLocked = Scrollable.lock())
	{
		if (ScrollableLocked->NumEntries() > 0)
		{
			BarHeight = BarHeight * (float(ScrollableLocked->LastViewedEntry() - ScrollableLocked->FirstViewedEntry() + 1) / ScrollableLocked->NumEntries());
			BarPosY = BarPosY + TotalScrollingHeight * (float(ScrollableLocked->FirstViewedEntry()) / ScrollableLocked->NumEntries());
		}
	}

	ScrollBarImage->SetPosition(Vector2(Position.x, BarPosY));
	ScrollBarImage->SetSize(Vector2(Size.x, BarHeight));
	ScrollBarImage->Render(Batch);

	ScrollUpButton->Render(Batch);
	ScrollDownButton->Render(Batch);
}

void FScroller::OnUIEvent(const FUIEvent& Event)
{
	if (!IsShown())
		return;
	
	if (Event.GetType() == EUIEventType::MousePressed)
	{
		//start dragging the scrollbar
		if (ScrollBarImage->CheckCollision(Event.GetVector()))
		{
			bBeingDraggedWithMouse = true;
			DraggingInitialClickOffset = Event.GetVector().y - BarPosY;
		}
		//check if we need to pass mouse click to buttons
		else if (ScrollUpButton->CheckCollision(Event.GetVector()))
		{
			ScrollUpButton->OnUIEvent(Event);
		}
		else if (ScrollDownButton->CheckCollision(Event.GetVector()))
		{
			ScrollDownButton->OnUIEvent(Event);
		}
		else
		{
			//handle click on empty space inside the scroller
			if (Event.GetVector().y < BarPosY)
			{
				ScrollUp();
			}
			else
			{
				ScrollDown();
			}
		}
	}
	else if (Event.GetType() == EUIEventType::MouseReleased)
	{
		//stop dragging the scrollbar
		bBeingDraggedWithMouse = false;
		DraggingInitialClickOffset = 0.0f;
	
		//check if we need to pass mouse click to buttons
		if (ScrollUpButton->CheckCollision(Event.GetVector()))
		{
			ScrollUpButton->OnUIEvent(Event);
		}
		else if (ScrollDownButton->CheckCollision(Event.GetVector()))
		{
			ScrollDownButton->OnUIEvent(Event);
		}
	}
	else if (Event.GetType() == EUIEventType::KeyPressed)
	{
		if (Event.GetKey() == FInput::Up)
		{
			ScrollUp();
		}
		else if (Event.GetKey() == FInput::Down)
		{
			ScrollDown();
		}
		else if (Event.GetKey() == FInput::PageUp)
		{
			if (auto ScrollableLocked = Scrollable.lock())
			{
				ScrollUp(ScrollableLocked->GetNumLinesPerPage());
			}
		}
		else if (Event.GetKey() == FInput::PageDown)
		{
			if (auto ScrollableLocked = Scrollable.lock())
			{
				ScrollDown(ScrollableLocked->GetNumLinesPerPage());
			}

		}
		else if (Event.GetKey() == FInput::Home)
		{
			ScrollToTop();
		}
		else if (Event.GetKey() == FInput::End)
		{
			ScrollToBottom();
		}
	}
	else if (Event.GetType() == EUIEventType::MouseWheelScrolled)
	{
		int ScrollValue = int(Event.GetVector().y);
		if (ScrollValue != 0)
		{
			if (auto ScrollableLocked = Scrollable.lock())
			{
				if (ScrollValue > 0)
				{
					ScrollableLocked->ScrollUp(size_t(ScrollValue));
				}
				else
				{
					ScrollableLocked->ScrollDown(size_t(-ScrollValue));
				}
			}
		}
	}

}

void FScroller::SetPosition(Vector2 Pos)
{
	IWidget::SetPosition(Pos);

	ScrollUpButton->SetPosition(Vector2(Pos.x, Pos.y));
	ScrollDownButton->SetPosition(Vector2(Pos.x, Pos.y + Size.y - Size.x));
}

void FScroller::SetSize(Vector2 NewSize)
{
	IWidget::SetSize(NewSize);
}

void FScroller::ScrollUp(size_t NumLines /* = 1 */)
{
	if (auto ScrollableLocked = Scrollable.lock())
	{
		ScrollableLocked->ScrollUp(NumLines);
	}
}

void FScroller::ScrollDown(size_t NumLines /* = 1 */)
{
	if (auto ScrollableLocked = Scrollable.lock())
	{
		ScrollableLocked->ScrollDown(NumLines);
	}
}

void FScroller::ScrollToTop()
{
	if (auto ScrollableLocked = Scrollable.lock())
	{
		ScrollableLocked->ScrollToTop();
	}
}

void FScroller::ScrollToBottom()
{
	if (auto ScrollableLocked = Scrollable.lock())
	{
		ScrollableLocked->ScrollToBottom();
	}
}
