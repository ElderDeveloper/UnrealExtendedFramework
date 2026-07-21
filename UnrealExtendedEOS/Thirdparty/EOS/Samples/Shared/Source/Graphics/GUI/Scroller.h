// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widget.h"

class IScrollable
{
public:
	virtual ~IScrollable() {}

	virtual void ScrollUp(size_t length = 1) = 0;
	virtual void ScrollDown(size_t length = 1) = 0;
	virtual void ScrollToTop() = 0;
	virtual void ScrollToBottom() = 0;

	virtual size_t NumEntries() const = 0;
	virtual size_t GetNumLinesPerPage() const = 0;
	virtual size_t FirstViewedEntry() const = 0;
	virtual size_t LastViewedEntry() const = 0;
};

/** Forward declarations */
class FButtonWidget;
class FSpriteWidget;

class FScroller : public IWidget
{
public:
	/**
	 * Constructor
	 */
	FScroller(std::weak_ptr<IScrollable> scrollable, Vector2 position, Vector2 size, UILayer layer, std::wstring BarTextureFile);

	/**
	 * Destructor
	 */
	virtual ~FScroller() {};

	/** IGfxComponent */
	virtual void Create() override;
	virtual void Release() override;
	virtual void Update() override;
	virtual void Render(FSpriteBatchPtr& Batch) override;

	/** IWidget */
	virtual void OnUIEvent(const FUIEvent& event) override;
	virtual void SetPosition(Vector2 Pos) override;
	virtual void SetSize(Vector2 NewSize) override;

	/**
	 * Scroll Up
	 */
	void ScrollUp(size_t NumLines = 1);

	/**
	 * Scroll Down
	 */
	void ScrollDown(size_t NumLines = 1);

	/**
	 * Scroll to Top of list
	 */
	void ScrollToTop();

	/**
	 * Scroll top Bottom of list
	 */
	void ScrollToBottom();


private:
	/** True if scroll bar is being dragged with mouse */
	bool bBeingDraggedWithMouse = false;

	/** Y position offset when scroll bar is first clicked */
	float DraggingInitialClickOffset = 0.0f;

	/** Height of scroll bar */
	float BarHeight;

	/** Y position of scroll bar */
	float BarPosY;

	/** Total height for scroll bar to scroll in */
	float TotalScrollingHeight;

	/** Scroll Bar Image */
	std::shared_ptr<FSpriteWidget> ScrollBarImage;

	/** Scrollable */
	std::weak_ptr<IScrollable> Scrollable;

	/** Scroll Up Button */
	std::unique_ptr<FButtonWidget> ScrollUpButton;

	/** Scroll Down Button */
	std::unique_ptr<FButtonWidget> ScrollDownButton;
};
