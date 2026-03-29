// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "Game.h"
#include "TextLabel.h"
#include "Button.h"
#include "UIEvent.h"
#include "GameEvent.h"
#include "AccountHelpers.h"
#include "Player.h"
#include "FriendList.h"
#include "FriendsDialog.h"

FFriendsDialog::FFriendsDialog(
	Vector2 DialogPos,
	Vector2 DialogSize,
	UILayer DialogLayer,
	FontPtr DialogNormalFont,
	FontPtr DialogSmallFont,
	FontPtr DialogTinyFont) :
	FDialog(DialogPos, DialogSize, DialogLayer)
{
	FriendsListWidget = std::make_shared<FFriendListWidget>(
		Position,
		Size,
		DialogLayer,
		DialogNormalFont,
		DialogSmallFont,
		DialogSmallFont,
		DialogTinyFont);
	FriendsListWidget->SetBottomOffset(100.0f);

	FriendsListWidget->SetBorderColor(Color::UIBorderGrey);

	FriendsListWidget->Create();
	AddWidget(FriendsListWidget);
}

void FFriendsDialog::SetPosition(Vector2 Pos)
{
	IWidget::SetPosition(Pos);

	if (FriendsListWidget)
	{
		FriendsListWidget->SetPosition(Pos);
	}
}

void FFriendsDialog::SetSize(Vector2 NewSize)
{
	IWidget::SetSize(NewSize);

	if (FriendsListWidget)
	{
		FriendsListWidget->SetSize(NewSize);
	}
}

void FFriendsDialog::OnGameEvent(const FGameEvent& Event)
{
	if (Event.GetType() == EGameEventType::UserLoggedIn)
	{
		SetFriendInfoVisible(true);
	}
	else if (Event.GetType() == EGameEventType::UserLoginRequiresMFA)
	{
		SetFriendInfoVisible(false);
		SetFocused(false);
	}
	else if (Event.GetType() == EGameEventType::UserLoginEnteredMFA)
	{
		SetFriendInfoVisible(true);
	}
	else if (Event.GetType() == EGameEventType::UserLoggedOut)
	{
		if (FPlayerManager::Get().GetNumPlayers() == 0)
		{
			Clear();
			SetFriendInfoVisible(false);
		}
	}
	else if (Event.GetType() == EGameEventType::ShowPrevUser)
	{
		Clear();
		Reset();
	}
	else if (Event.GetType() == EGameEventType::ShowNextUser)
	{
		Clear();
		Reset();
	}
	else if (Event.GetType() == EGameEventType::NewUserLogin)
	{
		SetFriendInfoVisible(false);
		Clear();
	}
	else if (Event.GetType() == EGameEventType::CancelLogin)
	{
		Clear();
		Reset();
	}
	else if (Event.GetType() == EGameEventType::NoUserLoggedIn)
	{
		SetFriendInfoVisible(false);
		Clear();
	}
}

void FFriendsDialog::SetFriendInfoVisible(bool bVisible)
{
	if (FriendsListWidget)
	{
		FriendsListWidget->SetFriendInfoVisible(bVisible);
	}
}

void FFriendsDialog::Reset()
{
	if (FriendsListWidget)
	{
		FriendsListWidget->SetFriendInfoVisible(true);
		FriendsListWidget->Reset();
	}
}

void FFriendsDialog::Clear()
{
	if (FriendsListWidget)
	{
		FriendsListWidget->RefreshFriendData(std::vector<FFriendData>());
		FriendsListWidget->ClearFilter();
		FriendsListWidget->Reset();
	}
}