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
 * Notification dialog
 */
class FNotificationDialog : public FDialog
{
public:
	/**
	 * Constructor
	 */
	FNotificationDialog(Vector2 DialogPos,
		Vector2 DialogSize,
		UILayer DialogLayer,
		FontPtr DialogNormalFont,
		FontPtr DialogSmallFont);

	/**
	 * Destructor
	 */
	virtual ~FNotificationDialog() {};

	/**
	* IGfxComponent Overrides
	*/
	virtual void Update() override;

	/** IWidget */
	virtual void SetPosition(Vector2 Pos) override;

	/**
	* Receives game event
	*
	* @param Event - Game event to act on
	*/
	void OnGameEvent(const FGameEvent& Event);

private:
	/** Dialog Background */
	WidgetPtr Background;

	/** Dialog Label */
	std::shared_ptr<FTextLabelWidget> Label;

	/** Queued up notifications */
	std::queue<std::wstring> NotificationQueue;

	float UpdateNotificationsTime = 3.f;

	/** Update Notifications Timer */
	float UpdateNotificationsTimer;

	/** Updated queued notifications */
	void UpdateNotifications();

	/** Shows next queued notification */
	void ShowNextNotification();

	/** Set Label Text */
	void SetLabelText(const std::wstring& LabelText);

	/** Set Label Text Color */
	void SetLabelTextColor(FColor Col);
};
