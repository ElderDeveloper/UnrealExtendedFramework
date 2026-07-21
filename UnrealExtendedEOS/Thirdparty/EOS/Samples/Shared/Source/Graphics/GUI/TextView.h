// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widget.h"
#include "Scroller.h"
#include "Sprite.h"
#include "Font.h"
#include "Console.h"
#include "Utils/CircularBuffer.h"


using FColoredLine = FConsoleLine;

/**
 * Text view widget that can view large amount of scrollable text
 */
class FTextViewWidget : public IWidget, public IScrollable
{
public:
	/**
	 * Constructor
	 */
	FTextViewWidget(Vector2 TextViewPos,
		Vector2 TextViewSize,
		UILayer TextViewLayer,
		const std::wstring& InitialText,
		const std::wstring& TextViewTextureFile,
		FontPtr TextViewFont,
		FColor BackCol = FColor(0.f, 0.f, 0.f, 1.f),
		FColor TextCol = FColor(1.f, 1.f, 1.f, 1.f));

	/**
	 * Destructor
	 */
	virtual ~FTextViewWidget() {};

	/** IGfxComponent */
	virtual void Create() override;
	virtual void Release() override;
	virtual void Update() override;
	virtual void Render(FSpriteBatchPtr& Batch) override;

	/** IWidget */
	virtual void OnUIEvent(const FUIEvent& event) override;
	virtual void SetPosition(Vector2 Pos) override;
	virtual void SetSize(Vector2 NewSize) override;
	virtual void SetFocused(bool bFocused) override;

	/** IScrollable */
	virtual void ScrollUp(size_t Length) override;
	virtual void ScrollDown(size_t Length) override;
	virtual void ScrollToTop() override;
	virtual void ScrollToBottom() override;
	virtual size_t NumEntries() const override;
	virtual size_t GetNumLinesPerPage() const override;
	virtual size_t FirstViewedEntry() const override;
	virtual size_t LastViewedEntry() const override;
	
	/** Returns true if scrolling is available and text can be scrolled */
	bool CanScroll() const { return Scroller && bScrollable && !bScrollerDisabled; }

	/** Updates number of lines of text that can be rendered */
	void UpdateNumLinesPerPage();

	/** Rests text view */
	void Reset();

	/** Clears all lines of text */
	void Clear(bool bKeepCursor = false);

	/** Adds a new line of text */
	void AddLine(const std::wstring& line, FColor = Color::White);

	/** Gets text from a line */
	std::wstring GetLine(size_t LineIndex);

	/** Delete a number of lines at the beginning */
	void DropFirstLines(size_t NumLinesToDrop);

	/** Perform text search. Scroll and select search results. Returns true if found. */
	bool Search(const std::wstring& SearchText);

	/** Return from search mode. */
	void StopSearch();

	/** Should we autoscroll on add? */
	bool IsAutoScrolling() const;

	/** Sets border offsets */
	void SetBorderOffsets(Vector2 InOffsets)
	{
		BorderOffsets = Vector2(ceilf(InOffsets.x), ceilf(InOffsets.y));
	}

	/** Sets offsets for scroll bar */
	void SetScrollerOffsets(Vector2 InOffsets)
	{
		ScrollerOffsets = Vector2(ceilf(InOffsets.x), ceilf(InOffsets.y));
	}

	/** Sets background visibility */
	void SetBackgroundVisible(bool bIsVisible) { bIsBackgroundVisible = bIsVisible; };

	/** Sets whether text can be selected */
	void SetCanSelectText(bool bCanSelect) { bCanSelectText = bCanSelect; };

	/** Gets initial text value */
	const std::wstring& GetInitialText() const { return InitialTextValue; }

	/**
	* Sets font to use to display text string
	*
	* @param NewFont - Font to use for text
	*/
	void SetFont(FontPtr NewFont) { Font = NewFont; };

	/** 
	* Select all text in text view.
	*/
	void SelectAllText();

	/** Disable Scroller */
	void DisableScroller() { bScrollerDisabled = true; };

	/** Enable Scroller */
	void EnableScroller();

	/** Returns true if we have only one line that matches the initial text */
	bool HasInitialText() const;

	/** Returns true if we have no text lines */
	bool IsEmpty() const;

protected:
	/** Create Scroller */
	void CreateScroller();

	/** Wraps long lines over multiple lines */
	void WrapLine(const std::wstring& Line, float LineWidth, std::vector<std::wstring>& OutWrappedLines);

	/**
	 * Marks widget as dirty which can lead to rendering objects regeneration
	 */
	void MarkDirty();

	/** Gets relative position on screen for a line */
	size_t ScreenPositionToLineNum(Vector2 ScreenPos) const;

	/** Background image */
	std::shared_ptr<FSpriteWidget> BackgroundImage;

	/** Background image for text area */
	std::shared_ptr<FSpriteWidget> TextBackgroundImage;

	/** Initial text */
	std::wstring InitialTextValue;

	/** Collection of text, one entry per line (circular buffer) */
	TCircularBuffer<FColoredLine> Lines;

	/** File to use for background image */
	std::wstring TextureFile;

	/** Border offsets */
	Vector2 BorderOffsets;

	/** Position offsets for drawing scroll bar */
	Vector2 ScrollerOffsets;

	/** Text font */
	FontPtr Font;

#ifdef EOS_DEMO_SDL
	std::vector<FTexturePtr> FontTextures;
#endif

	/** Text color */
	FColor TextCol = FColor(1.f, 1.f, 1.f, 1.f);

	/** Scroll bar */
	std::unique_ptr<FScroller> Scroller;

	/** Number of lines for each page */
	size_t NumLinesPerPage = 0;

	/** Index of the first viewed line */
	size_t FirstViewedLine = 0;

	/** True if scroller is disabled for this text view */
	bool bScrollerDisabled = false;

	/** True if the text can be scrolled */
	bool bScrollable = false;

	/** Index of first selected line */
	size_t FirstSelectedLine = 0;

	/** Index of last selected line */
	size_t LastSelectedLine = 0;

	/** True if we can select text */
	bool bSelectionEnabled = false;

	/** True if we are currently selecting lines of text */
	bool bSelectingLines = false;

	/** True if current text selection direction is upwards */
	bool bSelectingUp = false;

	/** True if text can be selected */
	bool bCanSelectText = true;

	/** Currently performing text search. */
	bool bSearching = false;

	/** True if enabled */
	bool bEnabled = true;

	/** True if background is visible */
	bool bIsBackgroundVisible = true;

	/** Were there any changes since last render? */
	bool bDirtyFlag = true;

	/** Width of scroll bar */
	static constexpr float ScrollerWidth = 10.0f;

	/** Spacing between lines */
	static constexpr float LineSpacing = 3.f;
};
