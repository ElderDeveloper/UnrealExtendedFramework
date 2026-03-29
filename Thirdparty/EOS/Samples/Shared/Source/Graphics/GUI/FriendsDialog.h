// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Dialog.h"
#include "Font.h"

/**
 * Forward declarations
 */
class FFriendListWidget;
class FGameEvent;

/**
 * Friends dialog
 */
class FFriendsDialog : public FDialog
{
public:
	/**
	 * Constructor
	 */
	FFriendsDialog(Vector2 DialogPos,
		Vector2 DialogSize,
		UILayer DialogLayer,
		FontPtr DialogNormalFont,
		FontPtr DialogSmallFont,
		FontPtr DialogTinyFont);

	/**
	 * Destructor
	 */
	virtual ~FFriendsDialog() {};

	/** IWidget */
	virtual void SetPosition(Vector2 Pos) override;
	virtual void SetSize(Vector2 NewSize) override;

	/**
	* Receives game event
	*
	* @param Event - Game event to act on
	*/
	void OnGameEvent(const FGameEvent& Event);

	/** Sets visibility of friend info */
	void SetFriendInfoVisible(bool bVisible);

	/** Reset */
	void Reset();

	/** Clear */
	void Clear();
	
private:
	/** Friends List */
	std::shared_ptr<FFriendListWidget> FriendsListWidget;
};
