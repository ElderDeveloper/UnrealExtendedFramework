// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "Game.h"
#include "Menu.h"
#include "Sprite.h"
#include "TextLabel.h"
#include "Button.h"
#include "ProgressBar.h"
#include "TransferProgressDialog.h"

FTransferProgressDialog::FTransferProgressDialog(Vector2 DialogPos,
						 Vector2 DialogSize,
						 UILayer DialogLayer,
						 std::wstring InTransferName,
						 FontPtr DialogNormalFont,
						 FontPtr DialogSmallFont,
						 std::shared_ptr<FTransferProgressDialog::FDelegate> InDelegate) :
	FDialog(DialogPos, DialogSize, DialogLayer),
	TransferName(InTransferName),
	Delegate(InDelegate)
{
	ProgressBackground = std::make_shared<FSpriteWidget>(
		Position,
		DialogSize,
		DialogLayer - 1,
		L"Assets/textfield.dds");
	AddWidget(ProgressBackground);

	ProgressLabel = std::make_shared<FTextLabelWidget>(
		Vector2(Position.x + 30.0f, Position.y + 25.0f),
		Vector2(150.f, 30.f),
		DialogLayer - 1,
		L"File transfer progress: ",
		L"");
	ProgressLabel->SetFont(DialogNormalFont);
	AddWidget(ProgressLabel);


	ProgressBar = std::make_shared<FProgressBar>(
		Vector2(Position.x + 30.f, Position.y + 20.f),
		Vector2(256.0f, 32.0f),
		DialogLayer - 1,
		L"Transfer progress:",
		L"assets/progress_bar_finished.dds",
		L"assets/progress_bar_unfinished.dds",
		DialogSmallFont);
	AddWidget(ProgressBar);

	FColor ButtonCol = FColor(0.f, 0.47f, 0.95f, 1.f);
	CancelTransferButton = std::make_shared<FButtonWidget>(
		Vector2(Position.x + 190, Position.y + 105),
		Vector2(100.f, 30.f),
		DialogLayer - 1,
		L"CANCEL",
		assets::DefaultButtonAssets,
		DialogSmallFont,
		ButtonCol);
	CancelTransferButton->SetOnPressedCallback([this]()
	{
		if (Delegate)
		{
			Delegate->CancelTransfer();
		}
		Hide();
	});
	CancelTransferButton->SetBackgroundColors(assets::DefaultButtonColors);
	AddWidget(CancelTransferButton);
}

void FTransferProgressDialog::SetPosition(Vector2 Pos)
{
	IWidget::SetPosition(Pos);

	ProgressBackground->SetPosition(Position);
	ProgressLabel->SetPosition(Vector2(Position.x + 30.0f, Position.y + 25.0f));
	ProgressBar->SetPosition(Vector2(Position.x + 30, Position.y + 60.0f));
	CancelTransferButton->SetPosition(Vector2(Position.x + 190, Position.y + 105.0f));
}

void FTransferProgressDialog::Update()
{
	if (!IsShown())
	{
		return;
	}

	if (!Delegate)
	{
		return;
	}

	//Somehow we lost track on the file transfer that this dialog reflects.
	if (Delegate->GetCurrentTransferName() != TransferName)
	{
		Hide();
		return;
	}

	ProgressBackground->Update();
	ProgressLabel->Update();
	ProgressBar->SetProgress(Delegate->GetCurrentTransferProgress());
	ProgressBar->Update();
	CancelTransferButton->Update();
}

void FTransferProgressDialog::OnEscapePressed()
{
	if (Delegate)
	{
		Delegate->CancelTransfer();
	}
	Hide();
}