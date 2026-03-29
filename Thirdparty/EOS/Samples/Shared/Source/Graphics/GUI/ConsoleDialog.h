// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Dialog.h"
#include "Font.h"

/**
 * Forward declarations
 */
class FTextFieldWidget;
class FTextViewWidget;
class FTextLabelWidget;
class FButtonWidget;
class FUIEvent;
class FConsole;
class FSpriteWidget;

/**
 * Console dialog
 */
class FConsoleDialog : public FDialog
{
public:
	/**
	 * Constructor
	 */
	FConsoleDialog(Vector2 ConsolePos,
		Vector2 ConsoleSize,
		UILayer ConsoleLayer,
		std::weak_ptr<FConsole> Console,
		FontPtr ConsoleNormalFont,
		FontPtr ConsoleLargeFont,
		FontPtr ConsoleLargestFont,
		FontPtr ConsoleButtonFont,
		FontPtr ConsoleTitleFont);

	/**
	 * Destructor
	 */
	virtual ~FConsoleDialog() {};

	/** IGfxComponent */
	virtual void Create() override;
	virtual void Update() override;
	virtual void Render(FSpriteBatchPtr& Batch) override;

	/** IWidget */
	virtual void OnUIEvent(const FUIEvent& event) override;
	virtual void SetPosition(Vector2 Pos) override;
	virtual void SetSize(Vector2 NewSize) override;
	virtual bool CheckCollision(Vector2 Pos) const override;

private:
	/**
	 * Sets the font to use for text view based on size of console
	 */
	void SetTextViewFont();

	/** Console */
	std::weak_ptr<FConsole> Console;

	/** Background texture */
	std::shared_ptr<FSpriteWidget> Background;

	/** Clear Button */
	std::shared_ptr<FButtonWidget> ClearButton;
	
	/** Input Field */
	std::shared_ptr<FTextFieldWidget> TextInputField;
	
	/** Text View */
	std::shared_ptr<FTextViewWidget> TextView;

	/** Title Label */
	std::shared_ptr<FTextLabelWidget> TitleLabel;

	/** Search label */
	std::shared_ptr<FTextLabelWidget> SearchLabel;

	/** Search icon */
	std::shared_ptr<FSpriteWidget> SearchSprite;

	/** Search Input Field */
	std::shared_ptr<FTextFieldWidget> SearchInputField;

	/** Not found label */
	std::shared_ptr<FTextLabelWidget> NotFoundLabel;

	/** Normal Font */
	FontPtr NormalFont;

	/** Large Font */
	FontPtr LargeFont;

	/** Largest font */
	FontPtr LargestFont;

	/** Button font */
	FontPtr ButtonFont;

	/** Title font */
	FontPtr TitleFont;

	/** Stores number of lines in console */
	size_t NumConsoleLinesLastTime = 0;

	/** How many lines were dropped according to data from the last time we checked. */
	size_t NumLinesDroppedTotalLastTime = 0;
};
