// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "Game.h"
#include "Menu.h"
#include "Main.h"
#include "TextLabel.h"
#include "TextView.h"
#include "Button.h"
#include "UIEvent.h"
#include "FriendList.h"
#include "Sprite.h"
#include "FriendInfo.h"
#include "GameEvent.h"

#ifdef EOS_SAMPLE_P2P
#include "P2PNATDialog.h"

// depends on FButtonWidget::EButtonVisualState
const std::vector<FColor> ChatButtonBackgroundColors = { assets::DefaultButtonColors[0], assets::DefaultButtonColors[1], assets::DefaultButtonColors[2], Color::UIBackgroundGrey };
#elif EOS_SAMPLE_VOICE
#include "VoiceDialog.h"

// depends on FButtonWidget::EButtonVisualState
const std::vector<FColor> ChatButtonBackgroundColors = { assets::DefaultButtonColors[0], assets::DefaultButtonColors[1], assets::DefaultButtonColors[2], Color::UIBackgroundGrey };
#endif

#ifdef EOS_SAMPLE_LOBBIES
#include "Lobbies.h"
#endif


FFriendInfoWidget::FFriendInfoWidget(Vector2 InfoPos,
									 Vector2 InfoSize,
									 UILayer InfoLayer,
									 FFriendData FriendData,
									 FontPtr InfoLargeFont,
									 FontPtr InfoSmallFont) :
	IWidget(InfoPos, InfoSize, InfoLayer),
	FriendData(FriendData),
	LargeFont(InfoLargeFont),
	SmallFont(InfoSmallFont)
{
	BackgroundImage = std::make_shared<FSpriteWidget>(Vector2(0.f, 0.f), Vector2(200.f, 100.f), InfoLayer, L"", Color::UIBackgroundGrey);
}

void FFriendInfoWidget::Create()
{
	BackgroundImage->Create();

	Button1.reset();
	Button2.reset();
	Button3.reset();

	bool bIsFriend = (FriendData.Status == EOS_EFriendsStatus::EOS_FS_Friends);
	bool bIsPlaceholder = (FriendData.bPlaceholder);

	Vector2 NameOffset = (bIsFriend) ?
		Vector2(1.f, 0.f) :
		Vector2(1.f, -2.f);

	Vector2 NameSize = (bIsFriend) ?
		Vector2(Size.x, Size.y / 2.f) :
		Size;

	if (bIsPlaceholder)
	{
		NameOffset = Vector2(50.f, -2.f);
		NameSize = Size;
	}

	NameLabel = std::make_shared<FTextLabelWidget>(
		Position + NameOffset,
		NameSize,
		Layer - 1,
		FriendData.Name,
		L"",
		FColor(0.5f, 0.5f, 0.5f, 1.f),
		FColor(1.f, 1.f, 1.f, 1.f),
		EAlignmentType::Left);

	NameLabel->Create();
	NameLabel->SetFont(LargeFont);
	NameLabel->SetText(FriendData.Name);

	if (!bIsPlaceholder)
	{
		if (FriendData.Status == EOS_EFriendsStatus::EOS_FS_InviteReceived ||
			FriendData.Status == EOS_EFriendsStatus::EOS_FS_InviteSent ||
			FriendData.Status == EOS_EFriendsStatus::EOS_FS_NotFriends)
		{
			const float ButtonSize = Size.y / 2.0f;
			const Vector2 RightButtonOffset(Size.x - ButtonSize * 1.5f, Size.y * 0.25f);
			const Vector2 LeftButtonOffset(Size.x - ButtonSize * 3.0f, Size.y * 0.25f);

			FEpicAccountId FriendId = FriendData.UserId;

			// Invite received means we can either accept it or decline so we need 2 buttons
			if (FriendData.Status == EOS_EFriendsStatus::EOS_FS_InviteReceived)
			{
				Button1 = std::make_shared<FButtonWidget>(
					Position + LeftButtonOffset,
					Vector2(ButtonSize, ButtonSize),
					Layer - 1,
					L"",
					std::vector<std::wstring>({ L"Assets/yesbutton.dds" }),
					SmallFont,
					FColor(1.f, 1.f, 1.f, 1.f));
				Button2 = std::make_shared<FButtonWidget>(
					Position + RightButtonOffset,
					Vector2(ButtonSize, ButtonSize),
					Layer - 1,
					L"",
					std::vector<std::wstring>({ L"Assets/nobutton.dds" }),
					SmallFont,
					FColor(1.f, 1.f, 1.f, 1.f));

				Button1->Create();
				Button2->Create();

				Button1->SetOnPressedCallback([FriendId]() { FGame::Get().GetFriends()->AcceptInvite(FriendId); });
				Button2->SetOnPressedCallback([FriendId]() { FGame::Get().GetFriends()->RejectInvite(FriendId); });
			}
			else if (FriendData.Status == EOS_EFriendsStatus::EOS_FS_InviteSent)
			{
				/*Button1 = std::make_shared<FButtonWidget>(
					Position + RightButtonOffset,
					Vector2(ButtonSize, ButtonSize),
					Layer - 1,
					L"",
					L"Assets/nobutton.dds",
					SmallFont,
					FColor(1.f, 1.f, 1.f, 1.f));
				Button1->Init();
				Button1->CreateResources();
				Button1->SetOnPressedCallback([FriendId]() { FGame::Get().GetFriends()->RemoveInvite(FriendId); });*/
			}
			else if (FriendData.Status == EOS_EFriendsStatus::EOS_FS_NotFriends)
			{
				Button1 = std::make_shared<FButtonWidget>(
					Position + RightButtonOffset,
					Vector2(ButtonSize, ButtonSize),
					Layer - 1,
					L"",
					std::vector<std::wstring>({ L"Assets/addbutton.dds" }),
					SmallFont,
					FColor(1.f, 1.f, 1.f, 1.f));
				Button1->Create();
				Button1->SetOnPressedCallback([FriendId]()
				{
					FGame::Get().GetFriends()->AddFriend(FriendId);
					FGame::Get().GetMenu()->OnUIEvent(FUIEvent(EUIEventType::FriendInviteSent));
				});
			}
		}
		else if (FriendData.Status == EOS_EFriendsStatus::EOS_FS_Friends)
		{
#ifdef EOS_SAMPLE_SESSIONS
			//Invite to session button (session matchmaking demo only)
			const float ButtonSize = Size.x * 0.2f;
			Vector2 ButtonOffset(Size.x - (ButtonSize * 4.f) - 15.0f, Size.y * 0.25f);

			FEpicAccountId FriendId = FriendData.UserId;
			FProductUserId FriendProductUserId = FriendData.UserProductUserId;

			if (FriendProductUserId.IsValid())
			{
				Button1 = std::make_shared<FButtonWidget>(
					Position + ButtonOffset,
					Vector2(ButtonSize, Size.y / 2.0f),
					Layer - 1,
					L"Invite to session",
					assets::DefaultButtonAssets,
					SmallFont,
					assets::DefaultButtonColors[static_cast<size_t>(FButtonWidget::EButtonVisualState::Idle)]);
				Button1->Create();
				Button1->SetOnPressedCallback([FriendProductUserId]()
				{
					FGameEvent Event(EGameEventType::InviteFriendToSession, FriendProductUserId);
					FGame::Get().OnGameEvent(Event);
				});
				Button1->SetBackgroundColors(assets::DefaultButtonColors);

				ButtonOffset.x = ButtonOffset.x + ButtonSize + 5.f;

				Button2 = std::make_shared<FButtonWidget>(
					Position + ButtonOffset,
					Vector2(ButtonSize, Size.y / 2.0f),
					Layer - 1,
					L"Register with session",
					assets::DefaultButtonAssets,
					SmallFont,
					assets::DefaultButtonColors[static_cast<size_t>(FButtonWidget::EButtonVisualState::Idle)]);
				Button2->Create();
				Button2->SetOnPressedCallback([FriendProductUserId]()
				{
					FGameEvent Event(EGameEventType::RegisterFriendWithSession, FriendProductUserId);
					FGame::Get().OnGameEvent(Event);
				});
				Button2->SetBackgroundColors(assets::DefaultButtonColors);

				ButtonOffset.x = ButtonOffset.x + ButtonSize + 5.f;

				Button3 = std::make_shared<FButtonWidget>(
					Position + ButtonOffset,
					Vector2(ButtonSize, Size.y / 2.0f),
					Layer - 1,
					L"Request To Join Session",
					assets::DefaultButtonAssets,
					SmallFont,
					assets::DefaultButtonColors[static_cast<size_t>(FButtonWidget::EButtonVisualState::Idle)]);
				Button3->Create();
				Button3->SetOnPressedCallback([FriendProductUserId]()
				{
					FGameEvent Event(EGameEventType::RequestToJoinFriendSession, FriendProductUserId);
					FGame::Get().OnGameEvent(Event);
				});
				Button3->SetBackgroundColors(assets::DefaultButtonColors);
			}
#elif defined(EOS_SAMPLE_P2P) || defined (EOS_SAMPLE_VOICE)
			//Start chat button (p2p demo only)
			const float ButtonSize = Size.x * 0.3f;
			Vector2 ButtonOffset(Size.x - ButtonSize - 10.0f, Size.y * 0.25f);

			FEpicAccountId FriendUserId = FriendData.UserId;
			FProductUserId FriendProductUserId = FriendData.UserProductUserId;
			std::wstring FriendName = FriendData.Name;
			if (FriendProductUserId.IsValid() && FriendData.Presence.Status != EOS_Presence_EStatus::EOS_PS_Offline)
			{
				Button1 = std::make_shared<FButtonWidget>(
					Position + ButtonOffset,
					Vector2(ButtonSize, Size.y / 2.0f),
					Layer - 1,
					L"CHAT",
					std::vector<std::wstring>({ L"Assets/button.dds" }),
					SmallFont,
					ChatButtonBackgroundColors[static_cast<size_t>(FButtonWidget::EButtonVisualState::Idle)]);
				Button1->Create();
				Button1->SetOnPressedCallback([FriendUserId, FriendProductUserId, FriendName]()
				{
					FGameEvent Event(EGameEventType::StartChatWithFriend, FriendUserId, FriendProductUserId, FriendName);
					FGame::Get().OnGameEvent(Event);
				});
				Button1->SetBackgroundColors(ChatButtonBackgroundColors);
			}
#elif defined(EOS_SAMPLE_LOBBIES)
		//Invite to lobby (lobbies demo only)
		const float ButtonSize = Size.x * 0.4f;
		Vector2 ButtonOffset(Size.x - ButtonSize - 10.0f, Size.y * 0.25f);

		FProductUserId FriendProductUserId = FriendData.UserProductUserId;
		if (FriendProductUserId.IsValid() && FriendData.Presence.Status != EOS_Presence_EStatus::EOS_PS_Offline)
		{
			Button1 = std::make_shared<FButtonWidget>(
				Position + ButtonOffset,
				Vector2(ButtonSize, Size.y / 2.0f),
				Layer - 1,
				L"INVITE TO LOBBY",
				assets::DefaultButtonAssets,
				SmallFont,
				assets::DefaultButtonColors[static_cast<size_t>(FButtonWidget::EButtonVisualState::Idle)]);
			Button1->Create();
			Button1->SetOnPressedCallback([FriendProductUserId]()
			{
				FGameEvent Event(EGameEventType::InviteFriendToLobby, FriendProductUserId);
				FGame::Get().OnGameEvent(Event);
			});
			Button1->SetBackgroundColors(assets::DefaultButtonColors);
		}
#else
			std::wstring PresenceString;
			if (FriendData.Presence.Status == EOS_Presence_EStatus::EOS_PS_Online &&
				(!FriendData.Presence.Application.empty() || !FriendData.Presence.RichText.empty()))
			{
				if (!FriendData.Presence.RichText.empty())
				{
					PresenceString = std::wstring(L"* ") + FriendData.Presence.RichText;
				}
				else if (!FriendData.Presence.Application.empty())
				{
					PresenceString = std::wstring(L"* Playing ") + FriendData.Presence.Application + L" on " + FriendData.Presence.Platform;
				}
			}
			else
			{
				PresenceString = std::wstring(L"* ") + FFriends::FriendPresenceToString(FriendData.Presence.Status);
			}

			FColor PresenceColor = FColor(0.f, 1.f, 0.f, 1.f);
			if (FriendData.Presence.Status == EOS_Presence_EStatus::EOS_PS_Away)
			{
				PresenceColor = FColor(1.f, 1.f, 0.f, 1.f);
			}
			else if (FriendData.Presence.Status == EOS_Presence_EStatus::EOS_PS_DoNotDisturb)
			{
				PresenceColor = FColor(1.f, 0.f, 0.f, 1.f);
			}
			else if (FriendData.Presence.Status == EOS_Presence_EStatus::EOS_PS_Offline)
			{
				PresenceColor = FColor(0.5f, 0.5f, 0.5f, 1.f);
			}

			const char* TestString = "|Test.j";
			Vector2 LabelSize = Vector2(LargeFont->MeasureString(TestString));
			const float LargeTextHeight = ceilf(LabelSize.y);
			LabelSize = SmallFont->MeasureString(TestString);
			const float SmallTextHeight = ceilf(LabelSize.y);

			LabelSize = SmallFont->MeasureString(PresenceString.c_str());
			const float SmallTextWidth = ceilf(LabelSize.x);

			std::vector<std::wstring> EmptyButtonAssets = {};

			std::shared_ptr<FButtonWidget> button = std::make_shared<FButtonWidget>(
				Vector2(Position.x + 3.0f, Position.y + Size.y * 0.6f),
				Vector2(SmallTextWidth + 10.0f, SmallTextHeight),
				Layer - 1,
				PresenceString,
				EmptyButtonAssets,
				SmallFont,
				PresenceColor,
				PresenceColor,
				EAlignmentType::Left);
			button->Create();

			Button1 = button;
#endif
		}
	}
}

void FFriendInfoWidget::Release()
{
	BackgroundImage->Release();

	NameLabel->Release();

	Button1.reset();
	Button2.reset();
	Button3.reset();
}

void FFriendInfoWidget::Update()
{
	if (!bShown)
		return;

	BackgroundImage->Update();

	if (NameLabel)
	{
		NameLabel->Update();
	}

	if (Button1)
	{
#ifdef EOS_SAMPLE_P2P
		//Check if chat is in progress
		auto P2PDialog = static_cast<FMenu&>(*FGame::Get().GetMenu()).GetP2PNATDialog();
		if (P2PDialog)
		{
			FProductUserId CurrentChatAccountId = P2PDialog->GetCurrentChat();
			if (CurrentChatAccountId.IsValid() && CurrentChatAccountId == FriendData.UserProductUserId)
			{
				if (Button1->IsEnabled())
				{
					//Chatting is active. We need to swap chatting button
					Button1->SetText(L"CHATTING");
					Button1->SetTextColor(Color::UIButtonBlue);
					Button1->Disable();
				}
			}
			else
			{
				if (!Button1->IsEnabled())
				{
					//Normal look
					Button1->Enable();
					Button1->SetText(L"CHAT");
					Button1->SetTextColor(Color::White);
				}
			}
		}
#endif // EOS_SAMPLE_P2P

#ifdef EOS_SAMPLE_VOICE
		//Check if chat is in progress
		if (FGame::Get().GetVoice()->IsMember(FriendData.UserProductUserId))
		{
			if (Button1->IsEnabled())
			{
				//Chatting is active. We need to swap chatting button
				Button1->SetText(L"CHATTING");
				Button1->SetTextColor(Color::UIButtonBlue);
				Button1->Disable();
			}
		}
		else
		{
			if (!Button1->IsEnabled())
			{
				//Normal look
				Button1->Enable();
				Button1->SetText(L"CHAT");
				Button1->SetTextColor(Color::White);
			}
		}
#endif // EOS_SAMPLE_VOICE

#ifdef EOS_SAMPLE_LOBBIES
		if (FGame::Get().GetLobbies()->CanInviteToCurrentLobby(FriendData.UserProductUserId))
		{
			if (Button1 && !Button1->IsShown())
			{
				Button1->Show();
			}
		}
		else
		{
			if (Button1 && Button1->IsShown())
			{
				Button1->Hide();
			}
		}
#endif // EOS_SAMPLE_LOBBIES

		Button1->Update();
	}

	if (Button2)
	{
		Button2->Update();
	}

	if (Button3)
	{
		Button3->Update();
	}
}

void FFriendInfoWidget::Render(FSpriteBatchPtr& Batch)
{
	if (!bShown)
		return;

	IWidget::Render(Batch);

	BackgroundImage->Render(Batch);

	if (NameLabel)
	{
		NameLabel->Render(Batch);
	}

	if (Button1)
	{
		Button1->Render(Batch);
	}

	if (Button2)
	{
		Button2->Render(Batch);
	}

	if (Button3)
	{
		Button3->Render(Batch);
	}
}

#ifdef _DEBUG
void FFriendInfoWidget::DebugRender()
{
	IWidget::DebugRender();

	if (BackgroundImage) BackgroundImage->DebugRender();
	if (NameLabel) NameLabel->DebugRender();
	if (Button1) Button1->DebugRender();
	if (Button2) Button2->DebugRender();
	if (Button3) Button3->DebugRender();
}
#endif

void FFriendInfoWidget::OnUIEvent(const FUIEvent& event)
{
	if (!bShown)
		return;

	if (event.GetType() == EUIEventType::MousePressed || event.GetType() == EUIEventType::MouseReleased)
	{
		if (Button1 && Button1->CheckCollision(event.GetVector()))
		{
			Button1->OnUIEvent(event);
		}
		if (Button2 && Button2->CheckCollision(event.GetVector()))
		{
			Button2->OnUIEvent(event);
		}
		if (Button3 && Button3->CheckCollision(event.GetVector()))
		{
			Button3->OnUIEvent(event);
		}
	}
}

void FFriendInfoWidget::SetPosition(Vector2 Pos)
{
	IWidget::SetPosition(Pos);

	if (BackgroundImage) BackgroundImage->SetPosition(Pos);

	if (NameLabel) NameLabel->SetPosition(Vector2(Pos.x, NameLabel->GetPosition().y));

	const float ButtonSize = Size.y / 2.0f;
	const Vector2 RightButtonOffset(Size.x - ButtonSize * 1.5f, Size.y * 0.25f);
	const Vector2 LeftButtonOffset(Size.x - ButtonSize * 3.0f, Size.y * 0.25f);

	if (Button1)
	{
		if (FriendData.Status == EOS_EFriendsStatus::EOS_FS_InviteReceived ||
			FriendData.Status == EOS_EFriendsStatus::EOS_FS_NotFriends)
		{
			Button1->SetPosition(Position + LeftButtonOffset);
		}
		else
		{
			Button1->SetPosition(Vector2(Pos.x, Button1->GetPosition().y));
		}
	}

	if (Button2)
	{
		if (FriendData.Status == EOS_EFriendsStatus::EOS_FS_InviteReceived)
		{
			Button2->SetPosition(Position + RightButtonOffset);
		}
		else
		{
			Button2->SetPosition(Vector2(Pos.x, Button2->GetPosition().y));
		}
	}

	if (Button3)
	{
		Button3->SetPosition(Vector2(Pos.x, Button3->GetPosition().y));
	}
}

void FFriendInfoWidget::SetSize(Vector2 NewSize)
{
	IWidget::SetSize(NewSize);

	if (BackgroundImage) BackgroundImage->SetSize(NewSize);

	if (NameLabel) NameLabel->SetSize(Vector2(NewSize.x, NameLabel->GetSize().y));

	if (Button1 &&
		FriendData.Status != EOS_EFriendsStatus::EOS_FS_InviteReceived &&
		FriendData.Status != EOS_EFriendsStatus::EOS_FS_NotFriends)
	{
		Button1->SetSize(Vector2(NewSize.x, Button1->GetSize().y));
	}

	if (Button2 && FriendData.Status != EOS_EFriendsStatus::EOS_FS_InviteReceived)
	{
		Button2->SetSize(Vector2(NewSize.x, Button2->GetSize().y));
	}

	if (Button3)
	{
		Button3->SetSize(Vector2(NewSize.x, Button3->GetSize().y));
	}
}

void FFriendInfoWidget::SetFriendData(const FFriendData& Data)
{
	bool bNeedReset = false;
	bool bStatusChanged = (Data.Status != FriendData.Status);
	bool bDifferentAccount = (Data.UserId != FriendData.UserId) || (Data.UserProductUserId != FriendData.UserProductUserId) || (Data.Name != FriendData.Name);
	
	bNeedReset = (bStatusChanged || bDifferentAccount);
	
	if (!bNeedReset)
	{
		bool bAccountIdJustSet = !FriendData.UserId.IsValid() && Data.UserId.IsValid();
		bool bAccountProductUserIdJustSet = !FriendData.UserProductUserId.IsValid() && Data.UserProductUserId.IsValid();
		bNeedReset = bAccountIdJustSet || bAccountProductUserIdJustSet;
		if (!bNeedReset)
		{
			bool bPresenceChanged = !(Data.Presence == FriendData.Presence);
			bNeedReset = bPresenceChanged;
		}
	}

	FriendData = Data;

	if (bNeedReset)
	{
		Release();
		Create();
	}
}

void FFriendInfoWidget::SetFocused(bool bFocused)
{
	IWidget::SetFocused(bFocused);

	FColor Col = NameLabel->GetBackgroundColor();
	if (bFocused)
	{
		Col.R += 0.3f;
		if (Col.R > 1.0f)
		{
			Col.R = 1.0f;
		}
	}
	else
	{
		Col.R -= 0.3f;
		if (Col.R < 0.0f)
		{
			Col.R = 0.0f;
		}
	}
	NameLabel->SetBackgroundColor(Col);
}
