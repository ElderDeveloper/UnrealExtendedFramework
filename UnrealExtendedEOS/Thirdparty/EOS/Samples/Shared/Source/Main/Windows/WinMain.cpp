// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"

#ifdef DXTK

#include "StringUtils.h"
#include "Input.h"
#include "SampleConstants.h"
#include "Main.h"
#include "DebugLog.h"
#include "CommandLine.h"
#include "Settings.h"

using namespace DirectX;

// Forward declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void UpdateWindowSize(int Width, int Height);

// Indicates to hybrid graphics systems to prefer the discrete part by default
extern "C"
{
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

static bool bShuttingDown = false;

int WINAPI wWinMain(_In_ HINSTANCE HInstance, _In_opt_ HINSTANCE HPrevInstance, _In_ LPWSTR CmdLine, _In_ int NumCmdShow)
{
	UNREFERENCED_PARAMETER(HPrevInstance);

	if (!XMVerifyCPUSupport())
		return 1;

	HRESULT HR = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	if (FAILED(HR))
		return 1;

	FCommandLine::Get().Init(CmdLine);
	FSettings::Get().Init();

	Main = std::make_unique<FMain>();
	Main->InitCommandLine();
	Main->InitPlatform();

	// Register class and create window
	{
		// Register class
		WNDCLASSEX WCEX;
		WCEX.cbSize = sizeof(WNDCLASSEX);
		WCEX.style = CS_HREDRAW | CS_VREDRAW;
		WCEX.lpfnWndProc = WndProc;
		WCEX.cbClsExtra = 0;
		WCEX.cbWndExtra = 0;
		WCEX.hInstance = HInstance;
		WCEX.hIcon = LoadIcon(HInstance, L"IDI_ICON");
		WCEX.hCursor = LoadCursor(nullptr, IDC_ARROW);
		WCEX.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		WCEX.lpszMenuName = nullptr;
		WCEX.lpszClassName = L"EOSSDKSampleWindowClass";
		WCEX.hIconSm = LoadIcon(WCEX.hInstance, L"IDI_ICON");
		if (!RegisterClassEx(&WCEX))
			return 1;

		// Create window
		int W, H;
		Main->GetDefaultSize(W, H);

		RECT RC;
		RC.top = 0;
		RC.left = 0;
		RC.right = static_cast<LONG>(W);
		RC.bottom = static_cast<LONG>(H);

		AdjustWindowRect(&RC, WS_OVERLAPPEDWINDOW, FALSE);

		HWND HWnd = nullptr;

		if (!Main->bIsFullScreen)
		{
			HWnd = CreateWindowEx(0, L"EOSSDKSampleWindowClass", FStringUtils::Widen(SampleConstants::GameName).c_str(), WS_OVERLAPPEDWINDOW,
				CW_USEDEFAULT, CW_USEDEFAULT, RC.right - RC.left, RC.bottom - RC.top, nullptr, nullptr, HInstance,
				nullptr);
		}
		else
		{
			HWnd = CreateWindowEx(WS_EX_TOPMOST, L"EOSSDKSampleWindowClass", FStringUtils::Widen(SampleConstants::GameName).c_str(), WS_POPUP,
				0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), nullptr, nullptr, HInstance, nullptr);
		}

		if (!HWnd)
			return 1;

		ShowWindow(HWnd, NumCmdShow);

		SetWindowLongPtr(HWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(Main.get()));

		RECT ClientRC;
		GetClientRect(HWnd, &ClientRC);
		// Validate client rect, if invalid we just use default rect based on window width and height
		if ((ClientRC.bottom - ClientRC.top > 0) && (ClientRC.right - ClientRC.left > 0))
		{
			RC = ClientRC;
		}

		Main->Initialize(HWnd, RC.right - RC.left, RC.bottom - RC.top);
	}

	// Main message loop
	MSG Msg = {};
	while (WM_QUIT != Msg.message)
	{
		if (bShuttingDown && !Main->IsShutdownDelayed())
		{
			bShuttingDown = false;
			PostQuitMessage(0);
		}

		if (PeekMessage(&Msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&Msg);
			DispatchMessage(&Msg);
		}
		else
		{
			Main->Tick();
		}
	}


	Main.reset();

#if defined (_DEBUG) && defined (DUMP_MEM_LEAKS)
	_CrtDumpMemoryLeaks();
#endif // _DEBUG

	CoUninitialize();

	return (int)Msg.wParam;
}

LRESULT CALLBACK WndProc(HWND HWnd, UINT Message, WPARAM WParam, LPARAM LParam)
{
	PAINTSTRUCT PS;
	HDC Hdc;

	static bool bInSizemove = false;
	static bool bInSuspend = false;
	static bool bMinimized = false;

	auto MainPtr = reinterpret_cast<FMain*>(GetWindowLongPtr(HWnd, GWLP_USERDATA));

	switch (Message)
	{
	case WM_PAINT:
		if (bInSizemove && MainPtr)
		{
			MainPtr->Tick();
		}
		else
		{
			Hdc = BeginPaint(HWnd, &PS);
			EndPaint(HWnd, &PS);
		}
		break;

	case WM_MOVE:
		if (MainPtr)
		{
			MainPtr->OnWindowMoved();
		}
		break;

	case WM_SIZE:
		if (WParam == SIZE_MINIMIZED)
		{
			if (!bMinimized)
			{
				bMinimized = true;
				if (!bInSuspend && MainPtr)
				{
					MainPtr->OnSuspending();
				}
				bInSuspend = true;
			}
		}
		else if (bMinimized)
		{
			bMinimized = false;
			if (bInSuspend && MainPtr)
			{
				MainPtr->OnResuming();
			}
			bInSuspend = false;
		}
		else if (!bInSizemove && MainPtr)
		{
			UpdateWindowSize(LOWORD(LParam), HIWORD(LParam));
		}
		break;

	case WM_ENTERSIZEMOVE:
		bInSizemove = true;
		break;

	case WM_EXITSIZEMOVE:
		bInSizemove = false;
		if (MainPtr)
		{
			RECT RC;
			GetClientRect(HWnd, &RC);
			UpdateWindowSize(RC.right - RC.left, RC.bottom - RC.top);
		}
		break;

	case WM_GETMINMAXINFO:
	{
		int MinWidth = 854;
		int MinHeight = 480;
		if (Main)
		{
			Main->GetDefaultSize(MinWidth, MinHeight);
		}
		auto Info = reinterpret_cast<MINMAXINFO*>(LParam);
		Info->ptMinTrackSize.x = MinWidth;
		Info->ptMinTrackSize.y = MinHeight;
	}
	break;

	case WM_ACTIVATEAPP:
		if (MainPtr)
		{
			if (WParam)
			{
				MainPtr->OnActivated();
			}
			else
			{
				MainPtr->OnDeactivated();
			}
		}
		break;

	case WM_POWERBROADCAST:
		switch (WParam)
		{
		case PBT_APMQUERYSUSPEND:
			if (!bInSuspend && MainPtr)
			{
				MainPtr->OnSuspending();
			}
			bInSuspend = true;
			return TRUE;

		case PBT_APMRESUMESUSPEND:
			if (!bMinimized)
			{
				if (bInSuspend && MainPtr)
				{
					MainPtr->OnResuming();
				}
				bInSuspend = false;
			}
			return TRUE;
		}
		break;

	case WM_CLOSE:
		bShuttingDown = true;
		MainPtr->OnShutdown();
		if (!MainPtr->IsShutdownDelayed())
		{
			DestroyWindow(HWnd);
		}
		return 0;

	case WM_DESTROY: 
		PostQuitMessage(0);
		break;

	case WM_INPUT:
	case WM_MOUSEMOVE:
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEWHEEL:
	case WM_XBUTTONDOWN:
	case WM_XBUTTONUP:
	case WM_MOUSEHOVER:
		FInput::ProcessMessage(Message, WParam, LParam);
		break;

	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYUP:
	case WM_CHAR:
		FInput::ProcessMessage(Message, WParam, LParam);
		break;

	case WM_SYSKEYDOWN:
		if (WParam == VK_RETURN && (LParam & 0x60000000) == 0x20000000)
		{
			// Implements the classic ALT+ENTER fullscreen toggle
			if (Main->bIsFullScreen)
			{
				SetWindowLongPtr(HWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
				SetWindowLongPtr(HWnd, GWL_EXSTYLE, 0);

				int Width = 1024;
				int Height = 768;
				if (MainPtr)
				{
					MainPtr->GetDefaultSize(Width, Height);
				}

				ShowWindow(HWnd, SW_SHOWNORMAL);

				SetWindowPos(HWnd, HWND_TOP, 0, 0, Width, Height, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
			}
			else
			{
				SetWindowLongPtr(HWnd, GWL_STYLE, 0);
				SetWindowLongPtr(HWnd, GWL_EXSTYLE, WS_EX_TOPMOST);

				SetWindowPos(HWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

				ShowWindow(HWnd, SW_SHOWMAXIMIZED);
			}

			Main->bIsFullScreen = !Main->bIsFullScreen;
		}
		FInput::ProcessMessage(Message, WParam, LParam);
		break;

	case WM_MENUCHAR:
		// A menu is active and the user presses a key that does not correspond
		// to any mnemonic or accelerator key. Ignore so we don't produce an error beep.
		return MAKELRESULT(0, MNC_CLOSE);

	}

	return DefWindowProc(HWnd, Message, WParam, LParam);
}

void UpdateWindowSize(int Width, int Height)
{
	if (Main)
	{
		Main->OnWindowSizeChanged(Width, Height);
	}
}

#endif //DXTK
