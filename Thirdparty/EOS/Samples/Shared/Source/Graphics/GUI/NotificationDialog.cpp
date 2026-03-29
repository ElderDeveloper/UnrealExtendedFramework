// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "Main.h"
#include "Game.h"
#include "GameEvent.h"
#include "Menu.h"
#include "Sprite.h"
#include "TextLabel.h"
#include "Button.h"
#include "NotificationDialog.h"

FNotificationDialog::FNotificationDialog(Vector2 DialogPos,
						 Vector2 DialogSize,
						 UILayer DialogLayer,
						 FontPtr DialogNormalFont,
						 FontPtr DialogSmallFont) :
	FDialog(DialogPos, DialogSize, DialogLayer),
	UpdateNotificationsTimer(0.f)
{
	Background = std::make_shared<FSpriteWidget>(
		Position,
		Size,
		DialogLayer - 1,
		L"Assets/textfield.dds");
	AddWidget(Background);

	Label = std::make_shared<FTextLabelWidget>(
		Position,
		Vector2(Size.x, 30.f),
		DialogLayer - 1,
		L"",
		L"");
	Label->SetFont(DialogSmallFont);
	AddWidget(Label);
}

void FNotificationDialog::Update()
{
	UpdateNotifications();
}

void FNotificationDialog::SetPosition(Vector2 Pos)
{
	IWidget::SetPosition(Pos);

	Background->SetPosition(Position);
	Label->SetPosition(Vector2(Position.x, Position.y + (Label->GetSize().y / 2.f)));
}

void FNotificationDialog::SetLabelText(const std::wstring& LabelText)
{
	Label->SetText(LabelText);
}

void FNotificationDialog::SetLabelTextColor(FColor Col)
{
	Label->SetTextColor(Col);
}

void FNotificationDialog::OnGameEvent(const FGameEvent& Event)
{
	if (Event.GetType() == EGameEventType::AddNotification)
	{
		std::wstring LabelText = Event.GetFirstStr();
		NotificationQueue.push(LabelText);

		if (NotificationQueue.size() == 1)
		{
			// First notification so show right away
			ShowNextNotification();
		}
	}
}

void FNotificationDialog::UpdateNotifications()
{
	if (!NotificationQueue.empty())
	{
		UpdateNotificationsTimer += static_cast<float>(Main->GetTimer().GetElapsedSeconds());

		if (UpdateNotificationsTimer > UpdateNotificationsTime)
		{
			// Remove previous
			NotificationQueue.pop();

			// Show next (if there is one)
			ShowNextNotification();
		}
	}
}

void FNotificationDialog::ShowNextNotification()
{
	if (!NotificationQueue.empty())
	{
		std::wstring LabelText = NotificationQueue.front();

		SetLabelText(LabelText);

		Show();

		UpdateNotificationsTimer = 0.f;
	}
	else
	{
		Hide();
	}
}