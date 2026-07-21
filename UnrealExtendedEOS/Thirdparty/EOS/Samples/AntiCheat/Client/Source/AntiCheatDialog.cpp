// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"

#include "Game.h"
#include "Authentication.h"
#include "AntiCheatDialog.h"
#include "AntiCheatClient.h"
#include "CommandLine.h"
#include "DebugLog.h"
#include "GameEvent.h"
#include "Player.h"
#include "Sprite.h"

constexpr wchar_t AntiCheatModeParam[] = L"anticheatmode";
constexpr wchar_t AntiCheatPortParam[] = L"anticheatport";
constexpr wchar_t AntiCheatListenPortParam[] = L"anticheatlistenport";
constexpr wchar_t AntiCheatIPParam[] = L"anticheatip";
constexpr wchar_t AntiCheatAutoStartSessionParam[] = L"anticheatautostartsession";
constexpr float HeaderLabelHeight = 30.0f;

FAntiCheatDialog::FAntiCheatDialog(
	Vector2 InPosition,
	Vector2 InSize,
	UILayer InLayer,
	FontPtr DialogNormalFont,
	FontPtr DialogSmallFont)
	: FDialog(InPosition, InSize, InLayer)
{
	HeaderLabel = std::make_shared<FTextLabelWidget>(
		Position,
		Vector2(Size.x, HeaderLabelHeight),
		Layer - 1,
		L"ANTICHEAT",
		L"Assets/dialog_title.dds",
		FColor(1.f, 1.f, 1.f, 1.f),
		FColor(1.f, 1.f, 1.f, 1.f),
		EAlignmentType::Left
		);
	HeaderLabel->SetFont(DialogSmallFont);
	HeaderLabel->SetBorderColor(Color::UIBorderGrey);

	BackgroundImage = std::make_shared<FSpriteWidget>(
		Position,
		Size,
		Layer,
		L"Assets/texteditor.dds");

	ModeLabel = std::make_shared<FTextLabelWidget>(
		Vector2(50.f, 50.f),
		Vector2(100.f, 30.f),
		Layer - 1,
		L"Mode:",
		L"");
	ModeLabel->SetFont(DialogNormalFont);

	ClientServerModeButton = std::make_shared<FButtonWidget>(
		Vector2(0.f, 0.f),
		Vector2(0, 0.f),
		Layer - 1,
		L"ClientServer",
		assets::DefaultButtonAssets,
		DialogNormalFont,
		Color::UIButtonBlue
	);
	ClientServerModeButton->SetBackgroundColors(assets::DefaultButtonColors);
	ClientServerModeButton->SetOnPressedCallback([this]()
	{
		OnClientServerModeButtonPressed();
	});

	P2PModeButton = std::make_shared<FButtonWidget>(
		Vector2(0.f, 0.f),
		Vector2(0, 0.f),
		Layer - 1,
		L"P2P",
		assets::DefaultButtonAssets,
		DialogNormalFont,
		Color::UIButtonBlue
	);
	P2PModeButton->SetBackgroundColors(assets::DefaultButtonColors);
	P2PModeButton->SetOnPressedCallback([this]()
	{
		OnP2PModeButtonPressed();
	});

	ListenPortLabel = std::make_shared<FTextLabelWidget>(
		Vector2(50.f, 50.f),
		Vector2(100.f, 30.f),
		Layer - 1,
		L"Listen Port:",
		L"");
	ListenPortLabel->SetFont(DialogNormalFont);

	ListenPortField = std::make_shared<FTextFieldWidget>(
		Vector2(50.f, 50.f),
		Vector2(150.f, 30.f),
		Layer - 1,
		L"1234",
		L"Assets/textfield.dds",
		DialogNormalFont,
		FTextFieldWidget::EInputType::Normal,
		EAlignmentType::Left);
	ListenPortField->SetBorderColor(Color::UIBorderGrey);

	IPLabel = std::make_shared<FTextLabelWidget>(
		Vector2(50.f, 50.f),
		Vector2(100.f, 30.f),
		Layer - 1,
		L"IP:",
		L"");
	IPLabel->SetFont(DialogNormalFont);

	PortLabel = std::make_shared<FTextLabelWidget>(
		Vector2(50.f, 50.f),
		Vector2(100.f, 30.f),
		Layer - 1,
		L"Port:",
		L"");
	PortLabel->SetFont(DialogNormalFont);

	JoinGameButton = std::make_shared<FButtonWidget>(
		Vector2(0.f, 0.f),
		Vector2(0, 0.f),
		Layer - 1,
		L"JOIN GAME SERVER",
		assets::DefaultButtonAssets,
		DialogNormalFont,
		Color::UIButtonBlue
		);
	JoinGameButton->SetBackgroundColors(assets::DefaultButtonColors);
	JoinGameButton->SetOnPressedCallback([this]()
	{
		OnJoinGameButtonPressed();
	});

	LeaveGameButton = std::make_shared<FButtonWidget>(
		Vector2(0.f, 0.f),
		Vector2(0, 0.f),
		Layer - 1,
		L"LEAVE GAME SERVER",
		assets::DefaultButtonAssets,
		DialogNormalFont,
		Color::UIButtonBlue
		);
	LeaveGameButton->SetBackgroundColors(assets::DefaultButtonColors);
	LeaveGameButton->SetOnPressedCallback([this]()
	{
		OnLeaveGameButtonPressed();
	});
	LeaveGameButton->Disable();

	RegisterPeerButton = std::make_shared<FButtonWidget>(
		Vector2(0.f, 0.f),
		Vector2(0, 0.f),
		Layer - 1,
		L"REGISTER PEER",
		assets::DefaultButtonAssets,
		DialogNormalFont,
		Color::UIButtonBlue
	);
	RegisterPeerButton->SetBackgroundColors(assets::DefaultButtonColors);
	RegisterPeerButton->SetOnPressedCallback([this]()
	{
		OnRegisterPeerButtonPressed();
	});
	RegisterPeerButton->Disable();

	IPField = std::make_shared<FTextFieldWidget>(
		Vector2(50.f, 50.f),
		Vector2(150.f, 30.f),
		Layer - 1,
		L"127.0.0.1",
		L"Assets/textfield.dds",
		DialogNormalFont,
		FTextFieldWidget::EInputType::Normal,
		EAlignmentType::Left);
	IPField->SetBorderColor(Color::UIBorderGrey);

	PortField = std::make_shared<FTextFieldWidget>(
		Vector2(50.f, 50.f),
		Vector2(150.f, 30.f),
		Layer - 1,
		L"1234",
		L"Assets/textfield.dds",
		DialogNormalFont,
		FTextFieldWidget::EInputType::Normal,
		EAlignmentType::Left);
	PortField->SetBorderColor(Color::UIBorderGrey);

	HideUI();
}

void FAntiCheatDialog::SetWindowProportion(Vector2 InWindowProportion)
{
	ConsoleWindowProportion = InWindowProportion;
}

void FAntiCheatDialog::SetWindowSize(Vector2 WindowSize)
{
	const Vector2 DefListSize = Vector2(WindowSize.x * (1.0f - ConsoleWindowProportion.x) - 40.0f, WindowSize.y - 200.0f);
	const Vector2 DialogSize = Vector2(WindowSize.x * (1.0f - ConsoleWindowProportion.x) - 30.0f, WindowSize.y - 90.0f);

	if (HeaderLabel) HeaderLabel->SetSize(Vector2(DialogSize.x, HeaderLabelHeight));
	if (BackgroundImage) BackgroundImage->SetSize(DialogSize);
}

void FAntiCheatDialog::Create()
{
	if (HeaderLabel) HeaderLabel->Create();
	if (BackgroundImage) BackgroundImage->Create();
	if (ClientServerModeButton) ClientServerModeButton->Create();
	if (P2PModeButton) P2PModeButton->Create();
	if (JoinGameButton) JoinGameButton->Create();
	if (LeaveGameButton) LeaveGameButton->Create();
	if (RegisterPeerButton) RegisterPeerButton->Create();
	if (ModeLabel) ModeLabel->Create();
	if (IPLabel) IPLabel->Create();
	if (IPField) IPField->Create();
	if (ListenPortLabel) ListenPortLabel->Create();
	if (ListenPortField) ListenPortField->Create();
	if (PortLabel) PortLabel->Create();
	if (PortField) PortField->Create();

	AddWidget(HeaderLabel);
	AddWidget(BackgroundImage);
	AddWidget(ClientServerModeButton);
	AddWidget(P2PModeButton);
	AddWidget(JoinGameButton);
	AddWidget(LeaveGameButton);
	AddWidget(RegisterPeerButton);
	AddWidget(ModeLabel);
	AddWidget(IPLabel);
	AddWidget(IPField);
	AddWidget(ListenPortLabel);
	AddWidget(ListenPortField);
	AddWidget(PortLabel);
	AddWidget(PortField);
}

void FAntiCheatDialog::SetPosition(Vector2 Pos)
{
	IWidget::SetPosition(Pos);

	if (HeaderLabel) HeaderLabel->SetPosition(Pos);
	if (BackgroundImage) BackgroundImage->SetPosition(Vector2(Position.x, Position.y));
	if (ModeLabel) ModeLabel->SetPosition(Pos + Vector2(0.f, 40.f));
	if (ClientServerModeButton) ClientServerModeButton->SetPosition(ModeLabel->GetPosition() + Vector2(90.f, 0.f));
	if (P2PModeButton) P2PModeButton->SetPosition(ModeLabel->GetPosition() + Vector2(190.f, 0.f));
	if (ListenPortLabel) ListenPortLabel->SetPosition(Pos + Vector2(0.f, 80.f));
	if (ListenPortField) ListenPortField->SetPosition(Pos + Vector2(90.f, 80.f));
	if (IPLabel) IPLabel->SetPosition(Pos + Vector2(0.f, 120.f));
	if (IPField) IPField->SetPosition(Pos + Vector2(90.f, 120.f));
	if (PortLabel) PortLabel->SetPosition(Pos + Vector2(0.f, 160.f));
	if (PortField) PortField->SetPosition(Pos + Vector2(90.f, 160.f));
	if (JoinGameButton) JoinGameButton->SetPosition(PortLabel->GetPosition() + Vector2(35.f, 60.f));
	if (LeaveGameButton) LeaveGameButton->SetPosition(JoinGameButton->GetPosition() + Vector2(0.f, 40.f));
	if (RegisterPeerButton) RegisterPeerButton->SetPosition(LeaveGameButton->GetPosition() + Vector2(0.f, 40.f));
}

void FAntiCheatDialog::SetSize(Vector2 NewSize)
{
	IWidget::SetSize(NewSize);

	if (HeaderLabel) HeaderLabel->SetSize(Vector2(NewSize.x, HeaderLabelHeight));
	if (BackgroundImage) BackgroundImage->SetSize(Vector2(NewSize.x, NewSize.y));
	if (ClientServerModeButton) ClientServerModeButton->SetSize(Vector2(90.f, 30.f));
	if (P2PModeButton) P2PModeButton->SetSize(Vector2(40.f, 30.f));
	if (JoinGameButton) JoinGameButton->SetSize(Vector2(220.f, 30.f));
	if (LeaveGameButton) LeaveGameButton->SetSize(Vector2(220.f, 30.f));
	if (RegisterPeerButton) RegisterPeerButton->SetSize(Vector2(220.f, 30.f));
	if (ModeLabel) ModeLabel->SetSize(Vector2(100.f, 30.f));
	if (IPLabel) IPLabel->SetSize(Vector2(100.f, 30.f));
	if (IPField) IPField->SetSize(Vector2(160.f, 30.f));
	if (ListenPortLabel) ListenPortLabel->SetSize(Vector2(100.f, 30.f));
	if (ListenPortField) ListenPortField->SetSize(Vector2(160.f, 30.f));
	if (PortLabel) PortLabel->SetSize(Vector2(100.f, 30.f));
	if (PortField) PortField->SetSize(Vector2(160.f, 30.f));
}

void FAntiCheatDialog::ShowUI()
{
	if (ModeLabel)
	{
		ModeLabel->Enable();
		ModeLabel->Show();
	}

	if (IPLabel)
	{
		IPLabel->Enable();
		IPLabel->Show();
	}

	if (IPField)
	{
		IPField->Enable();
		IPField->Show();
	}

	if (ListenPortLabel)
	{
		ListenPortLabel->Enable();
	}

	if (ListenPortField)
	{
		ListenPortField->Enable();
	}

	if (PortLabel)
	{
		PortLabel->Enable();
		PortLabel->Show();
	}

	if (PortField)
	{
		PortField->Enable();
		PortField->Show();
	}

	if (ClientServerModeButton)
	{
		ClientServerModeButton->Show();
	}

	if (P2PModeButton)
	{
		P2PModeButton->Show();
	}

	if (JoinGameButton)
	{
		JoinGameButton->Show();
	}

	if (LeaveGameButton)
	{
		LeaveGameButton->Show();
	}

	if (!FGame::Get().GetAntiCheatClient()->IsInitialized())
	{
		ClientServerModeButton->Disable();
		P2PModeButton->Disable();
		JoinGameButton->Disable();
		LeaveGameButton->Disable();
	}
}

void FAntiCheatDialog::HideUI()
{
	if (ModeLabel)
	{
		ModeLabel->Hide();
	}

	if (IPLabel)
	{
		IPLabel->Hide();
	}

	if (IPField)
	{
		IPField->Disable();
		IPField->Hide();
	}

	if (ListenPortLabel)
	{
		ListenPortLabel->Hide();
	}

	if (ListenPortField)
	{
		ListenPortField->Disable();
		ListenPortField->Hide();
	}

	if (PortLabel)
	{
		PortLabel->Hide();
	}

	if (PortField)
	{
		PortField->Disable();
		PortField->Hide();
	}

	if (ClientServerModeButton)
	{
		ClientServerModeButton->Hide();
	}

	if (P2PModeButton)
	{
		P2PModeButton->Hide();
	}

	if (JoinGameButton)
	{
		JoinGameButton->Hide();
	}

	if (LeaveGameButton)
	{
		LeaveGameButton->Hide();
	}

	if (RegisterPeerButton)
	{
		RegisterPeerButton->Hide();
	}

	SetFocused(false);
}

void FAntiCheatDialog::OnGameEvent(const FGameEvent& Event)
{
	if (Event.GetType() == EGameEventType::UserLoggedIn)
	{
		ShowUI();

		// Set up any values we've been passed on the command line.
		const std::wstring ModeParam = FCommandLine::Get().GetParamValue(AntiCheatModeParam);
		if (!ModeParam.empty())
		{
			if (ModeParam == L"p2p")
			{
				OnP2PModeButtonPressed();
			}
			else if (ModeParam == L"clientserver")
			{
				OnClientServerModeButtonPressed();
			}
			else
			{
				FDebugLog::LogWarning(L"AntiCheatDialog - Unrecognized value for %s", AntiCheatModeParam);
			}
		}

		const std::wstring IPParam = FCommandLine::Get().GetParamValue(AntiCheatIPParam);
		if (!IPParam.empty())
		{
			IPField->SetText(IPParam);
		}

		const std::wstring PortParam = FCommandLine::Get().GetParamValue(AntiCheatPortParam);
		if (!PortParam.empty())
		{
			PortField->SetText(PortParam);
		}

		const std::wstring ListenPortParam = FCommandLine::Get().GetParamValue(AntiCheatListenPortParam);
		if (!ListenPortParam.empty())
		{
			ListenPortField->SetText(ListenPortParam);
		}
	}
	else if (Event.GetType() == EGameEventType::UserConnectLoggedIn)
	{
		// Autostart the anti-cheat client session if requested on the command line (useful for CI, etc).
		if (FCommandLine::Get().HasFlagParam(AntiCheatAutoStartSessionParam))
		{
			FDebugLog::Log(L"AntiCheatDialog - AntiCheatAutoStartSession requested on command line. Starting...");
			OnJoinGameButtonPressed();
		}
	}
	else if (Event.GetType() == EGameEventType::UserLoginRequiresMFA)
	{
		SetFocused(false);
	}
	else if (Event.GetType() == EGameEventType::UserLoginEnteredMFA)
	{
	}
	else if (Event.GetType() == EGameEventType::UserLoggedOut)
	{
		LeaveGame();
		HideUI();
	}
	else if (Event.GetType() == EGameEventType::ShowPrevUser)
	{
	}
	else if (Event.GetType() == EGameEventType::ShowNextUser)
	{
	}
	else if (Event.GetType() == EGameEventType::NewUserLogin)
	{
		HideUI();
	}
	else if (Event.GetType() == EGameEventType::CancelLogin)
	{
		ShowUI();
	}
	else if (Event.GetType() == EGameEventType::AntiCheatKicked)
	{
		LeaveGame();
	}
}

void FAntiCheatDialog::OnClientServerModeButtonPressed()
{
	ListenPortLabel->Hide();
	ListenPortField->Hide();
	JoinGameButton->SetText(L"JOIN GAME SERVER");
	LeaveGameButton->SetText(L"LEAVE GAME SERVER");
	RegisterPeerButton->Disable();
	RegisterPeerButton->Hide();
	bIsP2P = false;
}

void FAntiCheatDialog::OnP2PModeButtonPressed()
{
	ListenPortLabel->Show();
	ListenPortField->Show();
	RegisterPeerButton->Show();
	JoinGameButton->SetText(L"START P2P SESSION");
	LeaveGameButton->SetText(L"END P2P SESSION");
	bIsP2P = true;
}

void FAntiCheatDialog::OnJoinGameButtonPressed()
{
	const PlayerPtr Player = FPlayerManager::Get().GetPlayer(FPlayerManager::Get().GetCurrentUser());
	if (!Player)
	{
		FDebugLog::LogError(L"AntiCheatDialog - OnJoinGameButtonPressed: Current player is invalid!");
		return;
	}

	// Get a Connect ID Token which will be sent to the server as part of the registration message.
	EOS_ProductUserId ProductUserId = Player->GetProductUserID();
	std::string ConnectIdToken = FGame::Get().GetAuthentication()->GetConnectIdToken(ProductUserId);
	if (ConnectIdToken.empty())
	{
		FDebugLog::LogError(L"AntiCheatDialog - OnJoinGameButtonPressed: Failed to get Connect ID Token!");
		return;
	}

	if (bIsP2P)
	{
		if (FGame::Get().GetAntiCheatClient()->StartP2P(std::stoi(ListenPortField->GetText()), ProductUserId, ConnectIdToken.c_str()))
		{
			ClientServerModeButton->Disable();
			ClientServerModeButton->SetFocused(false);

			P2PModeButton->Disable();
			P2PModeButton->SetFocused(false);

			JoinGameButton->Disable();
			JoinGameButton->SetFocused(false);

			LeaveGameButton->Enable();

			RegisterPeerButton->Enable();
			RegisterPeerButton->SetFocused(true);

		}
	}
	else
	{
		const std::string IP = FStringUtils::Narrow(IPField->GetText());
		const int Port = std::stoi(PortField->GetText());

		const bool bDidSessionBegin = FGame::Get().GetAntiCheatClient()->Start(IP, Port, Player->GetProductUserID(), ConnectIdToken.c_str());
		if (bDidSessionBegin)
		{
			ClientServerModeButton->Disable();
			ClientServerModeButton->SetFocused(false);

			P2PModeButton->Disable();
			P2PModeButton->SetFocused(false);

			JoinGameButton->Disable();
			JoinGameButton->SetFocused(false);

			LeaveGameButton->Enable();
			LeaveGameButton->SetFocused(true);
		}
	}
}

void FAntiCheatDialog::OnLeaveGameButtonPressed()
{
	LeaveGame();	
}

void FAntiCheatDialog::OnRegisterPeerButtonPressed()
{
	const std::string IP = FStringUtils::Narrow(IPField->GetText());
	const int Port = std::stoi(PortField->GetText());
	FGame::Get().GetAntiCheatClient()->ConnectP2PPeer(IP, Port);
}

void FAntiCheatDialog::LeaveGame()
{
	if (bIsP2P)
	{
		FGame::Get().GetAntiCheatClient()->StopP2P();
	}
	else
	{
		FGame::Get().GetAntiCheatClient()->Stop();
	}

	ClientServerModeButton->Enable();
	P2PModeButton->Enable();

	LeaveGameButton->Disable();
	LeaveGameButton->SetFocused(false);

	JoinGameButton->Enable();
	JoinGameButton->SetFocused(true);

	RegisterPeerButton->Disable();
	RegisterPeerButton->SetFocused(false);
}
