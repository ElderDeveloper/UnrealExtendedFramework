// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#ifdef DXTK
#include "Keyboard.h"
#include "Input.h"
#include "GamePad.h"
#include "Menu.h"
#include "Game.h"

using DirectX::Keyboard;
using DirectX::GamePad;
using DirectX::Mouse;

// Default Windows keyboard mappings from input commands to key bindings
std::map<int, int> KeyBindings
{
	{ int(FInput::InputCommands::Exit), FInput::Escape },
	{ int(FInput::InputCommands::ConsoleToggle), FInput::OemTilde },
	{ int(FInput::InputCommands::ConsoleToggleAlt), FInput::Oem8 },
	{ int(FInput::InputCommands::UIAccept), FInput::Enter }
};

// Default Windows mouse button mappings from input commands to mouse buttons
std::map<int, int> MouseBindings
{
	{ int(FInput::InputCommands::UIClicked), int(FInput::MouseButtons::Left) }
};


// Default Windows gamepad button mappings from input commands to gamapad buttons
std::map<int, int> GamePadBindings
{
	{ int(FInput::InputCommands::Exit), int(FInput::GamePadButtons::View) }
};


FInput::FInput()
{
	InputKeyboard = std::make_unique<Keyboard>();
	KeyboardTracker = std::make_unique<Keyboard::KeyboardStateTracker>();

	InputMouse = std::make_unique<Mouse>();
	MouseTracker = std::make_unique<Mouse::ButtonStateTracker>();

	InputGamePad = std::make_unique<GamePad>();
	GamePadTracker = std::make_unique<GamePad::ButtonStateTracker>();
}

FInput::~FInput()
{

}

void FInput::Update()
{
	if (InputKeyboard && KeyboardTracker)
	{
		const auto KeyboardState = InputKeyboard->GetState();
		KeyboardTracker->Update(KeyboardState);

		//Generate events
		if (IsKeyPressed(InputCommands::SelectAll))
		{
			FUIEvent NewEvent(EUIEventType::SelectAll);
			FGame::Get().GetMenu()->OnUIEvent(NewEvent);
		}
		if (IsKeyPressed(InputCommands::PasteText))
		{
			FUIEvent NewEvent(EUIEventType::PasteText);
			FGame::Get().GetMenu()->OnUIEvent(NewEvent);
		}
		if (IsKeyPressed(InputCommands::CopyText))
		{
			FUIEvent NewEvent(EUIEventType::CopyText);
			FGame::Get().GetMenu()->OnUIEvent(NewEvent);
		}
	}

	if (InputMouse && MouseTracker)
	{
		const auto& MouseState = InputMouse->GetState();

		// Update mouse wheel
		int ScrollWheelDiff = MouseState.scrollWheelValue - ScrollWheelState;
		ScrollWheelState = MouseState.scrollWheelValue;
		const int WheelDelta = 120;
		if (std::abs(ScrollWheelDiff) >= WheelDelta)
		{
			const int NumLinesScrolled = ScrollWheelDiff / WheelDelta;
			ScrolledLinesThisFrame = NumLinesScrolled;
		}
		else
		{
			ScrolledLinesThisFrame = 0;
		}
		MouseTracker->Update(MouseState);
	}

	if (InputGamePad && GamePadTracker)
	{
		// Just use 0 for index for first player
		const auto& GamePadState = InputGamePad->GetState(0);
		GamePadTracker->Update(GamePadState);
	}
}

void FInput::Reset()
{
	if (InputKeyboard)
	{
		InputKeyboard->Reset();
		InputKeyboard.reset();
	}

	if (KeyboardTracker)
	{
		KeyboardTracker->Reset();
	}

	if (InputMouse)
	{
		InputMouse.reset();
	}

	if (MouseTracker)
	{
		MouseTracker->Reset();
	}

	if (InputGamePad)
	{
		InputGamePad.reset();
	}

	if (GamePadTracker)
	{
		GamePadTracker->Reset();
	}
}

std::wstring FInput::GetClipboardText() const
{
	std::wstring Result;

	if (!OpenClipboard(nullptr))
		return Result;

	HANDLE HData = GetClipboardData(CF_UNICODETEXT);
	if (HData == nullptr)
		return Result;

	wchar_t* FriendData = static_cast<wchar_t*>(HData);
	if (!FriendData)
		return Result;

	Result = FriendData;

	GlobalUnlock(HData);
	CloseClipboard();

	return Result;
}

void FInput::CopyToClipboard(const std::vector<std::wstring>& Lines) const
{
	if (!OpenClipboard(nullptr))
		return;

	EmptyClipboard();

	size_t MemSize = 0;
	for (const auto& NextLine : Lines)
	{
		MemSize += (NextLine.size() + 1) * sizeof(wchar_t);
	}
	MemSize += sizeof(wchar_t);

	HGLOBAL HMem = GlobalAlloc(GMEM_MOVEABLE, MemSize);

	//populate data
	{
		char* MemBlockPtr = static_cast<char*>(GlobalLock(HMem));
		for (const auto& NextLine : Lines)
		{
			std::wstring LineToWrite = NextLine + L'\n';
			size_t LineMemLength = LineToWrite.size() * sizeof(wchar_t);
			memcpy(MemBlockPtr, LineToWrite.c_str(), LineMemLength);
			MemBlockPtr += LineMemLength;
		}

		wchar_t Zero = L'\0';
		memcpy(MemBlockPtr, &Zero, sizeof(wchar_t));
	}

	GlobalUnlock(HMem);

	SetClipboardData(CF_UNICODETEXT, HMem);
	CloseClipboard();
}

bool FInput::IsKeyPressed(InputCommands InputCommand) const
{
	if (InputCommand == InputCommands::PasteText)
	{
		return (KeyboardTracker->pressed.V && (KeyboardTracker->GetLastState().IsKeyDown(Keyboard::LeftControl) || KeyboardTracker->GetLastState().IsKeyDown(Keyboard::RightControl)));
	}
	if (InputCommand == InputCommands::SelectAll)
	{
		return (KeyboardTracker->pressed.A && (KeyboardTracker->GetLastState().IsKeyDown(Keyboard::LeftControl) || KeyboardTracker->GetLastState().IsKeyDown(Keyboard::RightControl)));
	}
	if (InputCommand == InputCommands::CopyText)
	{
		return (KeyboardTracker->pressed.C && (KeyboardTracker->GetLastState().IsKeyDown(Keyboard::LeftControl) || KeyboardTracker->GetLastState().IsKeyDown(Keyboard::RightControl)));
	}

	if (InputCommand == InputCommands::SearchText)
	{
		return (KeyboardTracker->pressed.F && (KeyboardTracker->GetLastState().IsKeyDown(Keyboard::LeftControl) || KeyboardTracker->GetLastState().IsKeyDown(Keyboard::RightControl)));
	}

	auto KeyBindingPair = KeyBindings.find(int(InputCommand));
	if (KeyBindingPair != KeyBindings.end())
	{
		Keyboard::Keys KeyBinding = static_cast<Keyboard::Keys>(KeyBindingPair->second);
		return KeyboardTracker->IsKeyPressed(KeyBinding);
	}

	return false;
}

bool FInput::IsKeyReleased(FInput::Keys Key) const
{
	if (KeyboardTracker)
	{
		return KeyboardTracker->IsKeyReleased(static_cast<Keyboard::Keys>(Key));
	}

	return false;
}

bool FInput::IsKeyReleased(InputCommands InputCommand) const
{
	if (KeyboardTracker && InputKeyboard && InputKeyboard->IsConnected())
	{
		auto KeyBindingPair = KeyBindings.find(int(InputCommand));
		if (KeyBindingPair != KeyBindings.end())
		{
			Keyboard::Keys KeyBinding = static_cast<Keyboard::Keys>(KeyBindingPair->second);
			return KeyboardTracker->IsKeyReleased(KeyBinding);
		}
	}

	return false;
}

bool FInput::IsMouseButtonPressed(InputCommands InputCommand)
{
	if (MouseTracker && InputMouse && InputMouse->IsConnected())
	{
		auto MouseBindingPair = MouseBindings.find(int(InputCommand));
		if (MouseBindingPair != MouseBindings.end())
		{
			MouseButtons MouseBinding = static_cast<MouseButtons>(MouseBindingPair->second);
			switch (MouseBinding)
			{
			case MouseButtons::Left:
				return MouseTracker->leftButton == Mouse::ButtonStateTracker::ButtonState::PRESSED;
			case MouseButtons::Right:
				return MouseTracker->rightButton == Mouse::ButtonStateTracker::ButtonState::PRESSED;
			case MouseButtons::Middle:
				return MouseTracker->middleButton == Mouse::ButtonStateTracker::ButtonState::PRESSED;
			default:
				break;
			}
		}
	}

	return false;
}

bool FInput::IsMouseButtonReleased(InputCommands InputCommand)
{
	if (MouseTracker && InputMouse && InputMouse->IsConnected())
	{
		auto MouseBindingPair = MouseBindings.find(int(InputCommand));
		if (MouseBindingPair != MouseBindings.end())
		{
			MouseButtons MouseBinding = static_cast<MouseButtons>(MouseBindingPair->second);
			switch (MouseBinding)
			{
			case MouseButtons::Left:
				return MouseTracker->leftButton == Mouse::ButtonStateTracker::ButtonState::RELEASED;
			case MouseButtons::Right:
				return MouseTracker->rightButton == Mouse::ButtonStateTracker::ButtonState::RELEASED;
			case MouseButtons::Middle:
				return MouseTracker->middleButton == Mouse::ButtonStateTracker::ButtonState::RELEASED;
			default:
				break;
			}
		}
	}

	return false;
}

int FInput::GetMouseScrollWheelLines() const
{
	return ScrolledLinesThisFrame;
}

bool FInput::IsGamePadButtonPressed(InputCommands InputCommand)
{
	if (GamePadTracker && GamePadTracker->GetLastState().IsConnected())
	{
		auto GamePadBindingPair = GamePadBindings.find(int(InputCommand));
		if (GamePadBindingPair != GamePadBindings.end())
		{
			GamePadButtons GamePadBinding = static_cast<GamePadButtons>(GamePadBindingPair->second);
			switch (GamePadBinding)
			{
			case GamePadButtons::A:
				return GamePadTracker->a == GamePad::ButtonStateTracker::ButtonState::PRESSED;
			case GamePadButtons::B:
				return GamePadTracker->b == GamePad::ButtonStateTracker::ButtonState::PRESSED;
			case GamePadButtons::X:
				return GamePadTracker->x == GamePad::ButtonStateTracker::ButtonState::PRESSED;
			case GamePadButtons::Y:
				return GamePadTracker->y == GamePad::ButtonStateTracker::ButtonState::PRESSED;

			case GamePadButtons::LeftStick:
				return GamePadTracker->leftStick == GamePad::ButtonStateTracker::ButtonState::PRESSED;
			case GamePadButtons::RightStick:
				return GamePadTracker->rightStick == GamePad::ButtonStateTracker::ButtonState::PRESSED;

			case GamePadButtons::LeftShoulder:
				return GamePadTracker->leftShoulder == GamePad::ButtonStateTracker::ButtonState::PRESSED;
			case GamePadButtons::RightShoulder:
				return GamePadTracker->rightShoulder == GamePad::ButtonStateTracker::ButtonState::PRESSED;

			case GamePadButtons::Back:
				return GamePadTracker->back == GamePad::ButtonStateTracker::ButtonState::PRESSED;
			case GamePadButtons::View:
				return GamePadTracker->view == GamePad::ButtonStateTracker::ButtonState::PRESSED;

			case GamePadButtons::Start:
				return GamePadTracker->start == GamePad::ButtonStateTracker::ButtonState::PRESSED;
			case GamePadButtons::Menu:
				return GamePadTracker->menu == GamePad::ButtonStateTracker::ButtonState::PRESSED;

			case GamePadButtons::DPadUp:
				return GamePadTracker->dpadUp == GamePad::ButtonStateTracker::ButtonState::PRESSED;
			case GamePadButtons::DPadDown:
				return GamePadTracker->dpadDown == GamePad::ButtonStateTracker::ButtonState::PRESSED;
			case GamePadButtons::DPadLeft:
				return GamePadTracker->dpadLeft == GamePad::ButtonStateTracker::ButtonState::PRESSED;
			case GamePadButtons::DPadRight:
				return GamePadTracker->dpadRight == GamePad::ButtonStateTracker::ButtonState::PRESSED;

			case GamePadButtons::LeftStickUp:
				return GamePadTracker->leftStickUp == GamePad::ButtonStateTracker::ButtonState::PRESSED;
			case GamePadButtons::LeftStickDown:
				return GamePadTracker->leftStickDown == GamePad::ButtonStateTracker::ButtonState::PRESSED;
			case GamePadButtons::LeftStickLeft:
				return GamePadTracker->leftStickLeft == GamePad::ButtonStateTracker::ButtonState::PRESSED;
			case GamePadButtons::LeftStickRight:
				return GamePadTracker->leftStickRight == GamePad::ButtonStateTracker::ButtonState::PRESSED;

			case GamePadButtons::RightStickUp:
				return GamePadTracker->rightStickUp == GamePad::ButtonStateTracker::ButtonState::PRESSED;
			case GamePadButtons::RightStickDown:
				return GamePadTracker->rightStickDown == GamePad::ButtonStateTracker::ButtonState::PRESSED;
			case GamePadButtons::RightStickLeft:
				return GamePadTracker->rightStickLeft == GamePad::ButtonStateTracker::ButtonState::PRESSED;
			case GamePadButtons::RightStickRight:
				return GamePadTracker->rightStickRight == GamePad::ButtonStateTracker::ButtonState::PRESSED;

			case GamePadButtons::LeftTrigger:
				return GamePadTracker->leftTrigger == GamePad::ButtonStateTracker::ButtonState::PRESSED;
			case GamePadButtons::RightTrigger:
				return GamePadTracker->rightTrigger == GamePad::ButtonStateTracker::ButtonState::PRESSED;

			default:
				break;
			}
		}
	}

	return false;
}

bool FInput::IsAnyKeyPressed() const
{
	if (KeyboardTracker)
	{
		return KeyboardTracker->GetLastState().IsAnyKeyPressed();
	}

	return false;
}

Vector2 FInput::GetMousePosition() const
{
	Vector2 MousePos;

	if (MouseTracker && InputMouse && InputMouse->IsConnected())
	{
		const auto& MouseState = MouseTracker->GetLastState();

		MousePos.x = static_cast<float>(MouseState.x);
		MousePos.y = static_cast<float>(MouseState.y);
	}

	return MousePos;
}

std::vector<FInput::Keys> FInput::GetKeysDown() const
{
	std::vector<FInput::Keys> Result;

	if (KeyboardTracker && IsAnyKeyPressed())
	{
		for (size_t NextKeyCode = 0; NextKeyCode <= static_cast<size_t>(Keyboard::Keys::OemClear); ++NextKeyCode)
		{
			if (KeyboardTracker->IsKeyPressed(static_cast<Keyboard::Keys>(NextKeyCode)))
			{
				//We don't want to generate button press event when key combination is pressed:
				wchar_t KeyPressed = static_cast<wchar_t>(NextKeyCode);

				if ((KeyPressed == L'V' || KeyPressed == L'v') && IsKeyPressed(FInput::InputCommands::PasteText))
				{
					continue;
				}
				if ((KeyPressed == L'A' || KeyPressed == L'a') && IsKeyPressed(FInput::InputCommands::SelectAll))
				{
					continue;
				}
				if ((KeyPressed == L'C' || KeyPressed == L'c') && IsKeyPressed(FInput::InputCommands::CopyText))
				{
					continue;
				}

				Result.push_back(static_cast<FInput::Keys>(NextKeyCode));
			}
		}
	}

	return Result;
}

bool FInput::IsCapsKeyDown() const
{
	if (KeyboardTracker)
	{
		return KeyboardTracker->GetLastState().IsKeyDown(Keyboard::LeftShift) ||
			KeyboardTracker->GetLastState().IsKeyDown(Keyboard::RightShift) ||
			KeyboardTracker->GetLastState().IsKeyDown(Keyboard::CapsLock);
	}

	return false;
}

bool FInput::IsAltKeyDown() const
{
	if (KeyboardTracker)
	{
		return KeyboardTracker->GetLastState().IsKeyDown(Keyboard::LeftAlt) ||
			KeyboardTracker->GetLastState().IsKeyDown(Keyboard::RightAlt);
	}

	return false;
}

void FInput::ProcessMessage(UINT Message, WPARAM WParam, LPARAM LParam)
{
	switch (Message)
	{
		// Mouse
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
	{
		Mouse::ProcessMessage(Message, WParam, LParam);
		break;
	}

	// Keyboard
	case WM_KEYDOWN:
	case WM_KEYUP:
	{
		Keyboard::ProcessMessage(Message, WParam, LParam);
		break;
	}
	case WM_SYSKEYUP:
	case WM_SYSKEYDOWN:
	{
		Keyboard::ProcessMessage(Message, WParam, LParam);
		break;
	}
	}
}

WCHAR FInput::GetKeyChar(UINT KeyCode)
{
	BYTE KeyboardState[256];
	auto KeyboardLayout = GetKeyboardLayout(0);
	GetKeyboardState(KeyboardState);

	switch (KeyCode)
	{
	case VK_PRIOR:
	case VK_NEXT:
	case VK_END:
	case VK_HOME:
	case VK_LEFT:
	case VK_UP:
	case VK_RIGHT:
	case VK_DOWN:
	case VK_SELECT:
	case VK_PRINT:
	case VK_EXECUTE:
	case VK_SNAPSHOT:
	case VK_INSERT:
	case VK_DELETE:
	case VK_HELP:
		return 0;
	}

	WCHAR Wchar[5] = {};

	UINT ScanCode = MapVirtualKey(KeyCode, MAPVK_VK_TO_VSC);
	int Ret = ToUnicodeEx(KeyCode, ScanCode, KeyboardState, Wchar, 4, 0, KeyboardLayout);

	if (Ret == 1)
	{
		return Wchar[0];
	}

	return static_cast<WCHAR>(KeyCode);
}

bool FInput::IsKeyShifted(UINT KeyCode)
{
	BYTE KeyboardState[256];
	GetKeyboardState(KeyboardState);

	if (KeyboardState[VK_SHIFT] & 0x80)
	{
		return true;
	}

	return false;
}

#endif // DXTK