// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef EOS_DEMO_SDL

class FUIEvent;

class FInput
{
public:

	enum Keys
	{
		None = 0,

		Back = SDL_SCANCODE_BACKSPACE,
		Tab = SDL_SCANCODE_TAB,

		Enter = SDL_SCANCODE_RETURN,

		Pause = SDL_SCANCODE_PAUSE,
		CapsLock = SDL_SCANCODE_CAPSLOCK,
		//Kana = SDL_SCANCODE_??,

		//Kanji = SDL_SCANCODE_??,

		Escape = SDL_SCANCODE_ESCAPE,
		//ImeConvert = SDL_SCANCODE_??,
		//ImeNoConvert = SDL_SCANCODE_??,

		Space = SDL_SCANCODE_SPACE,
		PageUp = SDL_SCANCODE_PAGEUP,
		PageDown = SDL_SCANCODE_PAGEDOWN,
		End = SDL_SCANCODE_END,
		Home = SDL_SCANCODE_HOME,
		Left = SDL_SCANCODE_LEFT,
		Up = SDL_SCANCODE_UP,
		Right = SDL_SCANCODE_RIGHT,
		Down = SDL_SCANCODE_DOWN,
		Select = SDL_SCANCODE_SELECT,
		//Print = SDL_SCANCODE_??,
		Execute = SDL_SCANCODE_EXECUTE,
		PrintScreen = SDL_SCANCODE_PRINTSCREEN,
		Insert = SDL_SCANCODE_INSERT,
		Delete = SDL_SCANCODE_DELETE,
		Help = SDL_SCANCODE_HELP,
		D0 = SDL_SCANCODE_0,
		D1 = SDL_SCANCODE_1,
		D2 = SDL_SCANCODE_2,
		D3 = SDL_SCANCODE_3,
		D4 = SDL_SCANCODE_4,
		D5 = SDL_SCANCODE_5,
		D6 = SDL_SCANCODE_6,
		D7 = SDL_SCANCODE_7,
		D8 = SDL_SCANCODE_8,
		D9 = SDL_SCANCODE_9,

		A = SDL_SCANCODE_A,
		B = SDL_SCANCODE_B,
		C = SDL_SCANCODE_C,
		D = SDL_SCANCODE_D,
		E = SDL_SCANCODE_E,
		F = SDL_SCANCODE_F,
		G = SDL_SCANCODE_G,
		H = SDL_SCANCODE_H,
		I = SDL_SCANCODE_I,
		J = SDL_SCANCODE_J,
		K = SDL_SCANCODE_K,
		L = SDL_SCANCODE_L,
		M = SDL_SCANCODE_M,
		N = SDL_SCANCODE_N,
		O = SDL_SCANCODE_O,
		P = SDL_SCANCODE_P,
		Q = SDL_SCANCODE_Q,
		R = SDL_SCANCODE_R,
		S = SDL_SCANCODE_S,
		T = SDL_SCANCODE_T,
		U = SDL_SCANCODE_U,
		V = SDL_SCANCODE_V,
		W = SDL_SCANCODE_W,
		X = SDL_SCANCODE_X,
		Y = SDL_SCANCODE_Y,
		Z = SDL_SCANCODE_Z,
		LeftWindows = SDL_SCANCODE_LGUI,
		RightWindows = SDL_SCANCODE_RGUI,
		Apps = SDL_SCANCODE_APPLICATION,

		Sleep = SDL_SCANCODE_SLEEP,
		NumPad0 = SDL_SCANCODE_KP_0,
		NumPad1 = SDL_SCANCODE_KP_1,
		NumPad2 = SDL_SCANCODE_KP_2,
		NumPad3 = SDL_SCANCODE_KP_3,
		NumPad4 = SDL_SCANCODE_KP_4,
		NumPad5 = SDL_SCANCODE_KP_5,
		NumPad6 = SDL_SCANCODE_KP_6,
		NumPad7 = SDL_SCANCODE_KP_7,
		NumPad8 = SDL_SCANCODE_KP_8,
		NumPad9 = SDL_SCANCODE_KP_9,
		//Multiply = SDL_SCANCODE_??,
		//Add = SDL_SCANCODE_??,
		Separator = SDL_SCANCODE_SEPARATOR,
		Subtract = SDL_SCANCODE_MINUS,

		Decimal = SDL_SCANCODE_DECIMALSEPARATOR,
		Divide = SDL_SCANCODE_SLASH,
		F1 = SDL_SCANCODE_F1,
		F2 = SDL_SCANCODE_F2,
		F3 = SDL_SCANCODE_F3,
		F4 = SDL_SCANCODE_F4,
		F5 = SDL_SCANCODE_F5,
		F6 = SDL_SCANCODE_F6,
		F7 = SDL_SCANCODE_F7,
		F8 = SDL_SCANCODE_F8,
		F9 = SDL_SCANCODE_F9,
		F10 = SDL_SCANCODE_F10,
		F11 = SDL_SCANCODE_F11,
		F12 = SDL_SCANCODE_F12,
		F13 = SDL_SCANCODE_F13,
		F14 = SDL_SCANCODE_F14,
		F15 = SDL_SCANCODE_F15,
		F16 = SDL_SCANCODE_F16,
		F17 = SDL_SCANCODE_F17,
		F18 = SDL_SCANCODE_F18,
		F19 = SDL_SCANCODE_F19,
		F20 = SDL_SCANCODE_F20,
		F21 = SDL_SCANCODE_F21,
		F22 = SDL_SCANCODE_F22,
		F23 = SDL_SCANCODE_F23,
		F24 = SDL_SCANCODE_F24,

		NumLock = SDL_SCANCODE_NUMLOCKCLEAR,
		Scroll = SDL_SCANCODE_SCROLLLOCK,

		LeftShift = SDL_SCANCODE_LSHIFT,
		RightShift = SDL_SCANCODE_RSHIFT,
		LeftControl = SDL_SCANCODE_LCTRL,
		RightControl = SDL_SCANCODE_RCTRL,
		LeftAlt = SDL_SCANCODE_LALT,
		RightAlt = SDL_SCANCODE_RALT,

		OemSemicolon = SDL_SCANCODE_SEMICOLON,
//		OemPlus = SDL_SCANCODE_PLUS,
		OemComma = SDL_SCANCODE_COMMA,
		OemMinus = SDL_SCANCODE_MINUS,
		OemPeriod = SDL_SCANCODE_PERIOD,
//		OemQuestion = SDL_SCANCODE_??,
		OemTilde = SDL_SCANCODE_NONUSBACKSLASH,

//		OemOpenBrackets = SDL_SCANCODE_??,
//		OemPipe = SDL_SCANCODE_??,
//		OemCloseBrackets = SDL_SCANCODE_??,
//		OemQuotes = SDL_SCANCODE_??,
		Oem8 = SDL_SCANCODE_GRAVE,

		OemBackslash = SDL_SCANCODE_BACKSLASH,

//		ProcessKey = SDL_SCANCODE_??,

//		OemCopy = 0xf2,
//		OemAuto = 0xf3,
//		OemEnlW = 0xf4,

//		Attn = 0xf6,
//		Crsel = 0xf7,
//		Exsel = 0xf8,
//		EraseEof = 0xf9,
//		Play = 0xfa,
//		Zoom = 0xfb,

//		Pa1 = 0xfd,
		OemClear = SDL_SCANCODE_CLEAR,
	};

	/** Mouse Buttons */
	enum class MouseButtons
	{
		Left = 0,
		Right,
		Middle
	};

	/** Game Pad Buttons */
	enum class GamePadButtons
	{
		A = SDL_CONTROLLER_BUTTON_A,
		B = SDL_CONTROLLER_BUTTON_B,
		X = SDL_CONTROLLER_BUTTON_X,
		Y = SDL_CONTROLLER_BUTTON_Y,

		LeftStick = SDL_CONTROLLER_BUTTON_LEFTSTICK,
		RightStick = SDL_CONTROLLER_BUTTON_RIGHTSTICK,

		LeftShoulder = SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
		RightShoulder = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,

		Back = SDL_CONTROLLER_BUTTON_BACK,
		View = SDL_CONTROLLER_BUTTON_GUIDE,

		Start = SDL_CONTROLLER_BUTTON_START,
		Menu = SDL_CONTROLLER_BUTTON_GUIDE,

		DPadUp = SDL_CONTROLLER_BUTTON_DPAD_UP,
		DPadDown = SDL_CONTROLLER_BUTTON_DPAD_DOWN,
		DPadLeft = SDL_CONTROLLER_BUTTON_DPAD_LEFT,
		DPadRight = SDL_CONTROLLER_BUTTON_DPAD_RIGHT
	};


	/**
	* Constructor
	*/
	FInput() noexcept(false);

	/**
	* No copying or copy assignment allowed for this class.
	*/
	FInput(FInput const&) = delete;
	FInput& operator=(FInput const&) = delete;

	/**
	* Destructor
	*/
	virtual ~FInput();

	/**
	* Update
	*/
	void Update();

	/**
	* Update Keyboard
	*/
	void UpdateKeyboard();

	/**
	* Update Mouse
	*/
	void UpdateMouse();

	/**
	* Update Controllers
	*/
	void UpdateControllers();

	/** Input Commands */
	enum class InputCommands
	{
		/** No command */
		None = 0,
			
		/** Exit game */
		Exit,

		/** Show / hide in-game console */
		ConsoleToggle,

		/** Show / hide in-game console (alternative) */
		ConsoleToggleAlt,

		/** Accept action for UI */
		UIAccept,

		/** UI element selected */
		UIClicked,

		/** Paste command used to paste into a UI element */
		PasteText,

		/** Select all text command */
		SelectAll,

		/** Copy selected text */
		CopyText,

		/** Perform text search. E.g. on Ctrl+F pressed. */
		SearchText,

		/** Total */
		Total
	};

	/**
	* Gets the text currently in the system clipboard
	*/
	std::wstring GetClipboardText() const;

	/**
	* Checks to see if a key defined by the given input command was just pressed
	*
	* @param InputCommand
	*/
	bool IsKeyPressed(InputCommands InputCommand);

	/**
	* Checks to see if a key defined by the given input command is in a pressed state
	*
	* @param InputCommand
	*/
	bool IsKeyHeld(InputCommands InputCommand);

	/**
	* Checks to see if a key defined by the given input command is in a released state
	*
	* @param InputCommand
	*/
	bool IsKeyReleased(InputCommands InputCommand);

	/**
	* Checks to see if a key defined by the given input command was just pressed
	*
	* @param Key
	*/
	bool IsKeyPressed(FInput::Keys Key) const;

	/**
	* Checks to see if a key defined by the given input command is in a pressed state
	*
	* @param Key
	*/
	bool IsKeyHeld(FInput::Keys Key) const;

	/**
	* Checks to see if a key defined by the given input command is in a released state
	*
	* @param Key
	*/
	bool IsKeyReleased(FInput::Keys Key) const;

	void CopyToClipboard(const std::vector<std::wstring>& Lines) const;

	static FUIEvent ProcessSDLEvent(const SDL_Event& SDLEvent);

	/** True if a caps key is pressed */
	bool IsCapsKeyDown() const;

	/** True if an alt key is pressed */
	bool IsAltKeyDown() const;

	/**
	 * Checks to see if any key on the keyboard is pressed
	 *
	 * @return True if any key is  pressed
	 */
	bool IsAnyKeyPressed() const;

	/** Get position of mouse cursor */
	Vector2 GetMousePosition() const;

	/** Get position of mouse cursor since last update */
	Vector2 GetMousePositionDiff() const;

	/**
	* Checks to see if a mouse button defined by the given input command has just been pressed
	*
	* @param InputCommand - Input command
	*
	* @return True if mouse button has just been pressed
	*/
	bool IsMouseButtonPressed(FInput::InputCommands InputCommand);

	/**
	* Checks to see if a mouse button defined by the given input command is in a pressed state
	*
	* @param InputCommand - Input command
	*
	* @return True if mouse button is pressed
	*/
	bool IsMouseButtonHeld(FInput::InputCommands InputCommand);

	/**
	* Checks to see if a mouse button defined by the given input command is in a released state
	*
	* @param InputCommand - Input command
	*
	* @return True if mouse button is released
	*/
	bool IsMouseButtonReleased(FInput::InputCommands InputCommand);

	/** Returns true if button has just been pressed */
	bool IsMouseButtonPressed(const Uint32 Button) const;

	/** Returns true if button has been pressed */
	bool IsMouseButtonHeld(const Uint32 Button) const;

	/** Returns true if button has just been released */
	bool IsMouseButtonReleased(const Uint32 Button) const;

	/**
	 * Gets the number of times the mouse scroll wheel has been scrolled this frame
	 *
	 * @return Number of mouse wheel scrolls
	 */
	static int GetMouseScrollWheelLines();

	bool IsGamePadButtonPressed(FInput::InputCommands InputCommand);

	bool IsGamePadButtonPressed(const SDL_GameControllerButton Button) const;

	bool IsGamePadButtonHeld(const SDL_GameControllerButton Button) const;

	bool IsGamePadButtonReleased(const SDL_GameControllerButton Button) const;

	float GetGamePadAxisValue(const SDL_GameControllerAxis Axis) const;

private:
	static void UpdateMouseWheel(Sint32 WheelPos);

	void ReceiveEvent(const SDL_Event& Event);

	Uint8 KeyboardState[SDL_NUM_SCANCODES];

	Uint8 PreviousKeyboardState[SDL_NUM_SCANCODES];

	Uint32 MouseState;

	Uint32 PreviousMouseState;

	Vector2 MousePos;

	Vector2 PreviousMousePos;

	/** Number of times mouse wheel has been scrolled this frame */
	static Sint32 MouseWheelPosY;

	static SDL_GameController* GameController;

	static int WhichController;

	static Uint8 ButtonStates[SDL_CONTROLLER_BUTTON_MAX];

	static Uint8 ButtonStatesPrev[SDL_CONTROLLER_BUTTON_MAX];

	static float AxisValues[SDL_CONTROLLER_AXIS_MAX];
};

#endif //EOS_DEMO_SDL