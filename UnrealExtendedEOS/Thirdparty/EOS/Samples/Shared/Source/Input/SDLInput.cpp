// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"

#ifdef EOS_DEMO_SDL
#include "DebugLog.h"
#include "StringUtils.h"
#include "SDLInput.h"
#include "UIEvent.h"

Sint32 FInput::MouseWheelPosY = 0;
SDL_GameController* FInput::GameController = nullptr;
int FInput::WhichController = -1;
Uint8 FInput::ButtonStates[SDL_CONTROLLER_BUTTON_MAX];
Uint8 FInput::ButtonStatesPrev[SDL_CONTROLLER_BUTTON_MAX];
float FInput::AxisValues[SDL_CONTROLLER_AXIS_MAX];

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
	{ int(FInput::InputCommands::Exit), int(FInput::GamePadButtons::Back) }
};

FInput::FInput() :
	MouseState(0),
	PreviousMouseState(0)
{
	// Init keyboard
	memset(KeyboardState, 0, sizeof(Uint8)*SDL_NUM_SCANCODES);
	memcpy(PreviousKeyboardState, SDL_GetKeyboardState(NULL), sizeof(Uint8)*SDL_NUM_SCANCODES);

	// Init controllers
	memset(ButtonStates, 0, sizeof(Uint8)*SDL_CONTROLLER_BUTTON_MAX);
	memset(ButtonStatesPrev, 0, sizeof(Uint8)*SDL_CONTROLLER_BUTTON_MAX);
	memset(AxisValues, 0, sizeof(float)*SDL_CONTROLLER_AXIS_MAX);

	SDL_GameControllerEventState(SDL_IGNORE);

	// Use first available controller
	int NumControllers = SDL_NumJoysticks();
	if (NumControllers >= 1)
	{
		for (int i = 0; i < NumControllers; i++)
		{
			if (SDL_IsGameController(i))
			{
				GameController = SDL_GameControllerOpen(i);
				break;
			}
		}
	}
}

FInput::~FInput()
{

}

void FInput::Update()
{
	UpdateKeyboard();
	UpdateMouse();
	UpdateControllers();
}

void FInput::UpdateKeyboard()
{
	memcpy(PreviousKeyboardState, KeyboardState, sizeof(Uint8)*SDL_NUM_SCANCODES);
	memcpy(KeyboardState, SDL_GetKeyboardState(NULL), sizeof(Uint8)*SDL_NUM_SCANCODES);
}

void FInput::UpdateMouse()
{
	PreviousMouseState = MouseState;
	PreviousMousePos = MousePos;

	int x = 0, y = 0;
	MouseState = SDL_GetMouseState(&x, &y);

	MousePos.x = (float)x;
	MousePos.y = (float)y;

	MouseWheelPosY = 0;
}

void FInput::UpdateControllers()
{
	if (GameController == nullptr)
	{
		return;
	}

	memcpy(&ButtonStatesPrev, &ButtonStates, sizeof(Uint8)*SDL_CONTROLLER_BUTTON_MAX);

	SDL_GameControllerUpdate();

	for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; ++i)
	{
		ButtonStates[i] = SDL_GameControllerGetButton(GameController, (SDL_GameControllerButton)i);
	}

	for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; ++i)
	{
		AxisValues[i] = SDL_GameControllerGetAxis(GameController, (SDL_GameControllerAxis)i);
	}
}

std::wstring FInput::GetClipboardText() const
{
	std::wstring Result;

	char *ClipboardText = SDL_GetClipboardText();
	Result = FStringUtils::Widen(ClipboardText);
	SDL_free(ClipboardText);

	return Result;
}

void FInput::CopyToClipboard(const std::vector<std::wstring>& Lines) const
{
	std::string ClipboardText;
	for (int i = 0; i < (int)Lines.size(); ++i)
	{
		ClipboardText += FStringUtils::Narrow(Lines[i]);
		if (i < (int)Lines.size() - 1)
		{
			ClipboardText += "\n";
		}
	}
	SDL_SetClipboardText(ClipboardText.c_str());
}

FUIEvent FInput::ProcessSDLEvent(const SDL_Event& SDLEvent)
{
	switch (SDLEvent.type)
	{
		case SDL_TEXTINPUT:
			return FUIEvent(SDLEvent.text.text);

		case SDL_KEYDOWN:
		{
			SDL_Scancode ScanCode = SDLEvent.key.keysym.scancode;
			SDL_Keycode KeyCode = SDLEvent.key.keysym.sym;

			bool bIsCopyPasteMod = SDL_GetModState() & KMOD_CTRL;
#ifdef __APPLE__
			bIsCopyPasteMod = bIsCopyPasteMod || SDL_GetModState() & KMOD_GUI;
#endif

			if (KeyCode == SDLK_c && bIsCopyPasteMod)
			{
				return FUIEvent(EUIEventType::CopyText);
			}
			else if (KeyCode == SDLK_a && bIsCopyPasteMod)
			{
				return FUIEvent(EUIEventType::SelectAll);
			}
			else if (KeyCode == SDLK_v && bIsCopyPasteMod)
			{
				return FUIEvent(EUIEventType::PasteText);
			}
			else if (KeyCode == SDLK_f && bIsCopyPasteMod)
			{
				return FUIEvent(EUIEventType::SearchText);
			}
			else if (KeyCode == SDLK_RETURN && SDL_GetModState() & KMOD_ALT)
			{
				return FUIEvent(EUIEventType::FullscreenToggle);
			}
			else
			{
				return FUIEvent(EUIEventType::KeyPressed, static_cast<FInput::Keys>(ScanCode));
			}
		}

		case SDL_KEYUP:
			return FUIEvent(EUIEventType::KeyReleased, static_cast<FInput::Keys>(SDLEvent.key.keysym.scancode));

		case SDL_MOUSEBUTTONDOWN:
		{
			if (SDLEvent.button.button == SDL_BUTTON_LEFT)
			{
				return FUIEvent(EUIEventType::MousePressed, Vector2(float(SDLEvent.button.x), float(SDLEvent.button.y)));
			}
			break;
		}

		case SDL_MOUSEBUTTONUP:
		{
			if (SDLEvent.button.button == SDL_BUTTON_LEFT)
			{
				return FUIEvent(EUIEventType::MouseReleased, Vector2(float(SDLEvent.button.x), float(SDLEvent.button.y)));
			}
			break;
		}

		case SDL_MOUSEWHEEL:
		{
			UpdateMouseWheel(SDLEvent.wheel.y);

			if (SDLEvent.wheel.y != 0)
			{
				return FUIEvent(EUIEventType::MouseWheelScrolled, Vector2(0.0f, float(SDLEvent.wheel.y)));
			}
			break;
		}

		case SDL_CONTROLLERDEVICEADDED:
		{
			if (GameController == nullptr)
			{
				WhichController = SDLEvent.cdevice.which;
				GameController = SDL_GameControllerOpen(WhichController);

				memset(ButtonStates, 0, sizeof(Uint8)*SDL_CONTROLLER_BUTTON_MAX);
				memset(ButtonStatesPrev, 0, sizeof(Uint8)*SDL_CONTROLLER_BUTTON_MAX);
				memset(AxisValues, 0, sizeof(float)*SDL_CONTROLLER_AXIS_MAX);
			}
			break;
		}

		case SDL_CONTROLLERDEVICEREMOVED:
		{
			if (WhichController == SDLEvent.cdevice.which)
			{
				WhichController = -1;
				GameController = nullptr;
			}
			break;
		}
		default:
			break;
	}

	return FUIEvent(EUIEventType::None);
}

bool FInput::IsKeyPressed(FInput::InputCommands InputCommand)
{
	auto KeyBindingPair = KeyBindings.find(int(InputCommand));
	if (KeyBindingPair != KeyBindings.end())
	{
		return IsKeyPressed(static_cast<FInput::Keys>(KeyBindingPair->second));
	}

	return false;
}

bool FInput::IsKeyHeld(FInput::InputCommands InputCommand)
{
	auto KeyBindingPair = KeyBindings.find(int(InputCommand));
	if (KeyBindingPair != KeyBindings.end())
	{
		return IsKeyHeld(static_cast<FInput::Keys>(KeyBindingPair->second));
	}

	return false;
}

bool FInput::IsKeyReleased(InputCommands InputCommand)
{
	auto KeyBindingPair = KeyBindings.find(int(InputCommand));
	if (KeyBindingPair != KeyBindings.end())
	{
		return IsKeyReleased(static_cast<FInput::Keys>(KeyBindingPair->second));
	}

	return false;
}

bool FInput::IsKeyPressed(FInput::Keys Key) const
{
	return (KeyboardState[Key] == 1 && PreviousKeyboardState[Key] == 0);
}

bool FInput::IsKeyHeld(FInput::Keys Key) const
{
	return KeyboardState[Key] == 1;
}

bool FInput::IsKeyReleased(FInput::Keys Key) const
{
	return (KeyboardState[Key] == 0 && PreviousKeyboardState[Key] == 1);
}

bool FInput::IsCapsKeyDown() const
{
	return IsKeyPressed(Keys::LeftShift) ||
		IsKeyPressed(Keys::RightShift) ||
		IsKeyPressed(Keys::CapsLock);
}

bool FInput::IsAltKeyDown() const
{
	return IsKeyPressed(Keys::LeftAlt) ||
		IsKeyPressed(Keys::RightAlt);
}

bool FInput::IsAnyKeyPressed() const
{
	for (size_t i = Keys::None + 1; i <= static_cast<size_t>(Keys::OemClear); ++i)
	{
		if (IsKeyPressed(static_cast<Keys>(i)))
		{
			return true;
		}
	}

	return false;
}

Vector2 FInput::GetMousePosition() const
{
	return MousePos;
}

Vector2 FInput::GetMousePositionDiff() const
{
	return MousePos - PreviousMousePos;
}

bool FInput::IsMouseButtonPressed(FInput::InputCommands InputCommand)
{
	auto MouseBindingPair = MouseBindings.find(int(InputCommand));
	if (MouseBindingPair != MouseBindings.end())
	{
		auto MouseBindingPair = MouseBindings.find(int(InputCommand));
		if (MouseBindingPair != MouseBindings.end())
		{
			MouseButtons MouseBinding = static_cast<MouseButtons>(MouseBindingPair->second);
			switch (MouseBinding)
			{
			case MouseButtons::Left:
			{
				return IsMouseButtonPressed(SDL_BUTTON_LEFT);
			}
			case MouseButtons::Right:
			{
				return IsMouseButtonPressed(SDL_BUTTON_RIGHT);
			}
			case MouseButtons::Middle:
			{
				return IsMouseButtonPressed(SDL_BUTTON_MIDDLE);
			}
			default:
				break;
			}
		}
	}

	return false;
}

bool FInput::IsMouseButtonReleased(FInput::InputCommands InputCommand)
{
	auto MouseBindingPair = MouseBindings.find(int(InputCommand));
	if (MouseBindingPair != MouseBindings.end())
	{
		auto MouseBindingPair = MouseBindings.find(int(InputCommand));
		if (MouseBindingPair != MouseBindings.end())
		{
			MouseButtons MouseBinding = static_cast<MouseButtons>(MouseBindingPair->second);
			switch (MouseBinding)
			{
			case MouseButtons::Left:
			{
				return IsMouseButtonReleased(SDL_BUTTON_LEFT);
			}
			case MouseButtons::Right:
			{
				return IsMouseButtonReleased(SDL_BUTTON_RIGHT);
			}
			case MouseButtons::Middle:
			{
				return IsMouseButtonReleased(SDL_BUTTON_MIDDLE);
			}
			default:
				break;
			}
		}
	}

	return false;
}

bool FInput::IsMouseButtonPressed(const Uint32 Button) const
{
	return ((SDL_BUTTON(Button) & MouseState) != 0) && ((SDL_BUTTON(Button) & PreviousMouseState) == 0);
}

bool FInput::IsMouseButtonHeld(const Uint32 Button) const
{
	return (SDL_BUTTON(Button) & MouseState) != 0;
}

bool FInput::IsMouseButtonReleased(const Uint32 Button) const
{
	return ((SDL_BUTTON(Button) & MouseState) == 0) && ((SDL_BUTTON(Button) & PreviousMouseState) != 0);
}

int FInput::GetMouseScrollWheelLines()
{
	return MouseWheelPosY;
}

void FInput::UpdateMouseWheel(Sint32 WheelPosY)
{
	MouseWheelPosY = std::abs(WheelPosY);
}

bool FInput::IsGamePadButtonPressed(FInput::InputCommands InputCommand)
{
	auto GamePadBindingPair = GamePadBindings.find(int(InputCommand));
	if (GamePadBindingPair != GamePadBindings.end())
	{
		GamePadButtons GamePadBinding = static_cast<GamePadButtons>(GamePadBindingPair->second);
		switch (GamePadBinding)
		{
		case GamePadButtons::A:
			return IsGamePadButtonPressed(SDL_CONTROLLER_BUTTON_A);
		case GamePadButtons::B:
			return IsGamePadButtonPressed(SDL_CONTROLLER_BUTTON_B);
		case GamePadButtons::X:
			return IsGamePadButtonPressed(SDL_CONTROLLER_BUTTON_X);
		case GamePadButtons::Y:
			return IsGamePadButtonPressed(SDL_CONTROLLER_BUTTON_Y);

		case GamePadButtons::LeftStick:
			return IsGamePadButtonPressed(SDL_CONTROLLER_BUTTON_LEFTSTICK);
		case GamePadButtons::RightStick:
			return IsGamePadButtonPressed(SDL_CONTROLLER_BUTTON_RIGHTSTICK);

		case GamePadButtons::LeftShoulder:
			return IsGamePadButtonPressed(SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
		case GamePadButtons::RightShoulder:
			return IsGamePadButtonPressed(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);

		case GamePadButtons::Back:
			return IsGamePadButtonPressed(SDL_CONTROLLER_BUTTON_BACK);
		case GamePadButtons::View:
			return IsGamePadButtonPressed(SDL_CONTROLLER_BUTTON_GUIDE);
		case GamePadButtons::Start:
			return IsGamePadButtonPressed(SDL_CONTROLLER_BUTTON_START);

		case GamePadButtons::DPadUp:
			return IsGamePadButtonPressed(SDL_CONTROLLER_BUTTON_DPAD_UP);
		case GamePadButtons::DPadDown:
			return IsGamePadButtonPressed(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
		case GamePadButtons::DPadLeft:
			return IsGamePadButtonPressed(SDL_CONTROLLER_BUTTON_DPAD_LEFT);
		case GamePadButtons::DPadRight:
			return IsGamePadButtonPressed(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);

		default:
			break;
		}
	}

	return false;
}

bool FInput::IsGamePadButtonPressed(const SDL_GameControllerButton Button) const
{
	return (ButtonStates[Button] == 1 && ButtonStatesPrev[Button] == 0);
}

bool FInput::IsGamePadButtonHeld(const SDL_GameControllerButton Button) const
{
	return (ButtonStates[Button] == 1);
}

bool FInput::IsGamePadButtonReleased(const SDL_GameControllerButton Button) const
{
	return (ButtonStates[Button] == 0 && ButtonStatesPrev[Button] == 1);
}

float FInput::GetGamePadAxisValue(const SDL_GameControllerAxis Axis) const
{
	return AxisValues[Axis];
}

#endif //EOS_DEMO_SDL
