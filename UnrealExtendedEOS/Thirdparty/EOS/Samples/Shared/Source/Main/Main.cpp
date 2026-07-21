// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "Game.h"
#include "DebugLog.h"
#include "CommandLine.h"
#include "StringUtils.h"
#include "Main.h"
#include "Platform.h"
#include "SampleConstants.h"

#include "eos_sdk.h"
#include "eos_logging.h"

#ifdef EOS_DEMO_SDL
#include "SDLSpriteBatch.h"
#endif

#ifdef DXTK
#include <dxgidebug.h>
#endif

#ifdef EOS_STEAM_ENABLED
#include "Steam/SteamManager.h"
#endif

#if ALLOW_RESERVED_OPTIONS
#include "ReservedInitializeOptions.h"
#endif

/**
* Callback function to use for EOS SDK log messages
*
* @param InMsg - A structure representing data for a log message
*/
void EOS_CALL EOSSDKLoggingCallback(const EOS_LogMessage* InMsg)
{
	if (InMsg->Level != EOS_ELogLevel::EOS_LOG_Off)
	{
		if (InMsg->Level == EOS_ELogLevel::EOS_LOG_Error || InMsg->Level == EOS_ELogLevel::EOS_LOG_Fatal)
		{
			FDebugLog::LogError(L"[EOS SDK] %ls: %ls", FStringUtils::Widen(InMsg->Category).c_str(), FStringUtils::Widen(InMsg->Message).c_str());
		}
		else if (InMsg->Level == EOS_ELogLevel::EOS_LOG_Warning)
		{
			FDebugLog::LogWarning(L"[EOS SDK] %ls: %ls", FStringUtils::Widen(InMsg->Category).c_str(), FStringUtils::Widen(InMsg->Message).c_str());
		}
		else
		{
			FDebugLog::Log(L"[EOS SDK] %ls: %ls", FStringUtils::Widen(InMsg->Category).c_str(), FStringUtils::Widen(InMsg->Message).c_str());
		}
	}
}

constexpr int32_t SampleConstants::MinimumWindowWidth;
constexpr int32_t SampleConstants::MinimumWindowHeight;
constexpr int32_t SampleConstants::DefaultWindowWidth;
constexpr int32_t SampleConstants::DefaultWindowHeight;

std::unique_ptr<FMain> Main;

FMain::FMain() noexcept(false):
	bIsFullScreen(false)
{
	FDebugLog::Init();
	FDebugLog::AddTarget(FDebugLog::ELogTarget::DebugOutput);
	FDebugLog::AddTarget(FDebugLog::ELogTarget::Console);
	FDebugLog::AddTarget(FDebugLog::ELogTarget::File);
}

FMain::~FMain()
{
	FDebugLog::Close();

	Game = nullptr;
#ifdef DXTK
	DXDeviceResources = nullptr;
	HMODULE Mod = LoadLibraryA("dxgi.dll");
	if (Mod)
	{
		using GetDebugFunction = HRESULT(WINAPI *)(UINT Flags, REFIID riid, _COM_Outptr_ void **pDebug);
		GetDebugFunction Function = (GetDebugFunction)GetProcAddress(Mod, "DXGIGetDebugInterface1");
		if (Function)
		{
#define LOCAL_DEFINE_GUID(VariableName, l, w1, w2, ...) static const GUID VariableName = { l, w1, w2, { __VA_ARGS__ }}
			LOCAL_DEFINE_GUID(guid_IDXGIDebug1, 0xc5a05f0c, 0x16f2, 0x4adf, 0x9f, 0x4d, 0xa8, 0xc4, 0xd5, 0x8a, 0xc5, 0x50);
			LOCAL_DEFINE_GUID(guid_DXGI_DEBUG_ALL, 0xe48ae283, 0xda80, 0x490b, 0x87, 0xe6, 0x43, 0xe9, 0xa9, 0xcf, 0xda, 0x8);
			LOCAL_DEFINE_GUID(guid_DXGI_DEBUG_DX, 0x35cdd7fc, 0x13b2, 0x421d, 0xa5, 0xd7, 0x7e, 0x44, 0x51, 0x28, 0x7d, 0x64);
			LOCAL_DEFINE_GUID(guid_DXGI_DEBUG_DXGI, 0x25cddaa4, 0xb1c6, 0x47e1, 0xac, 0x3e, 0x98, 0x87, 0x5b, 0x5a, 0x2e, 0x2a);
			LOCAL_DEFINE_GUID(guid_DXGI_DEBUG_APP, 0x6cd6e01, 0x4219, 0x4ebd, 0x87, 0x9, 0x27, 0xed, 0x23, 0x36, 0xc, 0x62);
#undef LOCAL_DEFINE_GUID

			IDXGIDebug1* Debug = nullptr;
			if (SUCCEEDED((*Function)(0, guid_IDXGIDebug1, (void**)&Debug)))
			{
				HRESULT hr = Debug->ReportLiveObjects(guid_DXGI_DEBUG_ALL, (DXGI_DEBUG_RLO_FLAGS)(DXGI_DEBUG_RLO_IGNORE_INTERNAL | DXGI_DEBUG_RLO_ALL));
				(void)hr;
				Debug->Release();
			}
		}
	}
#endif
}

void FMain::InitPlatform()
{
	FDebugLog::Log(L"[EOS SDK] Initializing ...");

	// Init EOS SDK
	EOS_InitializeOptions SDKOptions = {};
	SDKOptions.ApiVersion = EOS_INITIALIZE_API_LATEST;
	SDKOptions.AllocateMemoryFunction = nullptr;
	SDKOptions.ReallocateMemoryFunction = nullptr;
	SDKOptions.ReleaseMemoryFunction = nullptr;
	SDKOptions.ProductName = SampleConstants::GameName;
	SDKOptions.ProductVersion = "1.0";
#if ALLOW_RESERVED_OPTIONS
	SetReservedInitializeOptions(SDKOptions);
#else
	SDKOptions.Reserved = nullptr;
#endif
	SDKOptions.SystemInitializeOptions = nullptr;
	SDKOptions.OverrideThreadAffinity = nullptr;

	EOS_EResult InitResult = EOS_Initialize(&SDKOptions);
	if (InitResult != EOS_EResult::EOS_Success)
	{
		FDebugLog::Log(L"[EOS SDK] Init Failed!");
		return;
	}

	FDebugLog::Log(L"[EOS SDK] Initialized. Setting Logging Callback ...");
	EOS_EResult SetLogCallbackResult = EOS_Logging_SetCallback(&EOSSDKLoggingCallback);
	if (SetLogCallbackResult != EOS_EResult::EOS_Success)
	{
		FDebugLog::Log(L"[EOS SDK] Set Logging Callback Failed!");
	}
	else
	{
		FDebugLog::Log(L"[EOS SDK] Logging Callback Set");
		EOS_Logging_SetLogLevel(EOS_ELogCategory::EOS_LC_ALL_CATEGORIES, EOS_ELogLevel::EOS_LOG_Verbose);
	}

#ifdef EOS_STEAM_ENABLED
	if (!FCommandLine::Get().HasParam(CommandLineConstants::Server))
	{
		FSteamManager::GetInstance().Init();
	}
#endif

	const bool bCreateSuccess = FPlatform::Create();
	if (!bCreateSuccess)
	{
		FDebugLog::Log(L"[EOS SDK] Platform Create failed ...");
	}
}

#ifdef DXTK
void FMain::Initialize(HWND Window, int Width, int Height)
{
	Game = std::make_unique<FGame>();

	DXDeviceResources = std::make_unique<DeviceResources>();
	DXDeviceResources->RegisterDeviceNotify(this);
	DXDeviceResources->SetWindow(Window, Width, Height);

	DXDeviceResources->CreateDeviceResources();
	CreateDeviceDependentResources();

	DXDeviceResources->CreateWindowSizeDependentResources();
	CreateWindowSizeDependentResources();

	Game->UpdateLayout(Width, Height);
	Game->Init();
}
#endif //DXTK

#ifdef EOS_DEMO_SDL
void FMain::Initialize(SDL_Window* Window, SDL_GLContext InGLContext, int Width, int Height)
{
	SDLWindow = Window;
	GLContext = InGLContext;

	Game = std::make_unique<FGame>();

	CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();

	Game->UpdateLayout(Width, Height);
	Game->Init();
}

bool FMain::GLError(const wchar_t *ContextStr)
{
	GLenum GLError = glGetError();
	if (GLError != GL_NO_ERROR)
	{
		std::wstring GLErrorStrW = L"Unknown";

		switch (GLError)
		{
		case GL_INVALID_ENUM:
			GLErrorStrW = L"Invalid Enum";
			break;

		case GL_INVALID_VALUE:
			GLErrorStrW = L"Invalid Value";
			break;

		case GL_INVALID_OPERATION:
			GLErrorStrW = L"Invalid Operation";
			break;

		case GL_STACK_OVERFLOW:
			GLErrorStrW = L"Stack Overflow";
			break;

		case GL_STACK_UNDERFLOW:
			GLErrorStrW = L"Stack Underflow";
			break;

		case GL_OUT_OF_MEMORY:
			GLErrorStrW = L"Out of Memory";
			break;

			default:
				break;
		}

		FDebugLog::LogError(L"[%ls] GL Error: %ls\n", ContextStr, GLErrorStrW.c_str());
		return true;
	}
	return false;
}
#endif //EOS_DEMO_SDL

void FMain::Tick()
{
	Timer.Tick([&]()
	{
		Update();
	});

	Render();
}

void FMain::Update()
{
#ifdef DXTK
	Vector3 eye(0.0f, 0.7f, 1.5f);
	Vector3 at(0.0f, -0.1f, 0.0f);
	View = FMatrix::CreateLookAt(eye, at, Vector3(0.0f, 1.0f, 0.0f));
#endif //DXTK

	if (Game)
	{
		Game->Update();
	}
}

void FMain::Render()
{
	// Don't try to render anything before the first Update.
	if (Timer.GetFrameCount() == 0)
	{
		return;
	}

	Clear();

#ifdef DXTK
	DXDeviceResources->PIXBeginEvent(L"Render");
#endif //DXTK

	if (Game)
	{
		Game->Render();
	}

#ifdef DXTK
	// Show the new frame.
	DXDeviceResources->Present();

	DXDeviceResources->PIXEndEvent();
#endif //DXTK
}

void FMain::Clear()
{
	FColor ClearCol = FColor(0.09f, 0.11f, 0.11f, 1.f);

#ifdef DXTK
	DXDeviceResources->PIXBeginEvent(L"Clear");

	// Clear the views.
	auto context = DXDeviceResources->GetD3DDeviceContext();
	auto renderTarget = DXDeviceResources->GetRenderTargetView();
	auto depthStencil = DXDeviceResources->GetDepthStencilView();
	
	context->ClearRenderTargetView(renderTarget, ClearCol);
	context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	context->OMSetRenderTargets(1, &renderTarget, depthStencil);

	// Set the viewport.
	auto viewport = DXDeviceResources->GetScreenViewport();
	context->RSSetViewports(1, &viewport);

	DXDeviceResources->PIXEndEvent();
#endif //DXTK

#ifdef EOS_DEMO_SDL
	glClearColor(GLclampf(ClearCol.R), GLclampf(ClearCol.G), GLclampf(ClearCol.B), GLclampf(ClearCol.A));
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#endif
}

void FMain::OnActivated()
{

}

void FMain::OnDeactivated()
{

}

void FMain::OnSuspending()
{

}

void FMain::OnResuming()
{
	Timer.ResetElapsedTime();
}

void FMain::OnWindowMoved()
{
#ifdef DXTK
	auto r = DXDeviceResources->GetOutputSize();
	DXDeviceResources->WindowSizeChanged(r.right, r.bottom);
#endif //DXTK
}

void FMain::OnWindowSizeChanged(int Width, int Height)
{
#ifdef DXTK
	if (!DXDeviceResources->WindowSizeChanged(Width, Height))
		return;
#endif

	CreateWindowSizeDependentResources();

	Game->UpdateLayout(Width, Height);
}

void FMain::GetMinimumSize(int& Width, int& Height) const
{
	Width = SampleConstants::MinimumWindowWidth;
	Height = SampleConstants::MinimumWindowHeight;
}

void FMain::GetDefaultSize(int& Width, int& Height) const
{
	Width = SampleConstants::DefaultWindowWidth;
	Height = SampleConstants::DefaultWindowHeight;
}

void FMain::InitCommandLine()
{
	bIsFullScreen = HasFullScreenCommandLine();
}

bool FMain::HasFullScreenCommandLine()
{
	return FCommandLine::Get().HasFlagParam(CommandLineConstants::Fullscreen);
}

FStepTimer const& FMain::GetTimer()
{
	return Timer;
}

#ifdef DXTK
std::unique_ptr<DeviceResources> const& FMain::GetDeviceResources()
{
	return DXDeviceResources;
}

FMatrix const& FMain::GetWorld()
{
	return World;
}

FMatrix const& FMain::GetView()
{
	return View;
}

FMatrix const& FMain::GetProjection()
{
	return Projection;
}
#endif //DXTK

void FMain::CreateDeviceDependentResources()
{
#ifdef EOS_DEMO_SDL
	//load shaders
	FSDLSpriteBatch::Init();
#endif

	if (Game)
	{
		Game->Create();
	}
}

void FMain::CreateWindowSizeDependentResources()
{
	Vector2 Size;
#ifdef DXTK
	Size = Vector2(float(DXDeviceResources->GetOutputSize().right), float(DXDeviceResources->GetOutputSize().bottom));
#endif

#ifdef EOS_DEMO_SDL
	int Width = 0, Height = 0;
	SDL_GetWindowSize(SDLWindow, &Width, &Height);
	Size = Vector2(float(Width), float(Height));
#endif

	float aspectRatio = float(Size.x) / float(Size.y);

	// This sample makes use of a right-handed coordinate system using row-major matrices.
#ifdef DXTK
	const float PI = 3.141592654f;
	float fovAngleY = 70.0f * PI / 180.0f;

	Projection = FMatrix::CreatePerspectiveFieldOfView(
		fovAngleY,
		aspectRatio,
		0.01f,
		100.0f
	);
#endif
}

#ifdef DXTK
void FMain::OnDeviceLost()
{
	//output an error and quit
	MessageBoxA(0, "DirectX device is lost. Please restart the application.", "Error: Device lost", MB_OK);
	PostQuitMessage(0);
}

void FMain::OnDeviceRestored()
{
	CreateDeviceDependentResources();

	CreateWindowSizeDependentResources();
}
#endif //DXTK

void FMain::PrintToConsole(const std::wstring& Message)
{
	if (Game)
	{
		Game->PrintToConsole(Message);
	}
}

void FMain::PrintWarningToConsole(const std::wstring& Message)
{
	if (Game)
	{
		Game->PrintWarningToConsole(Message);
	}
}

void FMain::PrintErrorToConsole(const std::wstring& Message)
{
	if (Game)
	{
		Game->PrintErrorToConsole(Message);
	}
}

void FMain::OnShutdown()
{
	if (Game)
	{
		Game->OnShutdown();
	}
}

bool FMain::IsShutdownDelayed() const
{
	if (Game)
	{
		return Game->IsShutdownDelayed();
	}

	return false;
}
