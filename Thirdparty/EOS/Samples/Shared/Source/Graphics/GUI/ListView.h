// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widget.h"
#include "Scroller.h"
#include "Font.h"

/** Forward declarations */
class FTextLabelWidget;
class FUIEvent;

/** Template function to be able to customize list entry widget creation */
template<typename DataType, typename DataViewWidget>
std::shared_ptr<DataViewWidget> CreateListEntry(Vector2 Pos, Vector2 Size, UILayer Layer, const DataType& Data);

/**
 * Generic data list. Use it by specifying type of Data to be represented by each entry of the list
 * and widget type that is able to view the Data. Widgets of that type will be spawned in scrollable list.
 */
template <typename DataType, typename DataViewWidget> 
class FListViewWidget : public IWidget, public IScrollable
{
public:
	/**
	 * Constructor
	 */
	FListViewWidget(Vector2 ListPos,
		Vector2 ListSize,
		UILayer ListLayer,
		float EntryHeight,
		float LabelHeight,
		float ScrollerWidth,
		std::wstring BackgroundImageName,
		std::wstring TitleText,
		FontPtr ListNormalFont,
		FontPtr ListTitleFont,
		FontPtr ListSmallFont,
		FontPtr ListTinyFont);

	FListViewWidget(const FListViewWidget&) = delete;
	FListViewWidget& operator=(const FListViewWidget&) = delete;

	/**
	 * Destructor
	 */
	virtual ~FListViewWidget() {}

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

	/** Update widget with new data. */
	void RefreshData(const std::vector<DataType>& Data);

	/** Change title */
	void SetTitleText(const std::wstring& Text);

	/** Make list entries visible or invisible. Label is not affected. */
	void SetEntriesVisible(bool bVisible) { bEntriesVisible = bVisible; }

	/** Subscribe for entry selection */
	void SetOnEntrySelectedCallback(std::function<void(size_t)> Callback) { CallbackOnEntrySelected = Callback; }

	/** Clears list */
	void Clear();

	/** Resets list */
	void Reset();

	const DataType& GetDataEntry(size_t Index) { return Data[Index]; }

	void SetFonts(FontPtr NormalFont, FontPtr TitleFont);

private:
	/** Creates widgets for data entries */
	void CreateListEntries();

	/** Clears widgets */
	void ClearListEntries();

	/** Data entries */
	std::vector<DataType> Data;

	/** Background Image */
	std::shared_ptr<FSpriteWidget> BackgroundImage;
	
	/** Title Label */
	std::shared_ptr<FTextLabelWidget> TitleLabel;

	/** Collection of all data widgets */
	std::vector<std::shared_ptr<DataViewWidget>> DataWidgets;

	/** Callback for entry selection. */
	std::function<void(size_t)> CallbackOnEntrySelected;

	/** Shared font */
	FontPtr NormalFont;

	/** Title font */
	FontPtr TitleFont;

	/** Scroller */
	std::shared_ptr<FScroller> Scroller;

	/** Index of the first entry to view */
	size_t FirstEntryToView = 0;

	/** The height of the view widget for every entry */
	const float EntryHeight;

	/** The height of title label */
	const float LabelHeight;

	/** Additional offset between label and list entries. */
	const float LabelVerticalOffset;

	/** The width of the scroller */
	const float ScrollerWidth;

	/** Are list entries visible? */
	bool bEntriesVisible = true;
};

#include "ListView.inl"
