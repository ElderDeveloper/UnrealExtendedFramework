// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "DebugLog.h"
#include "CommandLine.h"
#include "Main.h"
#include "Game.h"
#include "Authentication.h"
#include "Platform.h"
#include "AccountHelpers.h"
#include "Player.h"
#include "GameEvent.h"
#include "Sprite.h"
#include "TextLabel.h"
#include "TextField.h"
#include "Button.h"
#include "Settings.h"
#include "AuthDialogs.h"

#ifdef EOS_SAMPLE_SESSIONS
#include "SessionMatchmaking.h"

const double MaxTimeToLogout = 7.0; //7 seconds
#endif

constexpr FColor FAuthDialogs::AuthButtonBackDisabledCol;
constexpr FColor FAuthDialogs::AuthButtonBackCol;

FAuthDialogs::FAuthDialogs(DialogPtr Parent,
						   std::wstring DialogLoginText,
						   FontPtr DialogBoldSmallFont,
						   FontPtr DialogSmallFont,
						   FontPtr DialogTinyFont) :
	ParentDialog(Parent),
	LoginText(DialogLoginText),
	BoldSmallFont(DialogBoldSmallFont),
	SmallFont(DialogSmallFont),
	TinyFont(DialogTinyFont),
	SavedLoginMode(ELoginMode::AccountPortal),
	NewLoginMode(SavedLoginMode)
{
	LoginMethods = 
	{
#ifdef DEV_BUILD
		FLoginMethodButtonData(L"Password", ELoginMode::IDPassword),
		FLoginMethodButtonData(L"Exchange", ELoginMode::ExchangeCode),
		FLoginMethodButtonData(L"Dev Auth", ELoginMode::DevAuth),
		FLoginMethodButtonData(L"Account Portal", ELoginMode::AccountPortal)
#else
		FLoginMethodButtonData(L"Dev Auth", ELoginMode::DevAuth),
		FLoginMethodButtonData(L"Account Portal", ELoginMode::AccountPortal)
#endif
	};
}

void FAuthDialogs::Update()
{
	if (bLoggingOut)
	{
#ifdef EOS_SAMPLE_SESSIONS
		if (!FGame::Get().GetSessions()->HasActiveLocalSessions() || (Main->GetTimer().GetTotalSeconds() - LogoutTriggeredTimestamp) > MaxTimeToLogout)
		{
			FGameEvent Event(EGameEventType::StartUserLogout, FPlayerManager::Get().GetCurrentUser());
			FGame::Get().OnGameEvent(Event);

			bLoggingOut = false;
		}
#else
		FGameEvent Event(EGameEventType::StartUserLogout, FPlayerManager::Get().GetCurrentUser());
		FGame::Get().OnGameEvent(Event);

		bLoggingOut = false;
#endif
	}

	if (NewLoginMode != SavedLoginMode)
	{
		DoChangeLoginMode(NewLoginMode);
	}

	for (DialogPtr NextGlobalDialog : Dialogs)
	{
		NextGlobalDialog->Update();
	}

	UpdateUserButtons();
}

void FAuthDialogs::UpdateUserButtons()
{
	if (!bIsSingleUserOnly && UserLoggedInDialog)
	{
		int NumPlayers = (int)FPlayerManager::Get().GetNumPlayers();
		int CurrentPlayerIndex = 0;
		if (FPlayerManager::Get().GetCurrentUser().IsValid())
		{
			CurrentPlayerIndex = FPlayerManager::Get().GetPlayerIndex(FPlayerManager::Get().GetCurrentUser());
		}

		WidgetPtr NewUserButton = UserLoggedInDialog->GetWidget(2);
		if (NewUserButton)
		{
			if (NumPlayers > 0)
			{
				NewUserButton->Enable();
			}
			else
			{
				NewUserButton->Disable();
			}
		}

		WidgetPtr PrevUserButton = UserLoggedInDialog->GetWidget(3);
		if (PrevUserButton)
		{
			if (NumPlayers > 1 && CurrentPlayerIndex > 1)
			{
				PrevUserButton->Enable();
			}
			else
			{
				PrevUserButton->Disable();
			}
		}

		WidgetPtr NextUserButton = UserLoggedInDialog->GetWidget(4);
		if (NextUserButton)
		{
			if (NumPlayers > 1 && (CurrentPlayerIndex > 0 && CurrentPlayerIndex < NumPlayers))
			{
				NextUserButton->Enable();
			}
			else
			{
				NextUserButton->Disable();
			}
		}
	}
}

void FAuthDialogs::Render(FSpriteBatchPtr& Batch)
{
	for (DialogPtr NextGlobalDialog : Dialogs)
	{
		NextGlobalDialog->Render(Batch);
	}
}

#ifdef _DEBUG
void FAuthDialogs::DebugRender()
{
	for (DialogPtr NextGlobalDialog : Dialogs)
	{
		NextGlobalDialog->DebugRender();
	}
}
#endif

void FAuthDialogs::Create()
{
	if (!ParentDialog)
	{
		return;
	}

	CreateLoginIDPasswordDialog();
	CreateLoginExchangeCodeDialog();
	CreateLoginDevAuthDialog();
	CreateLoginAccountPortalDialog();

	CreateMFALoginDialog();

	CreateUserLoggedIn();

	SetStartLoginMode();
}

void FAuthDialogs::Release()
{
	for (DialogPtr NextGlobalDialog : Dialogs)
	{
		NextGlobalDialog->Release();
	}
}

void FAuthDialogs::Init()
{
	UpdateLoginButtonsState();
}

void FAuthDialogs::SetPosition(Vector2 Pos)
{

}

void FAuthDialogs::SetSize(Vector2 NewSize)
{

}

void FAuthDialogs::OnUIEvent(const FUIEvent& Event)
{
	if (Event.GetType() == EUIEventType::MousePressed)
	{
		for (DialogPtr NextGlobalDialog : Dialogs)
		{
			if (NextGlobalDialog->CheckCollision(Event.GetVector()))
			{
				NextGlobalDialog->OnUIEvent(Event);
			}
			else
			{
				NextGlobalDialog->SetFocused(false);
			}
		}
	}
	else
	{
		for (DialogPtr NextGlobalDialog : Dialogs)
		{
			NextGlobalDialog->OnUIEvent(Event);
		}
	}
}

void FAuthDialogs::UpdateLayout()
{
	UpdateLoginIDPasswordDialog();
	UpdateLoginExchangeCodeDialog();
	UpdateLoginDevAuthDialog();
	UpdateLoginAccountPortalDialog();
	UpdateMFALoginDialog();
	UpdateUserLoggedIn();
}

void FAuthDialogs::CreateLoginIDPasswordDialog()
{
	float PosX = 10.f;
	float PosY = 120.f;
	float SizeX = 230.f;
	float SizeY = 230.f;

	std::shared_ptr<FTextLabelWidget> LoginLabel = std::make_shared<FTextLabelWidget>(
		Vector2(PosX + 40.f, PosY),
		Vector2(170.f, 30.f),
		ParentDialog->GetLayer() - 1,
		L"Log in to access " + LoginText + L".",
		L"");
	LoginLabel->Create();
	LoginLabel->SetFont(BoldSmallFont);

	std::shared_ptr<FTextLabelWidget> AuthLabel = std::make_shared<FTextLabelWidget>(
		Vector2(PosX - 50.f, 50.f),
		Vector2(170.f, 30.f),
		ParentDialog->GetLayer() - 1,
		L"Please Enter Credentials",
		L"");
	AuthLabel->Create();
	AuthLabel->SetFont(BoldSmallFont);

	std::shared_ptr<FTextFieldWidget> AuthUserIdField = std::make_shared<FTextFieldWidget>(
		Vector2(PosX, PosY + 60.f),
		Vector2(SizeX, 30.f),
		ParentDialog->GetLayer() - 1,
		L"Email",
		L"Assets/textfield.dds",
		TinyFont,
		FTextFieldWidget::EInputType::Email,
		EAlignmentType::Left);
	AuthUserIdField->Create();
	AuthUserIdField->SetBorderColor(Color::UIBorderGrey);

	std::shared_ptr<FTextFieldWidget> AuthUserPasswordField = std::make_shared<FTextFieldWidget>(
		Vector2(PosX, PosY + 110.f),
		Vector2(SizeX, 30.f),
		ParentDialog->GetLayer() - 1,
		L"Password",
		L"Assets/textfield.dds",
		TinyFont,
		FTextFieldWidget::EInputType::Password,
		EAlignmentType::Left);
	AuthUserPasswordField->Create();
	AuthUserPasswordField->SetBorderColor(Color::UIBorderGrey);
	AuthUserPasswordField->SetOnEnterPressedCallback([AuthUserIdField, this](const std::wstring& FieldValue)
	{
		if (IsDialogReadyForInput() && !AuthUserIdField->GetText().empty() && !FieldValue.empty())
		{
			FGameEvent Event(EGameEventType::StartUserLogin, (int)ELoginMode::IDPassword, AuthUserIdField->GetText(), FieldValue);
			FGame::Get().OnGameEvent(Event);
		}
	});
	AuthUserIdField->SetOnEnterPressedCallback([AuthUserPasswordField, this](const std::wstring& FieldValue)
	{
		if (IsDialogReadyForInput() && !AuthUserPasswordField->GetText().empty() && !FieldValue.empty())
		{
			FGameEvent Event(EGameEventType::StartUserLogin, (int)ELoginMode::IDPassword, FieldValue, AuthUserPasswordField->GetText());
			FGame::Get().OnGameEvent(Event);
		}
	});

#ifdef DEV_BUILD
	// Use Command Line vars to populate id and password if they exist
	std::wstring UserId = FCommandLine::Get().GetParamValue(CommandLineConstants::DevUsername);
	std::wstring UserPwd = FCommandLine::Get().GetParamValue(CommandLineConstants::DevPassword);

	if (!UserId.empty()) AuthUserIdField->SetText(UserId);
	if (!UserPwd.empty()) AuthUserPasswordField->SetText(UserPwd);
#endif

	std::shared_ptr<FButtonWidget> AuthButton = std::make_shared<FButtonWidget>(
		Vector2(PosX + 60.f, PosY + 180.f),
		Vector2(100.f, 30.f),
		ParentDialog->GetLayer() - 1,
		L"LOG IN",
		assets::DefaultButtonAssets,
		BoldSmallFont,
		AuthButtonBackCol);
	AuthButton->Create();
	AuthButton->SetOnPressedCallback([AuthUserIdField, AuthUserPasswordField, this]()
	{
		if (IsDialogReadyForInput() && !AuthUserIdField->GetText().empty() && !AuthUserPasswordField->GetText().empty())
		{
			FGameEvent Event(EGameEventType::StartUserLogin, (int)ELoginMode::IDPassword, AuthUserIdField->GetText(), AuthUserPasswordField->GetText());
			FGame::Get().OnGameEvent(Event);
		}
	});
	AuthButton->SetBackgroundColors(assets::DefaultButtonColors);

	std::shared_ptr<FButtonWidget> CancelButton = std::make_shared<FButtonWidget>(
		Vector2(0.f, 40.f),
		Vector2(80.f, 25.f),
		ParentDialog->GetLayer() - 1,
		L"CANCEL",
		assets::DefaultButtonAssets,
		SmallFont,
		AuthButtonBackCol);
	CancelButton->Create();
	CancelButton->SetOnPressedCallback([this]()
	{
		if (IsDialogReadyForInput())
		{
			FGameEvent Event(EGameEventType::CancelLogin);
			FGame::Get().OnGameEvent(Event);
		}
	});
	CancelButton->SetBackgroundColors(assets::DefaultButtonColors);

	LoginIDPasswordDialog = std::make_shared<FDialog>(Vector2(10, 10), Vector2(10, 10), ParentDialog->GetLayer() - 1);
	LoginIDPasswordDialog->AddWidget(LoginLabel);
	LoginIDPasswordDialog->AddWidget(AuthLabel);
	LoginIDPasswordDialog->AddWidget(AuthUserIdField);
	LoginIDPasswordDialog->AddWidget(AuthUserPasswordField);
	LoginIDPasswordDialog->AddWidget(AuthButton);
	LoginIDPasswordDialog->AddWidget(CancelButton);

	CreateLoginMethodWidgets(LoginIDPasswordDialog, ELoginMode::IDPassword, PosX);

	AddDialog(LoginIDPasswordDialog);
}

void FAuthDialogs::UpdateLoginIDPasswordDialog()
{
	if (LoginIDPasswordDialog && ParentDialog)
	{
		Vector2 ParentPos = ParentDialog->GetPosition();
		Vector2 ParentSize = ParentDialog->GetSize();

		LoginIDPasswordDialog->SetPosition(ParentPos);
		LoginIDPasswordDialog->SetSize(ParentSize);

		float PosX = 0.f;
		float PosY = 80.f;
		float SizeX = ParentSize.x;
		float SizeY = ParentSize.y;

		WidgetPtr LoginLabel = LoginIDPasswordDialog->GetWidget(0);
		if (LoginLabel)
		{
			float LLPosX = PosX + ((SizeX / 2.f) - (LoginLabel->GetSize().x / 2.f)) + 5.f;
			LoginLabel->SetPosition(Vector2(LLPosX, PosY) + ParentPos);
		}

		WidgetPtr AuthLabel1 = LoginIDPasswordDialog->GetWidget(1);
		if (AuthLabel1)
		{
			float AL1PosX = PosX + ((SizeX / 2.f) - (AuthLabel1->GetSize().x / 2.f)) + 5.f;
			AuthLabel1->SetPosition(Vector2(AL1PosX, PosY + 100.f) + ParentPos);
		}

		WidgetPtr AuthUserIdField = LoginIDPasswordDialog->GetWidget(2);
		if (AuthUserIdField)
		{
			AuthUserIdField->SetSize(Vector2(SizeX - 15.f, 30.f));
			AuthUserIdField->SetPosition(Vector2(PosX + 8.f, PosY + 140.f) + ParentPos);
		}

		WidgetPtr AuthUserPasswordField = LoginIDPasswordDialog->GetWidget(3);
		if (AuthUserPasswordField)
		{
			AuthUserPasswordField->SetSize(Vector2(SizeX - 15.f, 30.f));
			AuthUserPasswordField->SetPosition(Vector2(PosX + 8.f, PosY + 180.f) + ParentPos);
		}

		WidgetPtr AuthButton = LoginIDPasswordDialog->GetWidget(4);
		if (AuthButton)
		{
			float ABPosX = PosX + (SizeX / 2.f) - (AuthButton->GetSize().x / 2.f);
			AuthButton->SetPosition(Vector2(ABPosX, SizeY - AuthButton->GetSize().y - (SizeY * 0.025f)) + ParentPos);
		}

		WidgetPtr CancelButton = LoginIDPasswordDialog->GetWidget(5);
		if (CancelButton)
		{
			float CNPosX = PosX + (SizeX / 2.f) - (CancelButton->GetSize().x / 2.f);
			CancelButton->SetPosition(Vector2(CNPosX, SizeY - CancelButton->GetSize().y - 60.f) + ParentPos);
			if (FPlayerManager::Get().GetNumPlayers() > 0) CancelButton->Show(); else CancelButton->Hide();
		}

		UpdateLoginMethodButtons(LoginIDPasswordDialog);

		LoginIDPasswordDialog->SetPosition(ParentPos);
	}
}

void FAuthDialogs::CreateLoginExchangeCodeDialog()
{
	LoginExchangeCodeDialog = std::make_shared<FDialog>(Vector2(50, 50), Vector2(650, 700), ParentDialog->GetLayer() - 1);

	float PosX = 20.f;

	std::shared_ptr<FTextLabelWidget> LoginLabel = std::make_shared<FTextLabelWidget>(
		Vector2(PosX + 40.f, 10.f),
		Vector2(170.f, 30.f),
		LoginExchangeCodeDialog->GetLayer() - 1,
		L"Log in to access " + LoginText + L".",
		L"");
	LoginLabel->Create();
	LoginLabel->SetFont(BoldSmallFont);

	std::shared_ptr<FTextLabelWidget> AuthLabel = std::make_shared<FTextLabelWidget>(
		Vector2(PosX - 50.f, 50.f),
		Vector2(180.f, 30.f),
		LoginExchangeCodeDialog->GetLayer() - 1,
		L"Please Enter Exchange Code",
		L"");
	AuthLabel->Create();
	AuthLabel->SetFont(BoldSmallFont);

	std::shared_ptr<FTextLabelWidget> AuthLabel2 = std::make_shared<FTextLabelWidget>(
		Vector2(PosX - 50.f, 70.f),
		Vector2(160.f, 30.f),
		LoginExchangeCodeDialog->GetLayer() - 1,
		L"Typically generated by launcher.",
		L"");
	AuthLabel2->Create();
	AuthLabel2->SetFont(TinyFont);

	std::shared_ptr<FTextFieldWidget> AuthExchangeCodeField = std::make_shared<FTextFieldWidget>(
		Vector2(PosX, 110.f),
		Vector2(230.f, 30.f),
		LoginExchangeCodeDialog->GetLayer() - 1,
		L"Exchange Code",
		L"Assets/textfield.dds",
		TinyFont,
		FTextFieldWidget::EInputType::Email,
		EAlignmentType::Left);
	AuthExchangeCodeField->Create();
	AuthExchangeCodeField->SetBorderColor(Color::UIBorderGrey);
	AuthExchangeCodeField->SetOnEnterPressedCallback([this](const std::wstring& FieldValue)
	{
		if (IsDialogReadyForInput() && !FieldValue.empty())
		{
			FGameEvent Event(EGameEventType::StartUserLogin, (int)ELoginMode::ExchangeCode, FieldValue, L"");
			FGame::Get().OnGameEvent(Event);
		}
	});

	// Use Command Line vars to populate exchange code if it exists
	std::wstring ExchangeCode = FCommandLine::Get().GetParamValue(CommandLineConstants::ExchangeCode);
	if (!ExchangeCode.empty()) AuthExchangeCodeField->SetText(ExchangeCode);

	std::shared_ptr<FButtonWidget> AuthButton = std::make_shared<FButtonWidget>(
		Vector2(PosX, 170.f),
		Vector2(100.f, 30.f),
		LoginExchangeCodeDialog->GetLayer() - 1,
		L"LOG IN",
		assets::DefaultButtonAssets,
		BoldSmallFont,
		AuthButtonBackCol);
	AuthButton->Create();
	AuthButton->SetOnPressedCallback([AuthExchangeCodeField, this]()
	{
		if (IsDialogReadyForInput() && !AuthExchangeCodeField->GetText().empty())
		{
			FGameEvent Event(EGameEventType::StartUserLogin, (int)ELoginMode::ExchangeCode, AuthExchangeCodeField->GetText(), L"");
			FGame::Get().OnGameEvent(Event);
		}
	});
	AuthButton->SetBackgroundColors(assets::DefaultButtonColors);

	std::shared_ptr<FButtonWidget> CancelButton = std::make_shared<FButtonWidget>(
		Vector2(0.f, 40.f),
		Vector2(80.f, 25.f),
		LoginExchangeCodeDialog->GetLayer() - 1,
		L"CANCEL",
		assets::DefaultButtonAssets,
		SmallFont,
		AuthButtonBackCol);
	CancelButton->Create();
	CancelButton->SetOnPressedCallback([this]()
	{
		if (IsDialogReadyForInput())
		{
			FGameEvent Event(EGameEventType::CancelLogin);
			FGame::Get().OnGameEvent(Event);
		}
	});
	CancelButton->SetBackgroundColors(assets::DefaultButtonColors);

	LoginExchangeCodeDialog->AddWidget(LoginLabel);
	LoginExchangeCodeDialog->AddWidget(AuthLabel);
	LoginExchangeCodeDialog->AddWidget(AuthLabel2);
	LoginExchangeCodeDialog->AddWidget(AuthExchangeCodeField);
	LoginExchangeCodeDialog->AddWidget(AuthButton);
	LoginExchangeCodeDialog->AddWidget(CancelButton);

	CreateLoginMethodWidgets(LoginExchangeCodeDialog, ELoginMode::ExchangeCode, PosX);

	AddDialog(LoginExchangeCodeDialog);
}

void FAuthDialogs::UpdateLoginExchangeCodeDialog()
{
	if (LoginExchangeCodeDialog && ParentDialog)
	{
		Vector2 ParentPos = ParentDialog->GetPosition();
		Vector2 ParentSize = ParentDialog->GetSize();

		LoginExchangeCodeDialog->SetPosition(ParentPos);
		LoginExchangeCodeDialog->SetSize(ParentSize);

		float PosX = 0.f;
		float PosY = 80.f;
		float SizeX = ParentSize.x;
		float SizeY = ParentSize.y;

		WidgetPtr LoginLabel = LoginExchangeCodeDialog->GetWidget(0);
		float LLPosX = PosX + ((SizeX / 2.f) - (LoginLabel->GetSize().x / 2.f)) + 5.f;
		LoginLabel->SetPosition(Vector2(LLPosX, PosY) + ParentPos);

		WidgetPtr AuthLabel1 = LoginExchangeCodeDialog->GetWidget(1);
		float AL1PosX = PosX + ((SizeX / 2.f) - (AuthLabel1->GetSize().x / 2.f)) + 5.f;
		AuthLabel1->SetPosition(Vector2(AL1PosX, PosY + 100.f) + ParentPos);

		WidgetPtr AuthLabel2 = LoginExchangeCodeDialog->GetWidget(2);
		float AL2PosX = PosX + ((SizeX / 2.f) - (AuthLabel2->GetSize().x / 2.f));
		AuthLabel2->SetPosition(Vector2(AL2PosX, PosY + 120.f) + ParentPos);

		WidgetPtr AuthExchangeCodeField = LoginExchangeCodeDialog->GetWidget(3);
		AuthExchangeCodeField->SetSize(Vector2(SizeX - 15.f, 30.f));
		AuthExchangeCodeField->SetPosition(Vector2(PosX + 8.f, PosY + 160.f) + ParentPos);

		WidgetPtr AuthButton = LoginExchangeCodeDialog->GetWidget(4);
		float ABPosX = PosX + (SizeX / 2.f) - (AuthButton->GetSize().x / 2.f);
		AuthButton->SetPosition(Vector2(ABPosX, SizeY - AuthButton->GetSize().y - (SizeY * 0.025f)) + ParentPos);

		WidgetPtr CancelButton = LoginExchangeCodeDialog->GetWidget(5);
		float CNPosX = PosX + (SizeX / 2.f) - (CancelButton->GetSize().x / 2.f);
		CancelButton->SetPosition(Vector2(CNPosX, SizeY - CancelButton->GetSize().y - 60.f) + ParentPos);
		if (FPlayerManager::Get().GetNumPlayers() > 0) CancelButton->Show(); else CancelButton->Hide();

		UpdateLoginMethodButtons(LoginExchangeCodeDialog);

		LoginExchangeCodeDialog->SetPosition(ParentPos);
	}
}

void FAuthDialogs::CreateLoginDevAuthDialog()
{
	float PosX = 10.f;
	float PosY = 120.f;
	float SizeX = 230.f;
	float SizeY = 230.f;

	std::shared_ptr<FTextLabelWidget> LoginLabel = std::make_shared<FTextLabelWidget>(
		Vector2(PosX + 40.f, PosY),
		Vector2(170.f, 30.f),
		ParentDialog->GetLayer() - 1,
		L"Log in to access " + LoginText + L".",
		L"");
	LoginLabel->Create();
	LoginLabel->SetFont(BoldSmallFont);

	std::shared_ptr<FTextLabelWidget> AuthLabel = std::make_shared<FTextLabelWidget>(
		Vector2(PosX - 50.f, 50.f),
		Vector2(SizeX, 30.f),
		ParentDialog->GetLayer() - 1,
		L"Please Enter DevAuthTool Credentials",
		L"");
	AuthLabel->Create();
	AuthLabel->SetFont(BoldSmallFont);

	std::shared_ptr<FTextLabelWidget> AuthHintLabel = std::make_shared<FTextLabelWidget>(
		Vector2(PosX - 50.f, 60.f),
		Vector2(SizeX, 30.f),
		ParentDialog->GetLayer() - 1,
		L"Specify DevAuthTool location and the name.",
		L"");
	AuthHintLabel->Create();
	AuthHintLabel->SetFont(TinyFont);

	std::shared_ptr<FTextFieldWidget> AuthHostField = std::make_shared<FTextFieldWidget>(
		Vector2(PosX, PosY + 80.f),
		Vector2(SizeX, 30.f),
		ParentDialog->GetLayer() - 1,
		L"Host (e.g. 127.0.0.1:10000)",
		L"Assets/textfield.dds",
		TinyFont,
		FTextFieldWidget::EInputType::Normal,
		EAlignmentType::Left);
	AuthHostField->Create();
	AuthHostField->SetBorderColor(Color::UIBorderGrey);

	std::shared_ptr<FTextFieldWidget> AuthCredNameField = std::make_shared<FTextFieldWidget>(
		Vector2(PosX, PosY + 120.f),
		Vector2(SizeX, 30.f),
		ParentDialog->GetLayer() - 1,
		L"Credentials",
		L"Assets/textfield.dds",
		TinyFont,
		FTextFieldWidget::EInputType::Normal,
		EAlignmentType::Left);
	AuthCredNameField->Create();
	AuthCredNameField->SetBorderColor(Color::UIBorderGrey);
	AuthCredNameField->SetOnEnterPressedCallback([AuthHostField, this](const std::wstring& FieldValue)
	{
		if (IsDialogReadyForInput() && !AuthHostField->GetText().empty() && !FieldValue.empty())
		{
			FGameEvent Event(EGameEventType::StartUserLogin, (int)ELoginMode::DevAuth, AuthHostField->GetText(), FieldValue);
			FGame::Get().OnGameEvent(Event);
		}
	});
	AuthHostField->SetOnEnterPressedCallback([AuthCredNameField, this](const std::wstring& FieldValue)
	{
		if (IsDialogReadyForInput() && !AuthCredNameField->GetText().empty() && !FieldValue.empty())
		{
			FGameEvent Event(EGameEventType::StartUserLogin, (int)ELoginMode::DevAuth, FieldValue, AuthCredNameField->GetText());
			FGame::Get().OnGameEvent(Event);
		}
	});

	// Use Command Line vars to populate host and cred if they exist
	std::wstring DevHost = FCommandLine::Get().GetParamValue(CommandLineConstants::DevAuthHost);
	if (!DevHost.empty())
	{
		AuthHostField->SetText(DevHost);
	}
	else
	{
		// Try adding host from settings file
		if (FSettings::Get().TryGetAsString(SettingsConstants::DevAuthHost, DevHost))
		{
			AuthHostField->SetText(DevHost);
		}
	}

	std::wstring DevCred = FCommandLine::Get().GetParamValue(CommandLineConstants::DevAuthCred);
	if (!DevCred.empty())
	{
		AuthCredNameField->SetText(DevCred);
	}
	else
	{
		// Try adding creds from settings file
		if (FSettings::Get().TryGetAsString(SettingsConstants::DevAuthCred, DevCred))
		{
			AuthCredNameField->SetText(DevCred);
		}
	}

	std::shared_ptr<FButtonWidget> AuthButton = std::make_shared<FButtonWidget>(
		Vector2(PosX + 60.f, PosY + 180.f),
		Vector2(100.f, 30.f),
		ParentDialog->GetLayer() - 1,
		L"LOG IN",
		assets::DefaultButtonAssets,
		BoldSmallFont,
		AuthButtonBackCol);
	AuthButton->Create();
	AuthButton->SetOnPressedCallback([AuthHostField, AuthCredNameField, this]()
	{
		if (IsDialogReadyForInput() && !AuthHostField->GetText().empty(), !AuthCredNameField->GetText().empty())
		{
			FGameEvent Event(EGameEventType::StartUserLogin, (int)ELoginMode::DevAuth, AuthHostField->GetText(), AuthCredNameField->GetText());
			FGame::Get().OnGameEvent(Event);
		}
	});
	AuthButton->SetBackgroundColors(assets::DefaultButtonColors);

	std::shared_ptr<FButtonWidget> CancelButton = std::make_shared<FButtonWidget>(
		Vector2(0.f, 40.f),
		Vector2(80.f, 25.f),
		ParentDialog->GetLayer() - 1,
		L"CANCEL",
		assets::DefaultButtonAssets,
		SmallFont,
		AuthButtonBackCol);
	CancelButton->Create();
	CancelButton->SetOnPressedCallback([this]()
	{
		if (IsDialogReadyForInput())
		{
			FGameEvent Event(EGameEventType::CancelLogin);
			FGame::Get().OnGameEvent(Event);
		}
	});
	CancelButton->SetBackgroundColors(assets::DefaultButtonColors);

	LoginDevAuthDialog = std::make_shared<FDialog>(Vector2(10, 10), Vector2(10, 10), ParentDialog->GetLayer() - 1);
	LoginDevAuthDialog->AddWidget(LoginLabel);
	LoginDevAuthDialog->AddWidget(AuthLabel);
	LoginDevAuthDialog->AddWidget(AuthHostField);
	LoginDevAuthDialog->AddWidget(AuthCredNameField);
	LoginDevAuthDialog->AddWidget(AuthButton);
	LoginDevAuthDialog->AddWidget(CancelButton);

	CreateLoginMethodWidgets(LoginDevAuthDialog, ELoginMode::DevAuth, PosX);

	LoginDevAuthDialog->AddWidget(AuthHintLabel);

	AddDialog(LoginDevAuthDialog);
}

void FAuthDialogs::CreateLoginAccountPortalDialog()
{
	LoginAccountPortalDialog = std::make_shared<FDialog>(Vector2(50, 50), Vector2(650, 700), ParentDialog->GetLayer() - 1);

	float PosX = 20.f;

	std::shared_ptr<FTextLabelWidget> LoginLabel = std::make_shared<FTextLabelWidget>(
		Vector2(PosX + 40.f, 10.f),
		Vector2(170.f, 30.f),
		LoginAccountPortalDialog->GetLayer() - 1,
		L"Log in to access " + LoginText + L".",
		L"");
	LoginLabel->Create();
	LoginLabel->SetFont(BoldSmallFont);

	std::shared_ptr<FTextLabelWidget> AuthLabel = std::make_shared<FTextLabelWidget>(
		Vector2(PosX - 50.f, 50.f),
		Vector2(70.f, 30.f),
		LoginAccountPortalDialog->GetLayer() - 1,
		L"Account Portal",
		L"");
	AuthLabel->Create();
	AuthLabel->SetFont(BoldSmallFont);

	std::shared_ptr<FTextLabelWidget> AuthLabel2 = std::make_shared<FTextLabelWidget>(
		Vector2(PosX - 50.f, 70.f),
		Vector2(180.f, 30.f),
		LoginAccountPortalDialog->GetLayer() - 1,
		L"You will be taken to the account portal.",
		L"");
	AuthLabel2->Create();
	AuthLabel2->SetFont(TinyFont);

	std::shared_ptr<FTextLabelWidget> AuthLabel3 = std::make_shared<FTextLabelWidget>(
		Vector2(PosX - 50.f, 70.f),
		Vector2(100.f, 30.f),
		LoginAccountPortalDialog->GetLayer() - 1,
		L"",
		L"");
	AuthLabel3->Create();
	AuthLabel3->SetFont(TinyFont);

	std::shared_ptr<FButtonWidget> AuthButton = std::make_shared<FButtonWidget>(
		Vector2(PosX, 170.f),
		Vector2(100.f, 30.f),
		LoginAccountPortalDialog->GetLayer() - 1,
		L"LOG IN",
		assets::DefaultButtonAssets,
		BoldSmallFont,
		AuthButtonBackCol);
	AuthButton->Create();
	AuthButton->SetOnPressedCallback([this]()
	{
		if (IsDialogReadyForInput())
		{
			FGameEvent Event(EGameEventType::StartUserLogin, (int)ELoginMode::AccountPortal, L"", L"");
			FGame::Get().OnGameEvent(Event);
		}
	});
	AuthButton->SetBackgroundColors(assets::DefaultButtonColors);

	std::shared_ptr<FButtonWidget> CancelButton = std::make_shared<FButtonWidget>(
		Vector2(0.f, 40.f),
		Vector2(80.f, 25.f),
		LoginAccountPortalDialog->GetLayer() - 1,
		L"CANCEL",
		assets::DefaultButtonAssets,
		SmallFont,
		AuthButtonBackCol);
	CancelButton->Create();
	CancelButton->SetOnPressedCallback([this]()
	{
		if (IsDialogReadyForInput())
		{
			FGameEvent Event(EGameEventType::CancelLogin);
			FGame::Get().OnGameEvent(Event);
		}
	});
	CancelButton->SetBackgroundColors(assets::DefaultButtonColors);

	LoginAccountPortalDialog->AddWidget(LoginLabel);
	LoginAccountPortalDialog->AddWidget(AuthLabel);
	LoginAccountPortalDialog->AddWidget(AuthLabel2);
	LoginAccountPortalDialog->AddWidget(AuthLabel3);
	LoginAccountPortalDialog->AddWidget(AuthButton);
	LoginAccountPortalDialog->AddWidget(CancelButton);

	CreateLoginMethodWidgets(LoginAccountPortalDialog, ELoginMode::AccountPortal, PosX);

	AddDialog(LoginAccountPortalDialog);
}

void FAuthDialogs::UpdateLoginDevAuthDialog()
{
	if (LoginDevAuthDialog && ParentDialog)
	{
		Vector2 ParentPos = ParentDialog->GetPosition();
		Vector2 ParentSize = ParentDialog->GetSize();

		LoginDevAuthDialog->SetPosition(ParentPos);
		LoginDevAuthDialog->SetSize(ParentSize);

		float PosX = 0.f;
		float PosY = 80.f;
		float SizeX = ParentSize.x;
		float SizeY = ParentSize.y;

		WidgetPtr LoginLabel = LoginDevAuthDialog->GetWidget(0);
		if (LoginLabel)
		{
			float LLPosX = PosX + ((SizeX / 2.f) - (LoginLabel->GetSize().x / 2.f)) + 5.f;
			LoginLabel->SetPosition(Vector2(LLPosX, PosY) + ParentPos);
		}

		WidgetPtr AuthLabel1 = LoginDevAuthDialog->GetWidget(1);
		if (AuthLabel1)
		{
			AuthLabel1->SetPosition(Vector2(PosX, PosY + 100.f) + ParentPos);
			AuthLabel1->SetSize(Vector2(SizeX, 30.0f));
		}

		WidgetPtr AuthHintLabel = LoginDevAuthDialog->GetWidget(LoginDevAuthDialog->GetNumWidgets() - 1);
		if (AuthHintLabel)
		{
			AuthHintLabel->SetPosition(Vector2(PosX, PosY + 130.f) + ParentPos);
			AuthHintLabel->SetSize(Vector2(SizeX, 30.0f));
		}

		WidgetPtr AuthDevHostField = LoginDevAuthDialog->GetWidget(2);
		if (AuthDevHostField)
		{
			AuthDevHostField->SetSize(Vector2(SizeX - 15.f, 30.f));
			AuthDevHostField->SetPosition(Vector2(PosX + 8.f, PosY + 165.f) + ParentPos);
		}

		WidgetPtr AuthDevCredField = LoginDevAuthDialog->GetWidget(3);
		if (AuthDevCredField)
		{
			AuthDevCredField->SetSize(Vector2(SizeX - 15.f, 30.f));
			AuthDevCredField->SetPosition(Vector2(PosX + 8.f, PosY + 205.0f) + ParentPos);
		}

		WidgetPtr AuthButton = LoginDevAuthDialog->GetWidget(4);
		if (AuthButton)
		{
			float ABPosX = PosX + (SizeX / 2.f) - (AuthButton->GetSize().x / 2.f);
			AuthButton->SetPosition(Vector2(ABPosX, SizeY - AuthButton->GetSize().y - (SizeY * 0.025f)) + ParentPos);
		}

		WidgetPtr CancelButton = LoginDevAuthDialog->GetWidget(5);
		if (CancelButton)
		{
			float CNPosX = PosX + (SizeX / 2.f) - (CancelButton->GetSize().x / 2.f);
			CancelButton->SetPosition(Vector2(CNPosX, SizeY - CancelButton->GetSize().y - 60.f) + ParentPos);
			if (FPlayerManager::Get().GetNumPlayers() > 0) CancelButton->Show(); else CancelButton->Hide();
		}

		UpdateLoginMethodButtons(LoginDevAuthDialog);

		LoginDevAuthDialog->SetPosition(ParentPos);
	}
}

void FAuthDialogs::UpdateLoginAccountPortalDialog()
{
	if (LoginAccountPortalDialog && ParentDialog)
	{
		Vector2 ParentPos = ParentDialog->GetPosition();
		Vector2 ParentSize = ParentDialog->GetSize();

		LoginAccountPortalDialog->SetPosition(ParentPos);
		LoginAccountPortalDialog->SetSize(ParentSize);

		float PosX = 0.f;
		float PosY = 80.f;
		float SizeX = ParentSize.x;
		float SizeY = ParentSize.y;

		WidgetPtr LoginLabel = LoginAccountPortalDialog->GetWidget(0);
		float LLPosX = PosX + ((SizeX / 2.f) - (LoginLabel->GetSize().x / 2.f)) + 5.f;
		LoginLabel->SetPosition(Vector2(LLPosX, PosY) + ParentPos);

		WidgetPtr AuthLabel1 = LoginAccountPortalDialog->GetWidget(1);
		float AL1PosX = PosX + ((SizeX / 2.f) - (AuthLabel1->GetSize().x / 2.f)) + 5.f;
		AuthLabel1->SetPosition(Vector2(AL1PosX, PosY + 100.f) + ParentPos);

		WidgetPtr AuthLabel2 = LoginAccountPortalDialog->GetWidget(2);
		float AL2PosX = PosX + ((SizeX / 2.f) - (AuthLabel2->GetSize().x / 2.f));
		AuthLabel2->SetPosition(Vector2(AL2PosX, PosY + 120.f) + ParentPos);

		WidgetPtr AuthLabel3 = LoginAccountPortalDialog->GetWidget(3);
		float AL3PosX = PosX + ((SizeX / 2.f) - (AuthLabel3->GetSize().x / 2.f));
		AuthLabel3->SetPosition(Vector2(AL3PosX, PosY + 135.f) + ParentPos);

		WidgetPtr AuthButton = LoginAccountPortalDialog->GetWidget(4);
		float ABPosX = PosX + (SizeX / 2.f) - (AuthButton->GetSize().x / 2.f);
		AuthButton->SetPosition(Vector2(ABPosX, SizeY - AuthButton->GetSize().y - (SizeY * 0.025f)) + ParentPos);

		WidgetPtr CancelButton = LoginAccountPortalDialog->GetWidget(5);
		float CNPosX = PosX + (SizeX / 2.f) - (CancelButton->GetSize().x / 2.f);
		CancelButton->SetPosition(Vector2(CNPosX, SizeY - CancelButton->GetSize().y - 60.f) + ParentPos);
		if (FPlayerManager::Get().GetNumPlayers() > 0) CancelButton->Show(); else CancelButton->Hide();

		UpdateLoginMethodButtons(LoginAccountPortalDialog);

		LoginAccountPortalDialog->SetPosition(ParentPos);
	}
}


void FAuthDialogs::UpdateLoginMethodButtons(DialogPtr LoginDialog)
{
	const Vector2 ParentPos = ParentDialog->GetPosition();
	const Vector2 ParentSize = ParentDialog->GetSize();

	const float PosX = 0.f;
	const float PosY = 140.f;
	const float SpX = 5.f; // spacing between each button

	const float NumLoginMethods = float(LoginMethods.size());

	const float SizeX = (ParentDialog->GetSize().x - ((NumLoginMethods + 1) * SpX)) / NumLoginMethods;
	const Vector2 Size = Vector2(SizeX, 25.f);

	const float FirstButtonPosX = PosX + SpX;
	const size_t WidgetIndexOffset = 6;

	for (size_t MethodIndex = 0; MethodIndex < LoginMethods.size(); ++MethodIndex)
	{
		const float ButtonPosX = FirstButtonPosX + (SizeX + SpX) * MethodIndex;
		WidgetPtr MethodButton = LoginDialog->GetWidget(WidgetIndexOffset + MethodIndex);
		if (MethodButton)
		{
			MethodButton->SetSize(Size);
			MethodButton->SetPosition(Vector2(ButtonPosX, PosY) + ParentPos);
		}
	}
}

void FAuthDialogs::CreateMFALoginDialog()
{
	MFALoginDialog = std::make_shared<FDialog>(Vector2(50, 50), Vector2(650, 700), ParentDialog->GetLayer() - 1);

	float PosX = 20.f;

	std::shared_ptr<FTextLabelWidget> LoginLabel = std::make_shared<FTextLabelWidget>(
		Vector2(PosX + 40.f, 10.f),
		Vector2(170.f, 30.f),
		MFALoginDialog->GetLayer() - 1,
		L"Log in to access " + LoginText + L".",
		L"");
	LoginLabel->Create();
	LoginLabel->SetFont(BoldSmallFont);

	std::shared_ptr<FTextLabelWidget> AuthLabel = std::make_shared<FTextLabelWidget>(
		Vector2(PosX - 50.f, 50.f),
		Vector2(160.f, 30.f),
		MFALoginDialog->GetLayer() - 1,
		L"Please Enter MFA Code",
		L"");
	AuthLabel->Create();
	AuthLabel->SetFont(BoldSmallFont);

	std::shared_ptr<FTextLabelWidget> AuthLabel2 = std::make_shared<FTextLabelWidget>(
		Vector2(PosX - 50.f, 70.f),
		Vector2(150.f, 30.f),
		MFALoginDialog->GetLayer() - 1,
		L"MFA required to continue",
		L"");
	AuthLabel2->Create();
	AuthLabel2->SetFont(BoldSmallFont);

	std::shared_ptr<FTextFieldWidget> AuthMFACodeField = std::make_shared<FTextFieldWidget>(
		Vector2(PosX, 110.f),
		Vector2(180.f, 30.f),
		MFALoginDialog->GetLayer() - 1,
		L"MFA Code",
		L"Assets/textfield.dds",
		TinyFont,
		FTextFieldWidget::EInputType::Email,
		EAlignmentType::Left);
	AuthMFACodeField->Create();
	AuthMFACodeField->SetBorderColor(Color::UIBorderGrey);
	AuthMFACodeField->SetOnEnterPressedCallback([this](const std::wstring& FieldValue)
	{
		if (IsDialogReadyForInput() && !FieldValue.empty())
		{
			FGameEvent Event(EGameEventType::UserLoginEnteredMFA, (int)ELoginMode::ExchangeCode, FieldValue, FPlayerManager::Get().GetCurrentUser());
			FGame::Get().OnGameEvent(Event);
		}
	});

	std::shared_ptr<FButtonWidget> AuthButton = std::make_shared<FButtonWidget>(
		Vector2(PosX, 170.f),
		Vector2(100.f, 30.f),
		MFALoginDialog->GetLayer() - 1,
		L"CONTINUE",
		assets::DefaultButtonAssets,
		BoldSmallFont,
		AuthButtonBackCol);
	AuthButton->Create();
	AuthButton->SetOnPressedCallback([AuthMFACodeField, this]()
	{
		if (IsDialogReadyForInput() && !AuthMFACodeField->GetText().empty())
		{
			FGameEvent Event(EGameEventType::UserLoginEnteredMFA, (int)ELoginMode::ExchangeCode, AuthMFACodeField->GetText(), FPlayerManager::Get().GetCurrentUser());
			FGame::Get().OnGameEvent(Event);
		}
	});
	AuthButton->SetBackgroundColors(assets::DefaultButtonColors);

	MFALoginDialog->AddWidget(LoginLabel);
	MFALoginDialog->AddWidget(AuthLabel);
	MFALoginDialog->AddWidget(AuthLabel2);
	MFALoginDialog->AddWidget(AuthMFACodeField);
	MFALoginDialog->AddWidget(AuthButton);

	AddDialog(MFALoginDialog);

	HideDialog(MFALoginDialog);
}

void FAuthDialogs::UpdateMFALoginDialog()
{
	if (MFALoginDialog && ParentDialog)
	{
		Vector2 ParentPos = ParentDialog->GetPosition();
		Vector2 ParentSize = ParentDialog->GetSize();

		MFALoginDialog->SetPosition(ParentPos);
		MFALoginDialog->SetSize(ParentSize);

		float PosX = 0.f;
		float PosY = 80.f;
		float SizeX = ParentSize.x;
		float SizeY = ParentSize.y;

		WidgetPtr LoginLabel = MFALoginDialog->GetWidget(0);
		float LLPosX = PosX + ((SizeX / 2.f) - (LoginLabel->GetSize().x / 2.f)) + 5.f;
		LoginLabel->SetPosition(Vector2(LLPosX, PosY) + ParentPos);

		WidgetPtr AuthLabel1 = MFALoginDialog->GetWidget(1);
		float AL1PosX = PosX + ((SizeX / 2.f) - (AuthLabel1->GetSize().x / 2.f)) + 5.f;
		AuthLabel1->SetPosition(Vector2(AL1PosX, PosY + 100.f) + ParentPos);

		WidgetPtr AuthLabel2 = MFALoginDialog->GetWidget(2);
		float AL2PosX = PosX + ((SizeX / 2.f) - (AuthLabel2->GetSize().x / 2.f)) + 5.f;
		AuthLabel2->SetPosition(Vector2(AL2PosX, PosY + 120.f) + ParentPos);

		WidgetPtr AuthMFACodeField = MFALoginDialog->GetWidget(3);
		AuthMFACodeField->SetSize(Vector2(SizeX - 15.f, 30.f));
		AuthMFACodeField->SetPosition(Vector2(PosX + 8.f, PosY + 160.f) + ParentPos);

		WidgetPtr AuthButton = MFALoginDialog->GetWidget(4);
		float ABPosX = PosX + (SizeX / 2.f) - (AuthButton->GetSize().x / 2.f);
		AuthButton->SetPosition(Vector2(ABPosX, SizeY - AuthButton->GetSize().y - (SizeY * 0.025f)) + ParentPos);

		MFALoginDialog->SetPosition(ParentPos);
	}
}

void FAuthDialogs::CreateUserLoggedIn()
{
	UserLoggedInDialog = std::make_shared<FDialog>(Vector2(50.f, 50.f), Vector2(650.f, 700.f), ParentDialog->GetLayer() - 1);

	UserLoggedInLabel = std::make_shared<FTextLabelWidget>(
		Vector2(0.f, 0.f),
		Vector2(200.f, 30.f),
		UserLoggedInDialog->GetLayer() - 1,
		L"Pending...",
		L"",
		FColor(1.f, 1.f, 1.f, 1.f),
		FColor(1.f, 1.f, 1.f, 1.f),
		EAlignmentType::Right);
	UserLoggedInLabel->Create();
	UserLoggedInLabel->SetFont(SmallFont);
	UserLoggedInLabel->SetBackgroundVisible(false);

	std::shared_ptr<FButtonWidget> AuthButton = std::make_shared<FButtonWidget>(
		Vector2(0.f, 40.f),
		Vector2(100.f, 30.f),
		UserLoggedInDialog->GetLayer() - 1,
		L"LOG OUT",
		assets::DefaultButtonAssets,
		BoldSmallFont,
		AuthButtonBackCol);
	AuthButton->Create();
	AuthButton->SetOnPressedCallback([this]()
	{
		if (IsDialogReadyForInput())
		{
			FGameEvent Event(EGameEventType::UserLogOutTriggered, FPlayerManager::Get().GetCurrentUser());
			FGame::Get().OnGameEvent(Event);
		}
	});
	AuthButton->SetBackgroundColors(assets::DefaultButtonColors);

	std::shared_ptr<FButtonWidget> NewUserButton = std::make_shared<FButtonWidget>(
		Vector2(0.f, 40.f),
		Vector2(80.f, 25.f),
		UserLoggedInDialog->GetLayer() - 1,
		L"NEW",
		assets::DefaultButtonAssets,
		TinyFont,
		AuthButtonBackCol);
	NewUserButton->Create();
	NewUserButton->SetOnPressedCallback([this]()
	{
		if (IsDialogReadyForInput())
		{
			FGameEvent Event(EGameEventType::NewUserLogin);
			FGame::Get().OnGameEvent(Event);
		}
	});
	NewUserButton->SetBackgroundColors(assets::DefaultButtonColors);

	std::shared_ptr<FButtonWidget> PrevUserButton = std::make_shared<FButtonWidget>(
		Vector2(0.f, 40.f),
		Vector2(25.f, 25.f),
		UserLoggedInDialog->GetLayer() - 1,
		L"<",
		assets::DefaultButtonAssets,
		TinyFont,
		AuthButtonBackCol);
	PrevUserButton->Create();
	PrevUserButton->SetOnPressedCallback([this]()
	{
		if (IsDialogReadyForInput())
		{
			FGameEvent Event(EGameEventType::ShowPrevUser);
			FGame::Get().OnGameEvent(Event);
		}
	});
	PrevUserButton->SetBackgroundColors(assets::DefaultButtonColors);
	PrevUserButton->Disable();

	std::shared_ptr<FButtonWidget> NextUserButton = std::make_shared<FButtonWidget>(
		Vector2(0.f, 40.f),
		Vector2(25.f, 25.f),
		UserLoggedInDialog->GetLayer() - 1,
		L">",
		assets::DefaultButtonAssets,
		TinyFont,
		AuthButtonBackCol);
	NextUserButton->Create();
	NextUserButton->SetOnPressedCallback([this]()
	{
		if (IsDialogReadyForInput())
		{
			FGameEvent Event(EGameEventType::ShowNextUser);
			FGame::Get().OnGameEvent(Event);
		}
	});
	NextUserButton->SetBackgroundColors(assets::DefaultButtonColors);
	NextUserButton->Disable();

	UserLoggedInDialog->AddWidget(UserLoggedInLabel);
	UserLoggedInDialog->AddWidget(AuthButton);
	UserLoggedInDialog->AddWidget(NewUserButton);
	UserLoggedInDialog->AddWidget(PrevUserButton);
	UserLoggedInDialog->AddWidget(NextUserButton);

	AddDialog(UserLoggedInDialog);

	HideDialog(UserLoggedInDialog);
}

void FAuthDialogs::UpdateUserLoggedIn()
{
	if (UserLoggedInDialog && ParentDialog)
	{
		Vector2 ParentPos = ParentDialog->GetPosition();
		Vector2 ParentSize = ParentDialog->GetSize();

		UserLoggedInDialog->SetPosition(ParentPos);
		UserLoggedInDialog->SetSize(ParentSize);

		float PosX = 0.f;
		float PosY = 0.f;
		float SizeX = ParentSize.x;
		float SizeY = ParentSize.y;

		WidgetPtr UserLabel = UserLoggedInDialog->GetWidget(0);
		UserLabel->SetSize(Vector2(SizeX - 80.f, 30.0f));
		float ULPosX = PosX + (SizeX - UserLabel->GetSize().x - 10.f);
		UserLabel->SetPosition(Vector2(ULPosX, PosY + 4.0f) + ParentPos + UserLabelOffset);

		WidgetPtr AuthButton = UserLoggedInDialog->GetWidget(1);
		float ABPosX = PosX + (SizeX / 2.f) - (AuthButton->GetSize().x / 2.f);
		AuthButton->SetPosition(Vector2(ABPosX, SizeY - AuthButton->GetSize().y - (SizeY * 0.025f)) + ParentPos);

		WidgetPtr NewUserButton = UserLoggedInDialog->GetWidget(2);
		float NUPosX = PosX + (SizeX / 2.f) - (NewUserButton->GetSize().x / 2.f);
		NewUserButton->SetPosition(Vector2(NUPosX, SizeY - NewUserButton->GetSize().y - 60.f) + ParentPos);

		WidgetPtr PrevUserButton = UserLoggedInDialog->GetWidget(3);
		float PUPosX = PosX + (SizeX / 2.f) - (PrevUserButton->GetSize().x / 2.f) - (NewUserButton->GetSize().x * 0.8f);
		PrevUserButton->SetPosition(Vector2(PUPosX, SizeY - PrevUserButton->GetSize().y - 60.f) + ParentPos);

		WidgetPtr NextUserButton = UserLoggedInDialog->GetWidget(4);
		float NXTPosX = PosX + (SizeX / 2.f) - (NextUserButton->GetSize().x / 2.f) + (NewUserButton->GetSize().x * 0.8f);
		NextUserButton->SetPosition(Vector2(NXTPosX, SizeY - NextUserButton->GetSize().y - 60.f) + ParentPos);

		UserLoggedInDialog->SetPosition(ParentPos);
	}
}

void FAuthDialogs::CreateLoginMethodWidgets(DialogPtr LoginDialog, ELoginMode LoginMode, float PosX)
{
	const float SpX = 5.f; // spacing between each button
	const float PosY = 20.f;

	const float NumLoginMethods = float(LoginMethods.size());
	const float SizeX = (ParentDialog->GetSize().x - (NumLoginMethods * SpX - 1.f)) / NumLoginMethods - 10.0f;
	const Vector2 Size = Vector2(SizeX, 25.f);
	Vector2 ButtonPos(PosX, PosY);

	for(size_t MethodIndex = 0; MethodIndex < LoginMethods.size(); ++MethodIndex)
	{
		ELoginMode LoginMethod = LoginMethods[MethodIndex].LoginMode;
		std::shared_ptr<FButtonWidget> LoginMethodButton = std::make_shared<FButtonWidget>(
			ButtonPos,
			Size,
			LoginDialog->GetLayer() - 1,
			LoginMethods[MethodIndex].Label,
			assets::DefaultButtonAssets,
			TinyFont,
			LoginMode != LoginMethod ? AuthButtonBackCol : AuthButtonBackDisabledCol);
		LoginMethodButton->Create();
		if (LoginMode != LoginMethod) LoginMethodButton->SetOnPressedCallback([this, LoginMethod]() { ChangeLoginMode(LoginMethod); });
		LoginMethodButton->SetBackgroundColors(assets::DefaultButtonColors);
		LoginDialog->AddWidget(LoginMethodButton);

		ButtonPos += Vector2(Size.x + SpX, 0.0f);
	}
}

void FAuthDialogs::ChangeLoginMode(ELoginMode LoginMode)
{
	if (LoginMode != SavedLoginMode)
	{
		NewLoginMode = LoginMode;
	}
}

void FAuthDialogs::DoChangeLoginMode(ELoginMode LoginMode)
{
	HideLoginDialogs();

	HideDialog(MFALoginDialog);

	switch (LoginMode)
	{
		case ELoginMode::IDPassword:
		{
			if (LoginIDPasswordDialog)
			{
				ShowDialog(LoginIDPasswordDialog);
				UpdateLoginIDPasswordDialog();
			}
			break;
		}
		case ELoginMode::ExchangeCode:
		{
			if (LoginExchangeCodeDialog)
			{
				ShowDialog(LoginExchangeCodeDialog);
				UpdateLoginExchangeCodeDialog();
			}
			break;
		}
		case ELoginMode::DevAuth:
		{
			if (LoginDevAuthDialog)
			{
				ShowDialog(LoginDevAuthDialog);
				UpdateLoginDevAuthDialog();
			}
			break;
		}
		case ELoginMode::AccountPortal:
		{
			if (LoginAccountPortalDialog)
			{
				ShowDialog(LoginAccountPortalDialog);
				UpdateLoginAccountPortalDialog();
			}
			break;
		}
		case ELoginMode::PersistentAuth:
		{
			break;
		}
		case ELoginMode::ExternalAuth:
		{
			break;
		}
	}

	SavedLoginMode = LoginMode;

	UpdateLoginButtonsState();
}

void FAuthDialogs::SetStartLoginMode()
{
#ifdef DEV_BUILD
	// Change to Email / Password if both are specified in commandline
	if (FCommandLine::Get().HasParam(CommandLineConstants::DevUsername) &&
		FCommandLine::Get().HasParam(CommandLineConstants::DevPassword))
	{
		ChangeLoginMode(ELoginMode::IDPassword);
		return;
	}
#endif

	ChangeLoginMode(ELoginMode::DevAuth);
}

void FAuthDialogs::UpdateLoginButtonsState()
{
	if (LoginIDPasswordDialog)
	{
		UpdateLoginButtonState(LoginIDPasswordDialog->GetWidget(4));
		UpdateLoginButtonState(LoginIDPasswordDialog->GetWidget(6));
		UpdateLoginButtonState(LoginIDPasswordDialog->GetWidget(7));
		UpdateLoginButtonState(LoginIDPasswordDialog->GetWidget(8));
		UpdateLoginButtonState(LoginIDPasswordDialog->GetWidget(9));
		UpdateLoginButtonState(LoginIDPasswordDialog->GetWidget(10));
	}

	if (LoginExchangeCodeDialog)
	{
		UpdateLoginButtonState(LoginExchangeCodeDialog->GetWidget(4));
		UpdateLoginButtonState(LoginExchangeCodeDialog->GetWidget(6));
		UpdateLoginButtonState(LoginExchangeCodeDialog->GetWidget(7));
		UpdateLoginButtonState(LoginExchangeCodeDialog->GetWidget(8));
		UpdateLoginButtonState(LoginExchangeCodeDialog->GetWidget(9));
		UpdateLoginButtonState(LoginExchangeCodeDialog->GetWidget(10));
	}

	if (LoginDevAuthDialog)
	{
		UpdateLoginButtonState(LoginDevAuthDialog->GetWidget(4));
		UpdateLoginButtonState(LoginDevAuthDialog->GetWidget(6));
		UpdateLoginButtonState(LoginDevAuthDialog->GetWidget(7));
		UpdateLoginButtonState(LoginDevAuthDialog->GetWidget(8));
		UpdateLoginButtonState(LoginDevAuthDialog->GetWidget(9));
		UpdateLoginButtonState(LoginDevAuthDialog->GetWidget(10));
	}

	if (LoginAccountPortalDialog)
	{
		UpdateLoginButtonState(LoginAccountPortalDialog->GetWidget(4));
		UpdateLoginButtonState(LoginAccountPortalDialog->GetWidget(6));
		UpdateLoginButtonState(LoginAccountPortalDialog->GetWidget(7));
		UpdateLoginButtonState(LoginAccountPortalDialog->GetWidget(8));
		UpdateLoginButtonState(LoginAccountPortalDialog->GetWidget(9));
		UpdateLoginButtonState(LoginAccountPortalDialog->GetWidget(10));
	}
}

void FAuthDialogs::UpdateLoginButtonState(WidgetPtr AuthButton)
{
	bool bEnableButton = FPlatform::IsInitialized();

	bool bAutoLoginSettings = false;

	if (FCommandLine::Get().HasParam(CommandLineConstants::AutoLogin) ||
		FSettings::Get().TryGetAsBool(SettingsConstants::AutoLogin, bAutoLoginSettings) ||
		(FCommandLine::Get().HasParam(CommandLineConstants::EpicPortal) &&
		 FCommandLine::Get().HasParam(CommandLineConstants::LauncherAuthType) &&
		 FCommandLine::Get().HasParam(CommandLineConstants::LauncherAuthPassword)))
	{
		// Don't enable buttons if autologin is enabled and we haven't logged in yet
		if (!bHasLoggedIn)
		{
			bEnableButton = false;
		}
	}

	EnableButton(AuthButton, bEnableButton);
}

void FAuthDialogs::EnableLoginButtons(bool bEnable)
{
	if (LoginIDPasswordDialog) EnableButton(LoginIDPasswordDialog->GetWidget(4), bEnable);
	if (LoginExchangeCodeDialog) EnableButton(LoginExchangeCodeDialog->GetWidget(4), bEnable);
	if (LoginDevAuthDialog) EnableButton(LoginDevAuthDialog->GetWidget(4), bEnable);
	if (LoginAccountPortalDialog) EnableButton(LoginAccountPortalDialog->GetWidget(4), bEnable);
}

void FAuthDialogs::EnableLoginMethodButtons(bool bEnable)
{
	if (LoginIDPasswordDialog)
	{
		EnableButton(LoginIDPasswordDialog->GetWidget(6), bEnable);
		EnableButton(LoginIDPasswordDialog->GetWidget(7), bEnable);
		EnableButton(LoginIDPasswordDialog->GetWidget(8), bEnable);
		EnableButton(LoginIDPasswordDialog->GetWidget(9), bEnable);
		EnableButton(LoginIDPasswordDialog->GetWidget(10), bEnable);
	}

	if (LoginExchangeCodeDialog)
	{
		EnableButton(LoginExchangeCodeDialog->GetWidget(6), bEnable);
		EnableButton(LoginExchangeCodeDialog->GetWidget(7), bEnable);
		EnableButton(LoginExchangeCodeDialog->GetWidget(8), bEnable);
		EnableButton(LoginExchangeCodeDialog->GetWidget(9), bEnable);
		EnableButton(LoginExchangeCodeDialog->GetWidget(10), bEnable);
	}

	if (LoginDevAuthDialog)
	{
		EnableButton(LoginDevAuthDialog->GetWidget(6), bEnable);
		EnableButton(LoginDevAuthDialog->GetWidget(7), bEnable);
		EnableButton(LoginDevAuthDialog->GetWidget(8), bEnable);
		EnableButton(LoginDevAuthDialog->GetWidget(9), bEnable);
		EnableButton(LoginDevAuthDialog->GetWidget(10), bEnable);
	}

	if (LoginAccountPortalDialog)
	{
		EnableButton(LoginAccountPortalDialog->GetWidget(6), bEnable);
		EnableButton(LoginAccountPortalDialog->GetWidget(7), bEnable);
		EnableButton(LoginAccountPortalDialog->GetWidget(8), bEnable);
		EnableButton(LoginAccountPortalDialog->GetWidget(9), bEnable);
		EnableButton(LoginAccountPortalDialog->GetWidget(10), bEnable);
	}
}

void FAuthDialogs::EnableButton(WidgetPtr AuthButton, bool bEnable)
{
	if (AuthButton)
	{
		if (bEnable)
		{
			AuthButton->Enable();
		}
		else
		{
			AuthButton->Disable();
		}
	}
}

void FAuthDialogs::OnGameEvent(const FGameEvent& Event)
{
	if (Event.GetType() == EGameEventType::StartUserLogin)
	{
		EnableLoginButtons(false);
		EnableLoginMethodButtons(false);
	}
	else if (Event.GetType() == EGameEventType::UserLoggedIn)
	{
		HideLoginDialogs();

		ShowDialog(UserLoggedInDialog);
		UpdateUserLoggedIn();

		bHasLoggedIn = true;
	}
	else if (Event.GetType() == EGameEventType::UserLoginFailed)
	{
		if (!FPlayerManager::Get().GetCurrentUser().IsValid())
		{
			DoChangeLoginMode(SavedLoginMode);
		}
	}
	else if (Event.GetType() == EGameEventType::UserLoginRequiresMFA)
	{
		HideLoginDialogs();

		if (FPlayerManager::Get().GetCurrentUser().IsValid())
		{
			HideDialog(UserLoggedInDialog);
		}

		ShowDialog(MFALoginDialog);
	}
	else if (Event.GetType() == EGameEventType::UserLoginEnteredMFA)
	{
		HideDialog(MFALoginDialog);

		if (FPlayerManager::Get().GetCurrentUser().IsValid())
		{
			ShowDialog(UserLoggedInDialog);
		}
	}
	else if (Event.GetType() == EGameEventType::StartUserLogout)
	{
		if (FPlayerManager::Get().GetNumPlayers() == 0)
		{
			HideDialog(UserLoggedInDialog);
		}
	}
	else if (Event.GetType() == EGameEventType::UserLoggedOut)
	{
		if (FPlayerManager::Get().GetNumPlayers() > 0)
		{
			UpdateUserInfo();
			UpdateUserLoggedIn();
		}
		else
		{
			HideDialog(UserLoggedInDialog);
			DoChangeLoginMode(SavedLoginMode);
			ResetUserInfo();
		}
	}
	else if (Event.GetType() == EGameEventType::UserInfoRetrieved)
	{
		UpdateUserInfo();
	}
	else if (Event.GetType() == EGameEventType::ShowPrevUser)
	{
		UpdateCurrentPlayer();
	}
	else if (Event.GetType() == EGameEventType::ShowNextUser)
	{
		UpdateCurrentPlayer();
	}
	else if (Event.GetType() == EGameEventType::NewUserLogin)
	{
		NewLogin();
	}
	else if (Event.GetType() == EGameEventType::CancelLogin)
	{
		HideLoginDialogs();

		ShowDialog(UserLoggedInDialog);
		UpdateUserLoggedIn();

		UpdateCurrentPlayer();
	}
	else if (Event.GetType() == EGameEventType::UserLogOutTriggered)
	{
		bLoggingOut = true;
		LogoutTriggeredTimestamp = Main->GetTimer().GetTotalSeconds();
	}
	else if (Event.GetType() == EGameEventType::NoUserLoggedIn)
	{
		HideDialog(UserLoggedInDialog);
		DoChangeLoginMode(SavedLoginMode);
		UpdateLayout();
	}
}

void FAuthDialogs::HideLoginDialogs()
{
	HideDialog(LoginIDPasswordDialog);
	HideDialog(LoginExchangeCodeDialog);
	HideDialog(LoginDevAuthDialog);
	HideDialog(LoginAccountPortalDialog);
}

void FAuthDialogs::UpdateCurrentPlayer()
{
	HideLoginDialogs();

	ShowDialog(UserLoggedInDialog);

	UpdateUserLoggedIn();
	UpdateUserInfo();
}

void FAuthDialogs::NewLogin()
{
	HideDialog(UserLoggedInDialog);
	DoChangeLoginMode(ELoginMode::IDPassword);
	UpdateUserInfo();
}

void FAuthDialogs::ResetUserInfo()
{
	UserLoggedInLabel->SetText(L"");
}

void FAuthDialogs::SetUserInfoPending()
{
	std::wstring PendingText = L"Pending...";
	UserLoggedInLabel->SetText(PendingText);
}

void FAuthDialogs::UpdateUserInfo()
{
	ResetUserInfo();

	if (FPlayerManager::Get().GetCurrentUser().IsValid())
	{
		std::wstring DisplayName = FPlayerManager::Get().GetDisplayName(FPlayerManager::Get().GetCurrentUser());
		UserLoggedInLabel->SetText(DisplayName);
	}
}

void FAuthDialogs::SetTestUserInfo()
{
	std::wstring TestingText = L"User_123";
	UserLoggedInLabel->SetText(TestingText);
}

void FAuthDialogs::SetSingleUserOnly(bool bValue)
{
	bIsSingleUserOnly = bValue;

	if (bIsSingleUserOnly && UserLoggedInDialog)
	{
		WidgetPtr NewUserButton = UserLoggedInDialog->GetWidget(2);
		WidgetPtr PrevUserButton = UserLoggedInDialog->GetWidget(3);
		WidgetPtr NextUserButton = UserLoggedInDialog->GetWidget(4);

		if (NewUserButton) NewUserButton->Hide();
		if (PrevUserButton) PrevUserButton->Hide();
		if (NextUserButton) NextUserButton->Hide();
	}
}

void FAuthDialogs::ShowDialog(DialogPtr Dialog)
{
	ShowDialogTime = static_cast<float>(Main->GetTimer().GetTotalSeconds());

	if (Dialog)
	{
		Dialog->Show();
	}
}

void FAuthDialogs::HideDialog(DialogPtr Dialog)
{
	if (Dialog)
	{
		Dialog->Hide();
	}
}

void FAuthDialogs::AddDialog(DialogPtr Dialog)
{
	Dialogs.push_back(Dialog);
}

bool FAuthDialogs::IsDialogReadyForInput()
{
	float CurTime = static_cast<float>(Main->GetTimer().GetTotalSeconds());
	if (CurTime - ShowDialogTime > 0.5f)
	{
		return true;
	}
	return false;
}