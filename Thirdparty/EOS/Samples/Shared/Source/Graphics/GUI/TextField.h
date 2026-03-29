// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widget.h"
#include "Font.h"

class FTextLabelWidget;

/**
 * Text field widget that handles user input
 */
class FTextFieldWidget : public IWidget
{
public:
	/** Input Type */
	enum class EInputType
	{
		None = 0,
		Normal,
		Email,
		Password,
		Total
	};

	/**
	 * Constructor
	 */
	FTextFieldWidget(Vector2 TextFieldPosition,
		Vector2 TextFieldSize,
		UILayer TextFieldLayer,
		const std::wstring& InitialText,
		const std::wstring& TextureFile,
		FontPtr TextFieldFont,
		EInputType InputType = EInputType::Normal,
		EAlignmentType AlignmentType = EAlignmentType::Center,
		FColor BackCol = FColor(1.f, 1.f, 1.f, 1.f),
		FColor TextCol = FColor(1.f, 1.f, 1.f, 1.f));

	/**
	* Destructor
	*/
	virtual ~FTextFieldWidget() {};

	/** IGfxComponent */
	virtual void Create() override;
	virtual void Release() override;
	virtual void Update() override;
	virtual void Render(FSpriteBatchPtr& Batch) override;

	/** IWidget */
	virtual void OnUIEvent(const FUIEvent& Event) override;
	virtual void SetPosition(Vector2 Pos) override;
	virtual void SetSize(Vector2 NewSize) override;
	virtual void SetFocused(bool bValue) override;

	/** Clear */
	void Clear();

	/** Callback that is called when enter key is pressed */
	void SetOnEnterPressedCallback(std::function<void(const std::wstring&)> Callback);

	/**
	 * Sets font to use to display text string
	 *
	 * @param NewFont - Font to use for text
	 */
	void SetFont(const FontPtr& NewFont);

	/** Gets current text */
	const std::wstring GetText() const { return Text; }

	/** Sets current text */
	void SetText(std::wstring NewText)
	{
		Text = NewText;
		if (CursorPosition > Text.size())
		{
			CursorPosition = Text.size();
		}
	}

	/** Current cursor position */
	size_t GetCursorPosition() const { return CursorPosition; }

	/** Change cursor position */
	void SetCursorPosition(size_t NewPosition)
	{
		CursorPosition = NewPosition;
		if (CursorPosition > Text.size())
		{
			CursorPosition = Text.size();
		}
	}

	/** Gets initial text */
	const std::wstring GetInitialText() const { return InitialTextValue; }

	/** Sets text color */
	void SetTextColor(FColor Col) { Label->SetTextColor(Col); };

	/** Sets background color */
	void SetBackgroundColor(FColor Col) { Label->SetBackgroundColor(Col); };

	/**
	 * Sets visibility for background image
	 *
	 * @param Visible - Visibility
	 */
	void SetBackgroundVisible(bool bVisible) { Label->SetBackgroundVisible(bVisible); };

	/**
	 * Gets visibility for background image
	 *
	 * @return True if background image is visible
	 */
	bool IsBackgroundVisible() { return Label->IsBackgroundVisible(); };

	/** Set horizontal offset */
	void SetHorizontalOffset(float Value);

	/** Insert text at cursor position */
	void InsertTextAtCursor(std::wstring&& Text);

protected:
	/** Updates pasting text from clipboard */
	void UpdatePasteText();

	/** Updates caret */
	void UpdateCaret();

	/** Updates text */
	void UpdateText();

	/** Returns true if char is a alphabet input character */
	bool IsAlphaCharacter(WCHAR inputChar);

	/** Returns true if char is a numeric input character */
	bool IsNumericCharacter(WCHAR inputChar);

	/** Returns true if char is a special input character */
	bool IsSpecialInputCharacter(WCHAR inputChar);

	/** Label */
	std::shared_ptr<FTextLabelWidget> Label;

	/** Input Type */
	EInputType InputType = EInputType::Normal;

	/** Initial text value */
	std::wstring InitialTextValue;

	/** Text */
	std::wstring Text;

	/** Visible text */
	std::wstring VisibleText;

	/** Cursor position */
	size_t CursorPosition = 0;

	/** Callback called when enter key is pressed */
	std::function<void(const std::wstring&)> OnEnterPressedCallback;

	/** Timer used to manage blinking caret */
	float CaretBlinkTimer;

	/** Time to wait between caret blinks */
	static constexpr float CaretBlinkTime = 0.5f;

	/** True if caret is currently visible */
	bool bCaretVisible;
};