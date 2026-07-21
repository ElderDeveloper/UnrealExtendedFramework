// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "Main.h"
#include "Font.h"
#include "TextLabel.h"
#include "Sprite.h"
#include "Checkbox.h"

FCheckboxWidget::FCheckboxWidget(Vector2 CheckboxPos,
	Vector2 Size,
	UILayer Layer,
	const std::wstring& Text,
	const std::wstring& LabelBackTexture,
	FontPtr Font,
	const std::wstring& TickedTextureFile,
	const std::wstring& UntickedTextureFile,
	FColor LabelBackCol,
	FColor LabelTextCol) :
	IWidget(CheckboxPos, Size, Layer)
{
	TickedSprite = std::make_shared<FSpriteWidget>(CheckboxPos, Vector2(Size.y), Layer, TickedTextureFile);
	TickedSprite->Hide();
	UntickedSprite = std::make_shared<FSpriteWidget>(TickedSprite->GetPosition(), TickedSprite->GetSize(), Layer, UntickedTextureFile);
	Label = std::make_shared<FTextLabelWidget>(CheckboxPos + Vector2(TickedSprite->GetSize().x, 0.0f), Vector2(Size.x - TickedSprite->GetSize().x, Size.y), Layer, Text, LabelBackTexture, LabelBackCol, LabelTextCol);

	bTicked = false;

	if (Font != nullptr)
	{
		Label->SetFont(Font);
	}
}

void FCheckboxWidget::Create()
{
	if (Label)
	{
		Label->Create();
	}

	if (TickedSprite)
	{
		TickedSprite->Create();
	}

	if (UntickedSprite)
	{
		UntickedSprite->Create();
	}
}

void FCheckboxWidget::Release()
{
	if (Label)
	{
		Label->Release();
	}

	if (TickedSprite)
	{
		TickedSprite->Release();
	}

	if (UntickedSprite)
	{
		UntickedSprite->Release();
	}
}

void FCheckboxWidget::Update()
{
	if (bTicked)
	{
		TickedSprite->Show();
		UntickedSprite->Hide();
	}
	else
	{
		TickedSprite->Hide();
		UntickedSprite->Show();
	}
}

void FCheckboxWidget::Render(FSpriteBatchPtr& Batch)
{
	if (!IsShown())
		return;

	IWidget::Render(Batch);

	if (Label)
	{
		Label->Render(Batch);
	}

	if (TickedSprite && TickedSprite->IsShown())
	{
		TickedSprite->Render(Batch);
	}

	if (UntickedSprite && UntickedSprite->IsShown())
	{
		UntickedSprite->Render(Batch);
	}
}

#ifdef _DEBUG
void FCheckboxWidget::DebugRender()
{
	IWidget::DebugRender();

	Label->DebugRender();
	UntickedSprite->DebugRender();
	TickedSprite->DebugRender();
}
#endif

void FCheckboxWidget::OnUIEvent(const FUIEvent& Event)
{
	if (!IsShown())
		return;

	if (Event.GetType() == EUIEventType::MousePressed ||
		(IsFocused() && Event.GetType() == EUIEventType::KeyPressed && Event.GetKey() == FInput::Enter))
	{
		if (IsEnabled())
		{
			bTicked = !bTicked;
			if (OnTickedCallback)
			{
				OnTickedCallback(bTicked);
			}
		}
	}
}

void FCheckboxWidget::SetPosition(Vector2 Pos)
{
	IWidget::SetPosition(Pos);

	if (TickedSprite)
	{
		TickedSprite->SetPosition(Pos);
	}

	if (UntickedSprite)
	{
		UntickedSprite->SetPosition(Pos);
	}

	if (Label)
	{
		Label->SetPosition(Vector2(Pos.x + TickedSprite->GetSize().x, Pos.y));
	}


}

void FCheckboxWidget::SetSize(Vector2 NewSize)
{
	IWidget::SetSize(NewSize);

	if (TickedSprite)
	{
		TickedSprite->SetSize(Vector2(NewSize.y));
	}

	if (UntickedSprite)
	{
		UntickedSprite->SetSize(Vector2(NewSize.y));
	}

	if (Label)
	{
		Label->SetSize(Vector2(NewSize.x - TickedSprite->GetSize().x, NewSize.y));
	}
}

void FCheckboxWidget::SetOnTickedCallback(std::function<void(bool)> Callback)
{
	OnTickedCallback = Callback;
}

void FCheckboxWidget::SetTicked(bool bValue, bool bNotifyCallback /*= false*/)
{
	bTicked = bValue;
	if (bNotifyCallback && OnTickedCallback)
	{
		OnTickedCallback(bValue);
	}
}
