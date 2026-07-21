// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "DebugLog.h"
#include "StringUtils.h"
#include "Console.h"
#include "CommandLine.h"
#include "Main.h"
#include "Input.h"
#include "BaseGame.h"
#include "GameEvent.h"
#include "Authentication.h"
#include "Player.h"
#include "Platform.h"
#include "Font.h"
#include "TextLabel.h"
#include "Sprite.h"
#include "ConsoleDialog.h"
#include "FriendsDialog.h"
#include "ExitDialog.h"
#include "AuthDialogs.h"
#include "SampleConstants.h"
#include "BaseMenu.h"
#include "PopupDialog.h"

constexpr char SampleConstants::GameName[];

FBaseMenu::FBaseMenu(std::weak_ptr<FConsole> console) noexcept(false):
	Console(console),
	bShowFPS(false)
{
#ifdef DXTK
	LargeFont = std::make_shared<FFont>(L"Assets/Roboto16.spritefont");
	NormalFont = std::make_shared<FFont>(L"Assets/Roboto12.spritefont");
	SmallFont = std::make_shared<FFont>(L"Assets/Roboto10.spritefont");
	TinyFont = std::make_shared<FFont>(L"Assets/Roboto8.spritefont");

	BoldLargeFont = std::make_shared<FFont>(L"Assets/RobotoBold18.spritefont");
	BoldNormalFont = std::make_shared<FFont>(L"Assets/RobotoBold14.spritefont");
	BoldSmallFont = std::make_shared<FFont>(L"Assets/RobotoBold10.spritefont");
#endif

#ifdef EOS_DEMO_SDL
	LargeFont = std::make_shared<FFont>(L"Assets/Roboto-Regular.ttf", 24);
	NormalFont = std::make_shared<FFont>(L"Assets/Roboto-Regular.ttf", 14);
	SmallFont = std::make_shared<FFont>(L"Assets/Roboto-Regular.ttf", 12);
	TinyFont = std::make_shared<FFont>(L"Assets/Roboto-Regular.ttf", 10);

	BoldLargeFont = std::make_shared<FFont>(L"Assets/Roboto-Bold.ttf", 24);
	BoldNormalFont = std::make_shared<FFont>(L"Assets/Roboto-Bold.ttf", 14);
	BoldSmallFont = std::make_shared<FFont>(L"Assets/Roboto-Bold.ttf", 12);
#endif //EOS_DEMO_SDL

	BackgroundImage = std::make_shared<FSpriteWidget>(Vector2(0.f, 0.f), Vector2(1024.f, 384.f), DefaultLayer, L"Assets/menu_background.dds");

	TitleLabel = std::make_shared<FTextLabelWidget>(
		Vector2(40.f, 40.f),
		Vector2(400.f, 80.f),
		DefaultLayer - 1,
		L"EOS SDK " + FStringUtils::Widen(SampleConstants::GameName),
		L"",
		FColor(1.f, 1.f, 1.f, 1.f),
		FColor(1.f, 1.f, 1.f, 1.f),
		EAlignmentType::Left);

	FPSLabel = std::make_shared<FTextLabelWidget>(
		Vector2(5.f, 5.f),
		Vector2(80.f, 30.f),
		DefaultLayer - 1,
		L"FPS: ",
		L"",
		FColor(1.f, 1.f, 1.f, 1.f),
		FColor(1.f, 1.f, 1.f, 1.f),
		EAlignmentType::Left);
}

void FBaseMenu::Create()
{
	BackgroundImage->Create();

	TitleLabel->Create();
	TitleLabel->SetFont(BoldLargeFont->GetFont());

	FPSLabel->Create();
	FPSLabel->SetFont(SmallFont->GetFont());

	CreateConsoleDialog();
	CreateAuthDialogs();
	CreateExitDialog();
	CreatePopupDialog();
}

void FBaseMenu::Release()
{
	LargeFont->Release();
	NormalFont->Release();
	SmallFont->Release();
	TinyFont->Release();
	BoldLargeFont->Release();
	BoldNormalFont->Release();
	BoldSmallFont->Release();

	BackgroundImage->Release();

	TitleLabel->Release();
	FPSLabel->Release();

	if (ConsoleDialog)
	{
		ConsoleDialog->Release();
		ConsoleDialog.reset();
	}

	if (FriendsDialog)
	{
		FriendsDialog->Release();
		FriendsDialog.reset();
	}

	if (AuthDialogs) AuthDialogs->Release();
}

void FBaseMenu::Update()
{
	BackgroundImage->Update();
	TitleLabel->Update();
	FPSLabel->Update();

	std::unique_ptr<FInput> const& Input = FBaseGame::GetBase().GetInput();

	if (Input)
	{
#ifdef DXTK
		Vector2 MousePos = Input->GetMousePosition();

		if (Input->IsMouseButtonPressed(FInput::InputCommands::UIClicked))
		{
			// Notify Menu
			FUIEvent Event(EUIEventType::MousePressed, MousePos);
			OnUIEvent(Event);
		}

		if (Input->IsMouseButtonReleased(FInput::InputCommands::UIClicked))
		{
			// Notify Menu
			FUIEvent Event(EUIEventType::MouseReleased, MousePos);
			OnUIEvent(Event);
		}

		if (int MouseScrollDiff = Input->GetMouseScrollWheelLines())
		{
			FUIEvent Event(EUIEventType::MouseWheelScrolled, Vector2(0.0f, float(MouseScrollDiff)));
			OnUIEvent(Event);
		}
#endif // DXTK

		// check if characters were typed
		if (Input->IsAnyKeyPressed())
		{
			//Check if we need to generate repeated key press events
			if (KeyCurrentlyHeld != FInput::None)
			{
				if (!Input->IsKeyReleased(static_cast<FInput::Keys>(toupper(int(KeyCurrentlyHeld)))))
				{
					KeyCurrentlyHeldSeconds += static_cast<float>(Main->GetTimer().GetElapsedSeconds());

					if (KeyCurrentlyHeldSeconds >= SecondsTilKeyRepeat ||
						(KeyCurrentlyHeld == FInput::Back && KeyCurrentlyHeldSeconds >= (SecondsTilKeyRepeat / 2.0f)) ||
						(KeyCurrentlyHeld == FInput::Delete && KeyCurrentlyHeldSeconds >= (SecondsTilKeyRepeat / 2.0f)))
					{
						KeyCurrentlyHeldSeconds = 0.0f;
						FUIEvent event(EUIEventType::KeyPressed, KeyCurrentlyHeld);
						OnUIEvent(event);
					}
				}
				else
				{
					KeyCurrentlyHeld = FInput::None;
					KeyCurrentlyHeldSeconds = 0.0f;
				}
			}

#ifdef DXTK
			const auto& keysDown = Input->GetKeysDown();
			for (FInput::Keys Key : keysDown)
			{
				FUIEvent event(EUIEventType::KeyPressed, Key);
				OnUIEvent(event);

				KeyCurrentlyHeld = Key;
				KeyCurrentlyHeldSeconds = 0.0f;
			}
#endif // DXTK
		}
	}

	for (DialogPtr NextDialog : Dialogs)
	{
		NextDialog->Update();
	}

	AuthDialogs->Update();

	UpdateFPS();
}

void FBaseMenu::Render(FSpriteBatchPtr& Batch)
{
	if (BackgroundImage)
	{
		BackgroundImage->Render(Batch);
	}

	if (TitleLabel)
	{
		TitleLabel->Render(Batch);
	}

	if (FPSLabel && bShowFPS)
	{
		FPSLabel->Render(Batch);
	}
	
	for (DialogPtr NextDialog : Dialogs)
	{
		NextDialog->Render(Batch);
	}
	
	AuthDialogs->Render(Batch);
}

#ifdef _DEBUG
void FBaseMenu::DebugRender()
{
	if (BackgroundImage)
	{
		BackgroundImage->DebugRender();
	}

	if (TitleLabel)
	{
		TitleLabel->DebugRender();
	}

	if (FPSLabel)
	{
		FPSLabel->DebugRender();
	}

	for (DialogPtr NextDialog : Dialogs)
	{
		NextDialog->DebugRender();
	}

	AuthDialogs->DebugRender();
}
#endif

void FBaseMenu::Init()
{
	AuthDialogs->Init();
}

void FBaseMenu::UpdateLayout(int Width, int Height)
{
	Vector2 WindowSize = Vector2((float)Width, (float)Height);

	BackgroundImage->SetPosition(Vector2(0.f, 0.f));
	BackgroundImage->SetSize(Vector2((float)Width, ((float)Height) / 2.f));

	if (ConsoleDialog)
	{
		Vector2 ConsoleWidgetSize = Vector2(WindowSize.x * 0.7f, WindowSize.y * 0.75f);
		ConsoleDialog->SetSize(ConsoleWidgetSize);

		Vector2 ConsoleWidgetPos = Vector2(10.f, WindowSize.y - ConsoleWidgetSize.y - 10.f);
		ConsoleDialog->SetPosition(ConsoleWidgetPos);

		if (FriendsDialog)
		{
			Vector2 FriendsDialogSize = Vector2(WindowSize.x - ConsoleDialog->GetSize().x - 30.f,
				ConsoleDialog->GetSize().y);
			FriendsDialog->SetSize(FriendsDialogSize);

			Vector2 FriendDialogPos = Vector2(ConsoleDialog->GetPosition().x + ConsoleDialog->GetSize().x + 10.f,
				ConsoleDialog->GetPosition().y);
			FriendsDialog->SetPosition(FriendDialogPos);
		}
	}

	if (PopupDialog)
	{
		PopupDialog->SetPosition(Vector2((WindowSize.x / 2.f) - PopupDialog->GetSize().x / 2.0f, (WindowSize.y / 2.f) - PopupDialog->GetSize().y));
	}

	if (ExitDialog)
	{
		ExitDialog->SetPosition(Vector2((WindowSize.x / 2.f) - ExitDialog->GetSize().x / 2.0f, (WindowSize.y / 2.f) - ExitDialog->GetSize().y));
	}

	if (AuthDialogs) AuthDialogs->UpdateLayout();
}


void FBaseMenu::CreateFonts()
{
	LargeFont->Create();
	NormalFont->Create();
	SmallFont->Create();
	TinyFont->Create();
	BoldLargeFont->Create();
	BoldNormalFont->Create();
	BoldSmallFont->Create();
}

void FBaseMenu::CreateConsoleDialog()
{
	float PosX = 20.f;
	float PosY = 170.f;
	float SizeX = 1040.f;
	float SizeY = 540.f;

	ConsoleDialog = std::make_shared<FConsoleDialog>(
		Vector2(PosX, PosY),
		Vector2(SizeX, SizeY),
		DefaultLayer - 2,
		Console,
		SmallFont->GetFont(),
		NormalFont->GetFont(),
		LargeFont->GetFont(),
		BoldSmallFont->GetFont(),
		BoldSmallFont->GetFont());

	ConsoleDialog->SetBorderColor(Color::UIBorderGrey);
	ConsoleDialog->Create();
	
	AddDialog(ConsoleDialog);
}

void FBaseMenu::CreateFriendsDialog()
{
	const float FX = 100.0f;
	const float FY = 100.0f;
	const float FriendsWidth = 300.0f;
	const float FriendsHeight = 300.0f;

	FriendsDialog = std::make_shared<FFriendsDialog>(
		Vector2(FX, FY),
		Vector2(FriendsWidth, FriendsHeight),
		DefaultLayer - 2,
		NormalFont->GetFont(),
		BoldSmallFont->GetFont(),
		TinyFont->GetFont());

	FriendsDialog->SetBorderColor(Color::UIBorderGrey);
	FriendsDialog->Create();
	
	AddDialog(FriendsDialog);
}

void FBaseMenu::CreateAuthDialogs()
{
	AuthDialogs = std::make_shared<FAuthDialogs>(
		FriendsDialog,
		L"Friends",
		BoldSmallFont->GetFont(),
		SmallFont->GetFont(),
		TinyFont->GetFont());
	
	AuthDialogs->Create();
}

void FBaseMenu::CreateExitDialog()
{
	ExitDialog = std::make_shared<FExitDialog>(
		Vector2(200.f, 200.f),
		Vector2(330.f, 160.f),
		5,
		NormalFont->GetFont(),
		SmallFont->GetFont());

	ExitDialog->SetBorderColor(Color::UIBorderGrey);
	ExitDialog->Create();
	
	AddDialog(ExitDialog);
	
	HideDialog(ExitDialog);
}

void FBaseMenu::CreatePopupDialog()
{
	PopupDialog = std::make_shared<FPopupDialog>(
		Vector2(200.f, 200.f),
		Vector2(330.f, 160.f),
		10,
		L"",
		NormalFont->GetFont(),
		SmallFont->GetFont());

	PopupDialog->SetBorderColor(Color::UIBorderGrey);
	PopupDialog->Create();

	AddDialog(PopupDialog);

	HideDialog(PopupDialog);
}

void FBaseMenu::ShowDialog(DialogPtr Dialog)
{
	ShowDialogTime = static_cast<float>(Main->GetTimer().GetTotalSeconds());

	if (Dialog)
	{
		Dialog->Show();
	}
}

void FBaseMenu::HideDialog(DialogPtr Dialog)
{
	if (Dialog)
	{
		Dialog->Hide();
	}
}

bool FBaseMenu::IsDialogReadyForInput()
{
	float CurTime = static_cast<float>(Main->GetTimer().GetTotalSeconds());
	if (CurTime - ShowDialogTime > 0.5f)
	{
		return true;
	}
	return false;
}

void FBaseMenu::OnUIEvent(const FUIEvent& Event)
{
#ifdef EOS_DEMO_SDL
	if (Event.GetType() == EUIEventType::FullscreenToggle)
	{
		// Toggle fullscreen
		if (Main->bIsFullScreen)
		{
			SDL_SetWindowFullscreen(Main->GetWindow(), 0);
			Main->bIsFullScreen = false;
		}
		else
		{
			SDL_SetWindowFullscreen(Main->GetWindow(), SDL_WINDOW_FULLSCREEN_DESKTOP);
			Main->bIsFullScreen = true;
		}
	}
	else
#endif
	if (Event.GetType() == EUIEventType::MousePressed)
	{
		for (DialogPtr NextDialog : Dialogs)
		{
			if (NextDialog->CheckCollision(Event.GetVector()))
			{
				NextDialog->OnUIEvent(Event);
			}
			else
			{
				NextDialog->SetFocused(false);
			}
		}
	}
	else
	{
		for (DialogPtr NextDialog : Dialogs)
		{
			NextDialog->OnUIEvent(Event);
		}
	}

	if (AuthDialogs) AuthDialogs->OnUIEvent(Event);
}

void FBaseMenu::AddDialog(DialogPtr Dialog)
{
	Dialogs.push_back(Dialog);
}

void FBaseMenu::OnGameEvent(const FGameEvent& Event)
{
	if (Event.GetType() == EGameEventType::ShowPrevUser)
	{
		UpdateFriends();
	}
	else if (Event.GetType() == EGameEventType::ShowNextUser)
	{
		UpdateFriends();
	}
	else if (Event.GetType() == EGameEventType::CancelLogin)
	{
		UpdateFriends();
	}
	else if (Event.GetType() == EGameEventType::ToggleFPS)
	{
		ToggleFPS();
	}
	else if (Event.GetType() == EGameEventType::ExitGame)
	{
		if (PopupDialog->IsShown())
		{
			HideDialog(PopupDialog);
		}
		else
		{
			if (ExitDialog->IsShown())
			{
				HideDialog(ExitDialog);
			}
			else
			{
				ShowDialog(ExitDialog);
			}
		}
	}
	else if (Event.GetType() == EGameEventType::ShowPopupDialog)
	{
		PopupDialog->SetText(Event.GetFirstStr());
		ShowDialog(PopupDialog);
	}

	if (FriendsDialog) FriendsDialog->OnGameEvent(Event);

	if (AuthDialogs) AuthDialogs->OnGameEvent(Event);
}

void FBaseMenu::UpdateFriends()
{
	if (FriendsDialog)
	{
		FriendsDialog->SetPosition(Vector2(ConsoleDialog->GetPosition().x + ConsoleDialog->GetSize().x + 10.f,
										   FriendsDialog->GetPosition().y));
	}
}

void FBaseMenu::SetTestUserInfo()
{
	if (AuthDialogs) AuthDialogs->SetTestUserInfo();
}

void FBaseMenu::UpdateFPS()
{
	if (FPSLabel)
	{
		uint32_t FPS = Main->GetTimer().GetFramesPerSecond();
		static wchar_t FPSStr[20] = {};
		wsprintf(FPSStr, L"FPS: %d", FPS);

		FPSLabel->SetText(FPSStr);
	}
}

void FBaseMenu::ToggleFPS()
{
	bShowFPS = !bShowFPS;
}
