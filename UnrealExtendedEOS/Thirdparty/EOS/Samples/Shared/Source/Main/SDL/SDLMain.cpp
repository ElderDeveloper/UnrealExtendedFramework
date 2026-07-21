// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"

#ifdef EOS_DEMO_SDL

#include "DebugLog.h"
#include "StringUtils.h"
#include "Main.h"
#include "SDLInput.h"
#include "Game.h"
#include "Menu.h"
#include "UIEvent.h"
#include "SampleConstants.h"
#include "CommandLine.h"
#include "Settings.h"

// Indicates to hybrid graphics systems to prefer the discrete part by default

#ifdef _WIN32
extern "C"
{
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif //_WIN32

void UpdateWindowSize(int Width, int Height)
{
	if (Main)
	{
		Main->OnWindowSizeChanged(Width, Height);

		// Resize OpenGL Viewport
		glViewport(0, 0, Width, Height);
		Main->GLError(L"Viewport");
	}
}

bool InitGraphics(SDL_Window *Window, int Width, int Height)
{
	bool bSuccess = true;

	glClearColor(Color::LightSkyBlue.R, Color::LightSkyBlue.G, Color::LightSkyBlue.B, 1.0f);
	if (Main->GLError(L"Clear Color")) bSuccess = false;

	glClearDepth(1.0f);
	if (Main->GLError(L"Clear Depth")) bSuccess = false;

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if (Main->GLError(L"Blend Func")) bSuccess = false;

	glEnable(GL_DEPTH_TEST);
	if (Main->GLError(L"Enable Depth Test")) bSuccess = false;

	glDepthFunc(GL_LEQUAL);
	if (Main->GLError(L"Depth Func Equal")) bSuccess = false;

	return bSuccess;
}

void Shutdown()
{
	//Destroy context
	if (Main->GetGLContext())
	{
		SDL_GL_DeleteContext(Main->GetGLContext());
	}

	//Destroy window	
	SDL_DestroyWindow(Main->GetWindow());

	//Quit SDL subsystems
	SDL_Quit();
}

bool Init()
{
	//Initialize SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) < 0)
	{
		FDebugLog::LogError(L"SDL could not be initialized! SDL Error: %ls\n", FStringUtils::Widen(SDL_GetError()).c_str());
		return false;
	}

	static constexpr int OpenGLVersionMajor = 3;
	static constexpr int OpenGLVersionMinor = 1;

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, OpenGLVersionMajor);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, OpenGLVersionMinor);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	int Width, Height;
	Main->GetDefaultSize(Width, Height);

	Main->InitCommandLine();

	SDL_Window* Window = nullptr;

	//Create window
	if (Main->bIsFullScreen)
	{
		Window = SDL_CreateWindow(SampleConstants::GameName, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 0, 0, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN_DESKTOP);
	}
	else
	{
		Window = SDL_CreateWindow(SampleConstants::GameName, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, Width, Height, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	}

	if (Window == NULL)
	{
		FDebugLog::LogError(L"Window could not be created! SDL Error: %ls\n", FStringUtils::Widen(SDL_GetError()).c_str());
		return false;
	}

	//Create context
	SDL_GLContext GLContext = SDL_GL_CreateContext(Window);
	if (GLContext == NULL)
	{
		FDebugLog::LogError(L"OpenGL context could not be created! SDL Error: %ls\n", FStringUtils::Widen(SDL_GetError()).c_str());
		return false;
	}

	int Major, Minor;
	SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &Major);
	SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &Minor);
	FDebugLog::Log(L"OpenGL Context Chosen: %d.%d, Wanted: %d.%d", Major, Minor, OpenGLVersionMajor, OpenGLVersionMinor);

	if (Major != OpenGLVersionMajor)
	{
		FDebugLog::LogError(L"OpenGL Major Version Chosen: %d, Wanted: %d", Major, OpenGLVersionMajor);
		return false;
	}

	if (Minor != OpenGLVersionMinor)
	{
		FDebugLog::LogError(L"OpenGL Major Version Chosen: %d, Wanted: %d", Major, OpenGLVersionMinor);
		return false;
	}

	//Initialize GLEW
	glewExperimental = GL_TRUE;
	GLenum glewError = glewInit();
	if (glewError != GLEW_OK)
	{
		FDebugLog::LogError(L"Error initializing GLEW! %ls\n", FStringUtils::Widen(reinterpret_cast<const char*>(glewGetErrorString(glewError))).c_str());
		return false;
	}

	//Make sure we have required functions from OpenGL extensions.
	//Unfortunately we have to make this paranoid check because SDL seems to fail to detect correct OpenGL version on Windows sometimes.
	if (glCreateProgram == NULL || glCreateShader == NULL || !GLEW_VERSION_3_1)
	{
		FDebugLog::LogError(L"Error initializing OpenGL extensions! Make sure you have hardware that supports OpenGL 3.1 or later and you've got OpenGL-capable graphics drivers installed! Sometimes you can also get this error when running through remote desktop (try updating drivers/remote desktop software).");
		return false;
	}

	//Vsync settings
	//First try to use adaptive VSync
	if (SDL_GL_SetSwapInterval(-1) < 0)
	{
		FDebugLog::LogWarning(L"Warning: Unable to set adaptive VSync! SDL Error: %ls\n", FStringUtils::Widen(SDL_GetError()).c_str());

		//Try to set Vsync (on)
		if (SDL_GL_SetSwapInterval(1) < 0)
		{
			FDebugLog::LogWarning(L"Warning: Unable to set VSync! SDL Error: %ls\n", FStringUtils::Widen(SDL_GetError()).c_str());

			//Disable Vsync (off)
			if (SDL_GL_SetSwapInterval(0) < 0)
			{
				FDebugLog::LogWarning(L"Error: Unable to set VSync to Off! SDL Error: %ls\n", FStringUtils::Widen(SDL_GetError()).c_str());
			}
		}
	}


	//init True Type Fonts lib
	if (TTF_Init() != 0)
	{
		FDebugLog::LogError(L"Unable to initialize True Type Fonts. TTF Error: %ls\n", FStringUtils::Widen(TTF_GetError()).c_str());
		return false;
	}

	if (!Main->bIsFullScreen)
	{
		int MinWidth = 1024;
		int MinHeight = 768;
		Main->GetMinimumSize(MinWidth, MinHeight);
		SDL_SetWindowMinimumSize(Window, MinWidth, MinHeight);
	}

	SDL_GetWindowSize(Window, &Width, &Height);

	//Initialize OpenGL
	if (!InitGraphics(Window, Width, Height))
	{
		FDebugLog::LogError(L"Unable to initialize OpenGL!\n");
		return false;
	}

	Main->Initialize(Window, GLContext, Width, Height);

	return true;
}

void ProcessWindowEvent(SDL_WindowEvent *WinEvent)
{
	switch (WinEvent->event)
	{
		case SDL_WINDOWEVENT_RESIZED:
		case SDL_WINDOWEVENT_SIZE_CHANGED:
		{
			UpdateWindowSize(WinEvent->data1, WinEvent->data2);
			break;
		}
		case SDL_WINDOWEVENT_CLOSE:
		{
			SDL_Event QuitEvent;
			QuitEvent.type = SDL_QUIT;
			SDL_PushEvent(&QuitEvent);
			break;
		}
	}
}

bool ProcessSDLEvents(SDL_Event *Event)
{
	FUIEvent InputEvent = FInput::ProcessSDLEvent(*Event);
	if (InputEvent.GetType() != EUIEventType::None)
	{
		FGame::Get().GetMenu()->OnUIEvent(InputEvent);
	}
	else
	{
		switch (Event->type)
		{
			case SDL_QUIT:
			{
				return true;
			}
			case SDL_WINDOWEVENT:
			{
				ProcessWindowEvent(&Event->window);
				break;
			}
			default:
				break;
		}
	}

	return false;
}

int main(int Argc, char* Args[])
{
	// Ignore first param (executable path)
	std::vector<std::wstring> CmdLineParams;
	for (int i = 1; i < Argc; ++i)
	{
		CmdLineParams.push_back(FStringUtils::Widen(Args[i]));
	}
	FCommandLine::Get().Init(CmdLineParams);

	FSettings::Get().Init();

	Main = std::make_unique<FMain>();

	Main->InitPlatform();

	//Start up SDL and create window
	if (!Init())
	{
		printf("Failed to initialize!\n");
	}
	else
	{
		//Main loop flag
		bool bQuit = false;

		//Event handler
		SDL_Event Event;

		//Enable text input
		SDL_StartTextInput();

		//While application is running
		while (!bQuit)
		{
			//Handle events on queue
			while (SDL_PollEvent(&Event) != 0)
			{
				bQuit = ProcessSDLEvents(&Event);
			}

			Main->Tick();

			//Update screen
			SDL_GL_SwapWindow(Main->GetWindow());
		}

		//Disable text input
		SDL_StopTextInput();

		//Check if we need to delay shutting down
		Main->OnShutdown();
		while (Main->IsShutdownDelayed())
		{
			//We only update & render (no inputs, no events, etc)
			Main->Tick();

			//Update screen
			SDL_GL_SwapWindow(Main->GetWindow());
		}
	}

	//Free resources and close SDL
	Shutdown();

	Main.reset();

#if defined (_DEBUG) && defined (DUMP_MEM_LEAKS)
	_CrtDumpMemoryLeaks();
#endif // _DEBUG

	return 0;
}

#endif //EOS_DEMO_SDL
