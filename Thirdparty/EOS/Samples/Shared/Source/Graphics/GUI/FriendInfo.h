// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widget.h"
#include "Font.h"

/**
* Forward declarations
*/
class FUIEvent;
class FButtonWidget;
class FTextLabelWidget;
class FSpriteWidget;

/**
* Friends info widget
*/
class FFriendInfoWidget : public IWidget
{
public:
	/**
	 * Constructor
	 */
	FFriendInfoWidget(Vector2 InfoPos,
		Vector2 InfoSize,
		UILayer InfoLayer,
		FFriendData FriendData,
		FontPtr InfoLargeFont,
		FontPtr InfoSmallFont);

	/**
	* No copying or copy assignment allowed for this class.
	*/
	FFriendInfoWidget(const FFriendInfoWidget&) = delete;
	FFriendInfoWidget& operator=(const FFriendInfoWidget&) = delete;

	/**
	 * Destructor
	 */
	virtual ~FFriendInfoWidget() {};

	/** IGfxComponent */
	virtual void Create() override;
	virtual void Release() override;
	virtual void Update() override;
	virtual void Render(FSpriteBatchPtr& Batch) override;
#ifdef _DEBUG
	virtual void DebugRender() override;
#endif

	/** IWidget */
	virtual void OnUIEvent(const FUIEvent& Event) override;
	virtual void SetPosition(Vector2 Pos) override;
	virtual void SetSize(Vector2 NewSize) override;
	virtual void SetFocused(bool bFocused) override;

	/**
	 * Sets friend data
	 *
	 * @param Data - Friend data to copy
	 */
	void SetFriendData(const FFriendData& Data);

private:
	/** Friend Data */
	FFriendData FriendData;

	/** Background Image */
	std::shared_ptr<FSpriteWidget> BackgroundImage;

	/** Name Label */
	std::shared_ptr<FTextLabelWidget> NameLabel;

	/** Button one */
	std::shared_ptr<FButtonWidget> Button1;

	/** Button two */
	std::shared_ptr<FButtonWidget> Button2;

	/** Button three */
	std::shared_ptr<FButtonWidget> Button3;

	/** Large Font */
	FontPtr LargeFont;

	/** Small Font */
	FontPtr SmallFont;
};
