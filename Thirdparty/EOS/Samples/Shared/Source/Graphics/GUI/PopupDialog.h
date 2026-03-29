// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Dialog.h"
#include "Font.h"

/**
 * Forward declarations
 */
class FButtonWidget;
class FTextViewWidget;

/**
 * Generic popup dialog with OK button
 */
class FPopupDialog : public FDialog
{
public:
	/**
	 * Constructor
	 */
	FPopupDialog(Vector2 DialogPos,
		Vector2 DialogSize,
		UILayer DialogLayer,
		const std::wstring& LabelText, 
		FontPtr DialogNormalFont,
		FontPtr DialogSmallFont);

	/**
	 * Destructor
	 */
	virtual ~FPopupDialog() {};

	/** IWidget */
	virtual void SetPosition(Vector2 Pos) override;

	void SetText(const std::wstring& NewText);

private:
	/** Dialog Background */
	WidgetPtr Background;

	/** Message text view */
	std::shared_ptr<FTextViewWidget> MessageTextView;

	/** Dialog Ok Button */
	std::shared_ptr<FButtonWidget> OkButton;
};
