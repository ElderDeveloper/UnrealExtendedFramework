// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "Main.h"
#include "Font.h"
#include "TextLabel.h"
#include "Button.h"
#include "Sprite.h"

constexpr FColor FButtonWidget::ButtonBackDisabledCol;

FButtonWidget::FButtonWidget(Vector2 ButtonPos,
							 Vector2 ButtonSize,
							 UILayer Layer,
							 const std::wstring& Text,
							 const std::vector<std::wstring>& Textures,
							 FontPtr Font,
							 FColor ButtonBackCol,
							 FColor ButtonTextCol,
							 EAlignmentType LabelAllignmentType) :
	IWidget(ButtonPos, ButtonSize, Layer)
{
	BackgroundColors.resize(size_t(EButtonVisualState::Last));
	BackgroundColors[size_t(EButtonVisualState::Idle)] = ButtonBackCol;
	BackgroundColors[size_t(EButtonVisualState::Hovered)] = ButtonBackCol;
	BackgroundColors[size_t(EButtonVisualState::Pressed)] = ButtonBackCol;
	BackgroundColors[size_t(EButtonVisualState::Disabled)] = ButtonBackDisabledCol;

	if (Textures.size() == 1)
	{
		bAnimated = false;
		Label = std::make_shared<FTextLabelWidget>(ButtonPos, ButtonSize, Layer, Text, Textures[0], ButtonBackCol, ButtonTextCol, LabelAllignmentType);
	}
	else
	{
		bAnimated = true;
		Label = std::make_shared<FTextLabelWidget>(ButtonPos, ButtonSize, Layer, Text, Textures, ButtonBackCol, ButtonTextCol, LabelAllignmentType);
	}

	if (Font != nullptr)
	{
		Label->SetFont(Font);
	}
}

void FButtonWidget::Create()
{
	if (Label)
	{
		Label->Create();
	}
}

void FButtonWidget::Release()
{
	if (Label)
	{
		Label->Release();
	}
}

void FButtonWidget::Update()
{
	if (Label)
	{
		Label->Update();
	}

	//Update hovering state
	if (!bPressed)
	{
		bool bMouseWasHoveredLastFrame = bMouseWasHovered;
		bMouseWasHovered = IsMouseHovered();
		if (bMouseWasHovered != bMouseWasHoveredLastFrame)
		{
			if (IsEnabled())
			{
				Label->SetBackgroundColor(BackgroundColors[size_t((bMouseWasHovered) ? EButtonVisualState::Hovered : EButtonVisualState::Idle)]);
			}
		}
	}
}

void FButtonWidget::Render(FSpriteBatchPtr& Batch)
{
	if (!IsShown())
		return;

	IWidget::Render(Batch);

	if (Label)
	{
		Label->Render(Batch);
	}
}

#ifdef _DEBUG
void FButtonWidget::DebugRender()
{
	IWidget::DebugRender();

	Label->DebugRender();
}
#endif

void FButtonWidget::OnUIEvent(const FUIEvent& Event)
{
	if (!IsShown())
		return;

	if (Event.GetType() == EUIEventType::MousePressed)
	{
		if (IsEnabled())
		{
			if (bAnimated)
			{
				std::shared_ptr<FSpriteWidget> Sprite = Label->GetBackgroundImage();
				if (Sprite)
				{
					FAnimatedTexturePtr AnimatedTexture = Sprite->GetAnimatedTexture();
					if (AnimatedTexture)
					{
						AnimatedTexture->SetFrame(1);
					}
				}
			}

			Label->SetBackgroundColor(BackgroundColors[size_t(EButtonVisualState::Pressed)]);
			bPressed = true;
		}
	}

	if ((Event.GetType() == EUIEventType::MouseReleased && bPressed) ||
		(IsFocused() && Event.GetType() == EUIEventType::KeyPressed && Event.GetKey() == FInput::Enter))
	{
		if (IsEnabled())
		{
			if (bAnimated)
			{
				std::shared_ptr<FSpriteWidget> Sprite = Label->GetBackgroundImage();
				if (Sprite)
				{
					FAnimatedTexturePtr AnimatedTexture = Sprite->GetAnimatedTexture();
					if (AnimatedTexture)
					{
						if (AnimatedTexture->GetCurrentFrame() == 0)
						{
							AnimatedTexture->Play(15.0f, false);
						}
						else
						{
							AnimatedTexture->SetFrame(0);
						}
					}
				}
			}

			Label->SetBackgroundColor(BackgroundColors[size_t((bMouseWasHovered) ? EButtonVisualState::Hovered : EButtonVisualState::Idle)]);

			if (OnPressedCallback)
			{
				OnPressedCallback();
			}
		}
		bPressed = false;
	}
}

void FButtonWidget::SetPosition(Vector2 Pos)
{
	IWidget::SetPosition(Pos);

	if (Label)
	{
		Label->SetPosition(Vector2(Pos.x, Pos.y));
	}
}

void FButtonWidget::SetSize(Vector2 NewSize)
{
	IWidget::SetSize(NewSize);

	if (Label)
	{
		Label->SetSize(NewSize);
	}
}

void FButtonWidget::Enable()
{
	IWidget::Enable();

	if (Label)
	{
		Label->SetBackgroundColor(BackgroundColors[size_t(EButtonVisualState::Idle)]);
	}
}

void FButtonWidget::Disable()
{
	IWidget::Disable();

	if (Label)
	{
		Label->SetBackgroundColor(BackgroundColors[size_t(EButtonVisualState::Disabled)]);
	}
}

void FButtonWidget::SetFocused(bool bFocused)
{
	IWidget::SetFocused(bFocused);

	if (Label)
	{
		FColor Col = BackgroundColors[size_t(EButtonVisualState::Idle)];
		if (!IsEnabled())
		{
			Col = BackgroundColors[size_t(EButtonVisualState::Disabled)];
		}
		else
		{
			if (bFocused)
			{
				Col = BackgroundColors[size_t(EButtonVisualState::Hovered)];
			}
			if (bPressed)
			{
				Col = BackgroundColors[size_t(EButtonVisualState::Pressed)];
			}
		}
		Label->SetBackgroundColor(Col);
	}
}

void FButtonWidget::SetOnPressedCallback(std::function<void()> Callback)
{
	OnPressedCallback = Callback;
}
