// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "DebugLog.h"
#include "VectorRender.h"
#include "Input.h"
#include "Console.h"
#include "BaseMenu.h"
#include "TextureManager.h"
#include "BaseLevel.h"
#include "Authentication.h"
#include "AccountHelpers.h"
#include "Users.h"
#include "Friends.h"
#include "EosUI.h"
#include "Metrics.h"
#include "GameEvent.h"
#include "Player.h"
#include "Platform.h"
#include "Main.h"
#include "BaseGame.h"

#ifdef DXTK
#include "SpriteBatch.h"
#endif

#ifdef EOS_STEAM_ENABLED
#include "Steam/SteamManager.h"
#endif

/**
 * Singleton Implementation
 */
class FBaseGame::Impl
{
public:
	Impl(FBaseGame* Owner) :
		TheOwner(Owner)
	{
		if (Instance)
		{
			throw std::runtime_error("FBaseGame is a singleton");
		}

		Instance = (FBaseGame::Impl *)this;

		Init();
	}

	~Impl()
	{
		Instance = nullptr;
	}

	void Init()
	{
		HelpMessage =
		{
			L"Console commands available:",
			L" TEST - to print test message;",
			L" CLEAR - to clear the console;",
			L" EXIT - to stop the application;",
			L" FRIENDS - to open the friends overlay;",
			L" SETLOCALE - set the override locale code;",
			L" FPS - to toggle showing fps on-screen;",
			L" LOGIN USER_ID PASSWORD - to try to login;",
			L" LOGOUT USER_NAME - to log out a user;",
			L" INVITE FRIEND_NAME - to send friend invite using friend's display name;",
			L" PERSISTLOGIN - to try to login using locally stored persistent auth credentials;",
			L" PERSISTDELETE - to delete any locally stored persistent auth credentials;",
			L" PREV - to show previous user's info;",
			L" NEXT - to show next user's info;",
			L" NEW - to log in with a new user;",
			L" HELP - to print this help message."
		};
	}

	void AppendHelpMessageLines(const std::vector<const wchar_t*>& MoreLines)
	{
		HelpMessage.reserve(HelpMessage.size() + MoreLines.size());
		for (const wchar_t* NextLine : MoreLines)
		{
			if (NextLine)
			{
				HelpMessage.push_back(NextLine);
			}
		}
	}

	FBaseGame* TheOwner;

	/** Rendering batch for all UI */
	FSpriteBatchPtr UISpriteBatch;

	/** Console help message */
	std::vector<const wchar_t*> HelpMessage;

	static FBaseGame::Impl* Instance;
};

FBaseGame::Impl* FBaseGame::Impl::Instance = nullptr;

FBaseGame::FBaseGame() noexcept(false):
	TheImpl(std::make_unique<FBaseGame::Impl>(this))
{
	Input = std::make_unique<FInput>();
	Console = std::make_shared<FConsole>();
	Users = std::make_unique<FUsers>();
	TextureManager = std::make_unique<FTextureManager>();
	Authentication = std::make_shared<FAuthentication>();
	Friends = std::make_unique<FFriends>();
	Metrics = std::make_unique<FMetrics>();
	PlayerManager = std::make_unique<FPlayerManager>();
	VectorRender = std::make_unique<FVectorRender>();
	EosUI = std::make_unique<FEosUI>();
}

FBaseGame::~FBaseGame()
{
}

FBaseGame& FBaseGame::GetBase()
{
	if (!Impl::Instance || !Impl::Instance->TheOwner)
	{
		throw std::runtime_error("FBaseGame singleton not created");
	}

	return *Impl::Instance->TheOwner;
}

void FBaseGame::Init()
{
	Metrics->Init();
	Menu->Init();
	EosUI->Init();

	FGameEvent Event(EGameEventType::CheckAutoLogin);
	OnGameEvent(Event);
}

void FBaseGame::CreateConsoleCommands()
{
	if (Console)
	{
		// Console commands tests
		Console->AddCommand(L"TEST", [](const std::vector<std::wstring>&)
		{
			FBaseGame::GetBase().GetConsole()->AddLine(L"Test command executed.");
		});

		Console->AddCommand(L"CLEAR", [](const std::vector<std::wstring>&)
		{
			FBaseGame::GetBase().GetConsole()->Clear();
		});

		Console->AddCommand(L"EXIT", [](const std::vector<std::wstring>&)
		{
			FGameEvent Event(EGameEventType::ExitGame);
			FBaseGame::GetBase().OnGameEvent(Event);
		});

		Console->AddCommand(L"LOGIN", [](const std::vector<std::wstring>& args)
		{
			if (FPlatform::IsInitialized())
			{
				if (args.size() == 2)
				{
#ifdef DEV_BUILD
					FGameEvent Event(EGameEventType::StartUserLogin, (int)ELoginMode::IDPassword, args[0], args[1]);
#else
					FGameEvent Event(EGameEventType::StartUserLogin, (int)ELoginMode::DevAuth, args[0], args[1]);
#endif
					FBaseGame::GetBase().OnGameEvent(Event);
				}
				else
				{
#ifdef EOS_STEAM_ENABLED
					FSteamManager::GetInstance().StartLogin();
#else
#ifdef DEV_BUILD
					FBaseGame::GetBase().GetConsole()->AddLine(L"Auth error: name and pwd required.");
#else
					FBaseGame::GetBase().GetConsole()->AddLine(L"Auth error: host and credentials required.");
#endif
#endif //EOS_STEAM_ENABLED
				}
			}
			else
			{
				FDebugLog::LogError(L"EOS SDK is not initialized!");
			}
		});

		Console->AddCommand(L"PERSISTLOGIN", [](const std::vector<std::wstring>&)
		{
			FGameEvent Event(EGameEventType::StartUserLogin, (int)ELoginMode::PersistentAuth);
			FBaseGame::GetBase().OnGameEvent(Event);
		});

		Console->AddCommand(L"PERSISTDELETE", [](const std::vector<std::wstring>&)
		{
			FGameEvent Event(EGameEventType::DeletePersistentAuth);
			FBaseGame::GetBase().OnGameEvent(Event);
		});

		Console->AddCommand(L"LOGOUT", [](const std::vector<std::wstring>& args)
		{
			if (FPlatform::IsInitialized())
			{
				if (args.size() == 1)
				{
					PlayerPtr Player = FPlayerManager::Get().GetPlayer(args[0]);
					if (Player)
					{
						FGameEvent Event(EGameEventType::StartUserLogout, Player->GetUserID());
						FBaseGame::GetBase().OnGameEvent(Event);
					}
					else
					{
						FDebugLog::LogError(L"Can't find player!");
					}
				}
				else
				{
					FBaseGame::GetBase().GetConsole()->AddLine(L"Error: name required.");
				}
			}
			else
			{
				FDebugLog::LogError(L"EOS SDK is not initialized!");
			}
		});

		Console->AddCommand(L"CONTINUELOGIN", [](const std::vector<std::wstring>&)
		{
			FGameEvent Event(EGameEventType::ContinueLogin);
			FBaseGame::GetBase().OnGameEvent(Event);
		});

		Console->AddCommand(L"SETLOCALE", [](const std::vector<std::wstring>& args)
		{
			FBaseGame::GetBase().OnGameEvent( (args.size() > 0)
				? FGameEvent(EGameEventType::SetLocale, args[0])
				: FGameEvent(EGameEventType::SetLocale)
			);
		});

		Console->AddCommand(L"FRIENDS", [](const std::vector<std::wstring>& args)
		{
			if (FPlatform::IsInitialized())
			{
				if (FBaseGame::GetBase().GetEosUI())
				{
					FBaseGame::GetBase().GetEosUI()->ShowFriendsOverlay();
				}
				else
				{
					FDebugLog::LogError(L"EOS SDK UI is not initialized!");
				}
			}
			else
			{
				FDebugLog::LogError(L"EOS SDK is not initialized!");
			}
		});

		Console->AddCommand(L"INVITE", [](const std::vector<std::wstring>& args)
		{
			if (FPlatform::IsInitialized())
			{
				if (args.size() == 1)
				{
					if (FBaseGame::GetBase().GetFriends())
					{
						FBaseGame::GetBase().GetFriends()->QueryUserInfo(args[0]);
						FBaseGame::GetBase().GetFriends()->QueueFoundFriendAction(FriendAction::Invite);
					}
				}
				else
				{
					FBaseGame::GetBase().GetConsole()->AddLine(L"Invite error: name of user to invite is required.");
				}
			}
			else
			{
				FDebugLog::LogError(L"EOS SDK is not initialized!");
			}
		});

		Console->AddCommand(L"PREV", [](const std::vector<std::wstring>&)
		{
			FGameEvent Event(EGameEventType::ShowPrevUser);
			FBaseGame::GetBase().OnGameEvent(Event);
		});

		Console->AddCommand(L"NEXT", [](const std::vector<std::wstring>&)
		{
			FGameEvent Event(EGameEventType::ShowNextUser);
			FBaseGame::GetBase().OnGameEvent(Event);
		});

		Console->AddCommand(L"NEW", [](const std::vector<std::wstring>&)
		{
			FGameEvent Event(EGameEventType::NewUserLogin);
			FBaseGame::GetBase().OnGameEvent(Event);
		});

		Console->AddCommand(L"FPS", [](const std::vector<std::wstring>&)
		{
			FGameEvent Event(EGameEventType::ToggleFPS);
			FBaseGame::GetBase().OnGameEvent(Event);
		});

		Console->AddCommand(L"TESTSETPRESENCE", [](const std::vector<std::wstring>& args)
		{
			if (args.size() == 0)
			{
				FGameEvent Event(EGameEventType::TestSetPresence, L"Test");
				FBaseGame::GetBase().OnGameEvent(Event);
			}
			else
			{
				for (int i = 0; i < (int)args.size(); ++i)
				{
					FGameEvent Event(EGameEventType::TestSetPresence, args[i]);
					FBaseGame::GetBase().OnGameEvent(Event);
				}
			}
		});

		Console->AddCommand(L"PRINTAUTH", [](const std::vector<std::wstring>& args)
		{
			FGameEvent Event(EGameEventType::PrintAuth);
			FBaseGame::GetBase().OnGameEvent(Event);
		});

#if defined(_DEBUG) || defined(_TEST)
		Console->AddCommand(L"TESTFRIENDS", [](const std::vector<std::wstring>& args)
		{
			if (FBaseGame::GetBase().GetFriends())
			{
				std::vector<FFriendData> TestFriends(9);
				TestFriends[0].Name = L"Bob";
				TestFriends[0].Status = EOS_EFriendsStatus::EOS_FS_Friends;
				TestFriends[0].Presence.Status = EOS_Presence_EStatus::EOS_PS_Online;
				TestFriends[0].Presence.Application = L"Fortnite";
				TestFriends[0].Presence.Platform = L"Windows";
				TestFriends[0].Presence.RichText = L"Main menu";

				TestFriends[1].Name = L"Tom";
				TestFriends[1].Status = EOS_EFriendsStatus::EOS_FS_Friends;
				TestFriends[1].Presence.Status = EOS_Presence_EStatus::EOS_PS_Away;

				TestFriends[2].Name = L"Richard";
				TestFriends[2].Status = EOS_EFriendsStatus::EOS_FS_Friends;
				TestFriends[2].Presence.Status = EOS_Presence_EStatus::EOS_PS_Offline;

				TestFriends[3].Name = L"John1337";
				TestFriends[3].Status = EOS_EFriendsStatus::EOS_FS_InviteReceived;

				TestFriends[4].Name = L"Jessica";
				TestFriends[4].Status = EOS_EFriendsStatus::EOS_FS_InviteSent;

				TestFriends[5].Name = L"Mary";
				TestFriends[5].Status = EOS_EFriendsStatus::EOS_FS_InviteSent;

				TestFriends[6].Name = L"Alex";
				TestFriends[6].Status = EOS_EFriendsStatus::EOS_FS_Friends;
				TestFriends[6].Presence.Status = EOS_Presence_EStatus::EOS_PS_ExtendedAway;

				TestFriends[7].Name = L"Neil";
				TestFriends[7].Status = EOS_EFriendsStatus::EOS_FS_Friends;
				TestFriends[7].Presence.Status = EOS_Presence_EStatus::EOS_PS_DoNotDisturb;

				TestFriends[8].Name = L"Alice";
				TestFriends[8].Status = EOS_EFriendsStatus::EOS_FS_NotFriends;

				FBaseGame::GetBase().GetFriends()->SetFriends(std::move(TestFriends));
				FBaseGame::GetBase().GetMenu()->SetTestUserInfo();
			}
		});

		Console->AddCommand(L"DEBUGRENDER", [](const std::vector<std::wstring>&)
		{
			FBaseGame::GetBase().GetVectorRender()->ToggleDebugRender();
		});

#if defined(DEV_BUILD)
		Console->AddCommand(L"PACT", [](const std::vector<std::wstring>& args)
		{
			if (FPlatform::IsInitialized())
			{
				if (FBaseGame::GetBase().GetEosUI())
				{
					FBaseGame::GetBase().GetEosUI()->RunPactTests();
				}
				else
				{
					FDebugLog::LogError(L"EOS SDK UI is not initialized!");
				}
			}
			else
			{
				FDebugLog::LogError(L"EOS SDK is not initialized!");
			}
		});

		Console->AddCommand(L"TESTOVERLAY", [](const std::vector<std::wstring>& args)
		{
			if (FPlatform::IsInitialized())
			{
				if (FBaseGame::GetBase().GetEosUI())
				{
					if (args.size() >= 1)
					{
						FBaseGame::GetBase().GetEosUI()->LoadURLCustom(FStringUtils::Narrow(args[0]).c_str());
					}
					else
					{
						FDebugLog::LogError(L"TESTOVERLAY requires a URL as an argument.");
					}
				}
				else
				{
					FDebugLog::LogError(L"EOS SDK UI is not initialized!");
				}
			}
			else
			{
				FDebugLog::LogError(L"EOS SDK is not initialized!");
			}
		});
#endif
#endif

		Console->AddCommand(L"CHANGEKEY", [](const std::vector<std::wstring>& args)
		{
			if (FBaseGame::GetBase().GetEosUI())
			{
				if (args.size() >= 1)
				{
					std::wstring ArgList;
					for (const std::wstring& arg : args)
					{
						if (ArgList.size() > 0)
						{
							ArgList += L"+";
						}
						ArgList += arg;
					}
					FDebugLog::LogWarning(L"Updating key binding to %ls", ArgList.c_str());
					FBaseGame::GetBase().GetEosUI()->SetToggleFriendsKey(args);
				}
				else
				{
					FDebugLog::LogWarning(L"Updating key binding back to the system default");
					FBaseGame::GetBase().GetEosUI()->SetToggleFriendsKey(std::vector<std::wstring>{ });
				}
			}
		});

		Console->AddCommand(L"SETDISPLAY", [](const std::vector<std::wstring>& args)
		{
			FDebugLog::LogWarning(L"Updating display preference");
			if (FBaseGame::GetBase().GetEosUI())
			{
				EOS_UI_ENotificationLocation Location = EOS_UI_ENotificationLocation::EOS_UNL_BottomRight;
				if (args.size() >= 1)
				{
					std::wstring LocationStr = args[0];
					std::transform(LocationStr.begin(), LocationStr.end(), LocationStr.begin(), tolower);
					if (LocationStr == L"tl")
					{
						Location = EOS_UI_ENotificationLocation::EOS_UNL_TopLeft;
					}
					else if (LocationStr == L"tr")
					{
						Location = EOS_UI_ENotificationLocation::EOS_UNL_TopRight;
					}
					else if (LocationStr == L"bl")
					{
						Location = EOS_UI_ENotificationLocation::EOS_UNL_BottomLeft;
					}
					else if (LocationStr == L"br")
					{
						Location = EOS_UI_ENotificationLocation::EOS_UNL_BottomRight;
					}
				}
				FBaseGame::GetBase().GetEosUI()->SetDisplayPreference(Location);
			}
			else
			{
				FDebugLog::LogError(L"EOS SDK UI is not initialized!");
			}
		});

		Console->AddCommand(L"HELP", [](const std::vector<std::wstring>&)
		{
			if (Impl::Instance)
			{
				for (const auto& NextLine : Impl::Instance->HelpMessage)
				{
					FBaseGame::GetBase().GetConsole()->AddLine(NextLine);
				}
			}
		});

	}
}


void FBaseGame::AppendHelpMessageLines(const std::vector<const wchar_t*>& MoreLines)
{
	if (TheImpl)
	{
		TheImpl->AppendHelpMessageLines(MoreLines);
	}
}

void FBaseGame::Exit()
{
#ifdef DXTK
	if (Main && Main->GetDeviceResources())
	{
		PostMessage(Main->GetDeviceResources()->GetWindow(), WM_CLOSE, 0, 0);
	}
#endif

#ifdef EOS_DEMO_SDL
	SDL_Event QuitEvent;
	QuitEvent.type = SDL_QUIT;
	SDL_PushEvent(&QuitEvent);
#endif
}

void FBaseGame::Update()
{
	Input->Update();

	if (Input->IsKeyPressed(FInput::InputCommands::Exit) ||
		Input->IsGamePadButtonPressed(FInput::InputCommands::Exit))
	{
		FGameEvent Event(EGameEventType::ExitGame);
		OnGameEvent(Event);
	}

	Users->Update();
	Menu->Update();
	Level->Update();
	Friends->Update();

#ifdef EOS_STEAM_ENABLED
	FSteamManager::GetInstance().Update();
#endif

	FPlatform::Update();
}

void FBaseGame::Render()
{
	TheImpl->UISpriteBatch->Begin();
	Menu->Render(TheImpl->UISpriteBatch);
	TheImpl->UISpriteBatch->End();

	VectorRender->Begin();

#ifdef _DEBUG
	if (VectorRender->IsDebugRenderEnabled())
	{
		Menu->DebugRender();
		Level->DebugRender();
	}
#endif // _DEBUG

	VectorRender->DrawQueue(); //queue gets populated from normal Render calls
	VectorRender->End();

	FSpriteBatchPtr EmptyBatchPtr;
	Level->Render(EmptyBatchPtr);
}

void FBaseGame::Create()
{
#ifdef DXTK
	std::unique_ptr<DeviceResources> const& DeviceResources = Main->GetDeviceResources();
	ID3D11DeviceContext* Context = DeviceResources->GetD3DDeviceContext();

	TheImpl->UISpriteBatch = std::make_unique<DirectX::SpriteBatch>(Context);
#endif // DXTK

#ifdef EOS_DEMO_SDL
	TheImpl->UISpriteBatch = std::make_unique<FSDLSpriteBatch>();
#endif

	Menu->CreateFonts();
	Menu->Create();
	Level->Create();
	VectorRender->Create();
}

void FBaseGame::OnGameEvent(const FGameEvent& Event)
{
	PlayerManager->OnGameEvent(Event);
	Friends->OnGameEvent(Event);
	Menu->OnGameEvent(Event);
	Authentication->OnGameEvent(Event);
	Metrics->OnGameEvent(Event);
	Level->OnGameEvent(Event);
	EosUI->OnGameEvent(Event);

#ifdef EOS_STEAM_ENABLED
	FSteamManager::GetInstance().OnGameEvent(Event);
#endif
}


void FBaseGame::OnShutdown()
{
	// Must explicitly call FEosUI::OnShutdown() before the end of destruction to allow FBaseGame::GetBase() to not throw.
	if (EosUI)
	{
		EosUI->OnShutdown();
	}

	// Must explicitly call FAuthentication::Shutdown() before the end of destruction to allow FBaseGame::GetBase() to not throw.
	if (Authentication)
	{
		Authentication->Shutdown();
	}

	Release();
}

bool FBaseGame::IsShutdownDelayed()
{
	return false;
}

void FBaseGame::Release()
{
	if (Menu)
	{
		Menu->Release();
	}

	if (Level)
	{
		Level->Release();
	}

	if (VectorRender)
	{
		VectorRender->Release();
	}
}

void FBaseGame::UpdateLayout(int Width, int Height)
{
	if (VectorRender)
	{
		//recreate vector render
		bool bIsEnabled = VectorRender->IsDebugRenderEnabled();
		VectorRender = std::make_unique<FVectorRender>();
		VectorRender->Create();
		VectorRender->SetDebugRenderEnabled(bIsEnabled);
	}

	if (Menu)
	{
		Menu->UpdateLayout(Width, Height);
	}
}

std::unique_ptr<FInput> const& FBaseGame::GetInput()
{
	return Input;
}

std::shared_ptr<FConsole> const& FBaseGame::GetConsole()
{
	return Console;
}

std::shared_ptr<FBaseMenu> const& FBaseGame::GetMenu()
{
	return Menu;
}

const std::unique_ptr<FTextureManager>& FBaseGame::GetTextureManager()
{
	return TextureManager;
}

std::shared_ptr<FAuthentication> const& FBaseGame::GetAuthentication()
{
	return Authentication;
}

std::unique_ptr<FFriends> const& FBaseGame::GetFriends()
{
	return Friends;
}

const std::unique_ptr<FEosUI>& FBaseGame::GetEosUI()
{
	return EosUI;
}

std::unique_ptr<FUsers> const& FBaseGame::GetUsers()
{
	return Users;
}

std::unique_ptr<FVectorRender> const& FBaseGame::GetVectorRender()
{
	return VectorRender;
}

void FBaseGame::PrintToConsole(const std::wstring& Msg)
{
	if (Console)
	{
		Console->AddLine(Msg);
	}
}

void FBaseGame::PrintWarningToConsole(const std::wstring& Msg)
{
	if (Console)
	{
		static FColor col = FColor(1.f, 0.6f, 0.f, 1.f);
		Console->AddLine(Msg, col);
	}
}

void FBaseGame::PrintErrorToConsole(const std::wstring& Msg)
{
	if (Console)
	{
		static FColor col = FColor(1.f, 0.f, 0.f, 1.f);
		Console->AddLine(Msg, col);
	}
}
