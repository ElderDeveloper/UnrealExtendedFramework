// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Dialog.h"
#include "Font.h"

/**
 * Forward declarations
 */
class FTextLabelWidget;
class FButtonWidget;

/**
 * Exit dialog
 */
class FExitDialog : public FDialog
{
public:
	/**
	 * Constructor
	 */
	FExitDialog(Vector2 DialogPos,
		Vector2 DialogSize,
		UILayer DialogLayer,
		FontPtr DialogNormalFont,
		FontPtr DialogSmallFont);

	/**
	 * Destructor
	 */
	virtual ~FExitDialog() {};

	/** IWidget */
	virtual void SetPosition(Vector2 Pos) override;

private:
	/** Exit Dialog Background */
	WidgetPtr ExitBackground;

	/** Exit Dialog Label */
	std::shared_ptr<FTextLabelWidget> ExitLabel;

	/** Exit Dialog Ok Button */
	std::shared_ptr<FButtonWidget> ExitOkButton;

	/** Exit Dialog Cancel Button */
	std::shared_ptr<FButtonWidget> ExitCancelButton;
};
