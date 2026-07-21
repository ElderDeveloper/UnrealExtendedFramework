// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widget.h"
#include "Scroller.h"
#include "Font.h"
#include "Friends.h"

/** Forward declarations */
class FTextFieldWidget;
class FTextViewWidget;
class FFriendInfoWidget;
class FTextLabelWidget;
class FUIEvent;
class FScroller;

/**
 * Friend List
 */
class FFriendListWidget : public IWidget, public IScrollable
{
public:
	/**
	 * Constructor
	 */
	FFriendListWidget(Vector2 FriendListPos,
		Vector2 FriendLisSize,
		UILayer FriendListLayer,
		FontPtr FriendListNormalFont,
		FontPtr FriendListTitleFont,
		FontPtr FriendListSmallFont,
		FontPtr FriendListTinyFont);

	FFriendListWidget(const FFriendListWidget&) = delete;
	FFriendListWidget& operator=(const FFriendListWidget&) = delete;

	/**
	 * Destructor
	 */
	virtual ~FFriendListWidget() {};

	/** IGfxComponent */
	virtual void Create() override;
	virtual void Release() override;
	virtual void Update() override;
	virtual void Render(FSpriteBatchPtr& Batch) override;
#ifdef _DEBUG
	virtual void DebugRender() override;
#endif

	/** IWidget */
	virtual void OnUIEvent(const FUIEvent& event) override;
	virtual void SetFocused(bool) override;
	virtual void SetPosition(Vector2 Pos) override;
	virtual void SetSize(Vector2 NewSize) override;

	/** IScrollable */
	virtual void ScrollUp(size_t length) override;
	virtual void ScrollDown(size_t length) override;
	virtual void ScrollToTop() override;
	virtual void ScrollToBottom() override;
	virtual size_t NumEntries() const override;
	virtual size_t GetNumLinesPerPage() const override;
	virtual size_t FirstViewedEntry() const override;
	virtual size_t LastViewedEntry() const override;

	/** Refreshes friend data with given collection of friends */
	void RefreshFriendData(const std::vector<FFriendData>& friends);

	/** Sets visibility of friend info */
	void SetFriendInfoVisible(bool bVisible) { bFriendInfoVisible = bVisible; }

	/** Sets filter to display a subset of available friends */
	void SetFilter(const std::wstring& filter)
	{
		NameFilter = filter; 
		bIsFilterSet = !NameFilter.empty();
		if (bIsFilterSet)
		{
			bCanPerformSearch = true;
			FirstFriendToView = 0;
		}
	}

	/** Clears filter */
	void ClearFilter()
	{
		NameFilter.clear();
		bIsFilterSet	= false;
		bCanPerformSearch = false;
		FirstFriendToView = 0;
	}

	/** Clears friend list */
	void Clear();

	/** Resets friend list */
	void Reset();

	/** Sets offset from bottom. */
	void SetBottomOffset(float Value) { BottomOffset = Value; }

private:
	/** Creates widgets for friends info */
	void CreateFriends();

	/** Friend data */
	std::vector<FFriendData> FriendData;

	/** Friend data once filter has been applied */
	std::vector<FFriendData> FilteredData;

	/** Value for filter */
	std::wstring NameFilter;

	/** True if filter has been set to a non-default value */
	bool bIsFilterSet = false;

	/** True if there's not a search active already */
	bool bCanPerformSearch = false;

	/** Background Image */
	std::shared_ptr<FSpriteWidget> BackgroundImage;
	
	/** Title Label */
	std::shared_ptr<FTextLabelWidget> TitleLabel;

	/** Collection of all friend info widgets */
	std::vector<std::shared_ptr<FFriendInfoWidget>> FriendWidgets;

	/** Widget controlling friend search */
	std::shared_ptr<FTextFieldWidget> SearchFriendWidget;

	/** Search button */
	std::shared_ptr<FButtonWidget> SearchButtonWidget;

	/** Cancel search button */
	std::shared_ptr<FButtonWidget> CancelSearchButtonWidget;

	/** Shared font */
	FontPtr NormalFont;

	/** Title font */
	FontPtr TitleFont;

	/** Small font */
	FontPtr SmallFont;

	/** Tiny font */
	FontPtr TinyFont;

	/** Scroller */
	std::shared_ptr<FScroller> Scroller;

	/** Index of first friends to view */
	size_t FirstFriendToView = 0;

	/** Flag used to hide / show friend info */
	bool bFriendInfoVisible = true;

	/** Offset from the bottom of widget that is not used. */
	float BottomOffset = 0.0f;

	/** Synchronized to the Friends Dirty Counter, if numbers are different an update has occurred */
	uint64_t FriendsDirtyCounter = 0;

	/** Colors */
	static constexpr FColor EnabledCol = FColor(1.f, 1.f, 1.f, 1.f);
	static constexpr FColor DisabledCol = FColor(0.2f, 0.2f, 0.2f, 1.f);
};
