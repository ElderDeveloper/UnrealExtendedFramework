// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "Game.h"
#include "Menu.h"
#include "Sprite.h"
#include "TextLabel.h"
#include "Button.h"
#include "ExitDialog.h"

FExitDialog::FExitDialog(Vector2 DialogPos,
						 Vector2 DialogSize,
						 UILayer DialogLayer,
						 FontPtr DialogNormalFont,
						 FontPtr DialogSmallFont) :
	FDialog(DialogPos, DialogSize, DialogLayer)
{
	ExitBackground = std::make_shared<FSpriteWidget>(
		Position,
		Size,
		DialogLayer,
		L"Assets/textfield.dds");
	AddWidget(ExitBackground);

	ExitLabel = std::make_shared<FTextLabelWidget>(
		Position,
		Vector2(150.f, 30.f),
		DialogLayer - 1,
		L"Are you sure you want to exit?",
		L"");
	ExitLabel->SetFont(DialogNormalFont);
	AddWidget(ExitLabel);

	FColor ButtonCol = FColor(0.f, 0.47f, 0.95f, 1.f);

	ExitOkButton = std::make_shared<FButtonWidget>(
		Vector2(Position.x + 30.f, Position.y + 40.f),
		Vector2(100.f, 30.f),
		DialogLayer - 1,
		L"OK",
		assets::DefaultButtonAssets,
		DialogSmallFont,
		ButtonCol);
	ExitOkButton->SetOnPressedCallback([this]()
	{
		Hide();
		FGame::Get().Exit();
	});
	ExitOkButton->SetBackgroundColors(assets::DefaultButtonColors);
	AddWidget(ExitOkButton);

	ExitCancelButton = std::make_shared<FButtonWidget>(
		Vector2(Position.x + 150, Position.y + 40),
		Vector2(100.f, 30.f),
		DialogLayer - 1,
		L"CANCEL",
		assets::DefaultButtonAssets,
		DialogSmallFont,
		ButtonCol);
	ExitCancelButton->SetOnPressedCallback([this]()
	{
		Hide();
	});
	ExitCancelButton->SetBackgroundColors(assets::DefaultButtonColors);
	AddWidget(ExitCancelButton);
}

void FExitDialog::SetPosition(Vector2 Pos)
{
	ExitLabel->SetSize(Size);

	IWidget::SetPosition(Pos);

	ExitBackground->SetPosition(Position);
	ExitOkButton->SetPosition(Vector2(Position.x + 50, Position.y + 90));
	ExitCancelButton->SetPosition(Vector2(Position.x + 170, Position.y + 90));

	float MiddlePosX = Pos.x + ((GetSize().x) / 2.f);
	float LabelSizeX = ExitLabel->GetSize().x;
	float LabelPosX = MiddlePosX - (LabelSizeX / 2.f);
	ExitLabel->SetPosition(Vector2(LabelPosX, Position.y - 20));
}
