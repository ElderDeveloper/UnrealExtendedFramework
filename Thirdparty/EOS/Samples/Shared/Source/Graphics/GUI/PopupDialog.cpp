// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "Game.h"
#include "Menu.h"
#include "Sprite.h"
#include "TextLabel.h"
#include "TextView.h"
#include "Button.h"
#include "PopupDialog.h"

FPopupDialog::FPopupDialog(Vector2 DialogPos,
		Vector2 DialogSize,
		UILayer DialogLayer,
		const std::wstring& LabelText,
		FontPtr DialogNormalFont,
		FontPtr DialogSmallFont) :
	FDialog(DialogPos, DialogSize, DialogLayer)
{
	Background = std::make_shared<FSpriteWidget>(
		Position,
		Size,
		DialogLayer,
		L"Assets/textfield.dds");
	AddWidget(Background);

	MessageTextView = std::make_shared<FTextViewWidget>(
		Position,
		Vector2(150.f, 30.f),
		DialogLayer - 1,
		L"",
		L"",
		DialogSmallFont);

	MessageTextView->SetBorderOffsets(Vector2(1.0f, 1.0f));
	MessageTextView->SetScrollerOffsets(Vector2(1.f, 1.0f));
	MessageTextView->SetFont(DialogSmallFont);
	AddWidget(MessageTextView);

	FColor ButtonCol = FColor(0.f, 0.47f, 0.95f, 1.f);

	OkButton = std::make_shared<FButtonWidget>(
		Vector2(Position.x + 30.f, Position.y + 40.f),
		Vector2(100.f, 30.f),
		DialogLayer - 1,
		L"OK",
		assets::DefaultButtonAssets,
		DialogSmallFont,
		ButtonCol);
	OkButton->SetOnPressedCallback([this]()
	{
		Hide();
	});
	OkButton->SetBackgroundColors(assets::DefaultButtonColors);
	AddWidget(OkButton);
}

void FPopupDialog::SetPosition(Vector2 Pos)
{
	MessageTextView->SetSize(Vector2(GetSize().x, GetSize().y * 0.75f));

	IWidget::SetPosition(Pos);

	Background->SetPosition(Position);

	float MiddlePosX = Pos.x + ((GetSize().x) * 0.5f);
	float LabelPosX = MiddlePosX - (MessageTextView->GetSize().x * 0.4f);

	float MiddlePosY = Pos.y + ((GetSize().y) * 0.5f);
	float LabelPosY = MiddlePosY - (MessageTextView->GetSize().y * 0.45f);

	MessageTextView->SetPosition(Vector2(LabelPosX, LabelPosY));

	OkButton->SetPosition(Vector2(Position.x + 120, Position.y + 110));
}

void FPopupDialog::SetText(const std::wstring& NewText)
{
	if (MessageTextView)
	{
		MessageTextView->Clear();
		MessageTextView->AddLine(NewText);
	}
}
