// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "StringUtils.h"
#include "AccountHelpers.h"
#include "Input.h"
#include "Game.h"
#include "Main.h"
#include "Console.h"
#include "TextLabel.h"
#include "TextField.h"
#include "TextView.h"
#include "Button.h"
#include "UIEvent.h"
#include "ListView.h"

template <typename DataType, typename DataViewWidget>
FListViewWidget<DataType, DataViewWidget>::FListViewWidget(Vector2 ListPos,
									 Vector2 LisSize,
									 UILayer ListLayer,
									 float InEntryHeight,
									 float InLabelHeight,
									 float InScrollerWidth,
									 std::wstring BackgroundImageName,
									 std::wstring TitleText,
									 FontPtr ListNormalFont,
									 FontPtr ListTitleFont,
									 FontPtr ListSmallFont,
									 FontPtr ListTinyFont) :
	IWidget(ListPos, LisSize, ListLayer),
	NormalFont(ListNormalFont),
	TitleFont(ListTitleFont),
	EntryHeight(InEntryHeight),
	LabelHeight(InLabelHeight),
	LabelVerticalOffset((InLabelHeight > 0.01f) ? 20.0f : 0.0f),
	ScrollerWidth(InScrollerWidth)
{
	BackgroundImage = std::make_shared<FSpriteWidget>(ListPos, LisSize, ListLayer, BackgroundImageName);

	TitleLabel = std::make_shared<FTextLabelWidget>(
		Vector2(Position.x, Position.y + 4.f),
		Vector2(150.f, InLabelHeight),
		ListLayer - 1,
		TitleText,
		L"",
		FColor(1.f, 1.f, 1.f, 1.f),
		FColor(1.f, 1.f, 1.f, 1.f),
		EAlignmentType::Left);
}

template <typename DataType, typename DataViewWidget>
void FListViewWidget<DataType, DataViewWidget>::Create()
{
	BackgroundImage->Create();

	// Title
	TitleLabel->Create();
	TitleLabel->SetFont(TitleFont);

	// Scroller
	Vector2 scrollerPosition = Vector2(Position.x + Size.x - ScrollerWidth - 4.f, Position.y + LabelHeight + LabelVerticalOffset);
	Scroller = std::make_unique<FScroller>(std::static_pointer_cast<FListViewWidget<DataType, DataViewWidget>>(shared_from_this()),
		scrollerPosition,
		Vector2(ScrollerWidth, Size.y - LabelHeight),
		Layer - 1,
		L"Assets/scrollbar.dds");
	if (Scroller)
	{
		Scroller->Create();
		Scroller->Hide();
	}

	CreateListEntries();
}

template <typename DataType, typename DataViewWidget>
void FListViewWidget<DataType, DataViewWidget>::Release()
{
	if (BackgroundImage) BackgroundImage->Release();

	NormalFont.reset();
	TitleFont.reset();

	TitleLabel->Release();
	TitleLabel.reset();

	if (Scroller)
	{
		Scroller->Release();
		Scroller.reset();
	}

	ClearListEntries();
}

template <typename DataType, typename DataViewWidget>
void FListViewWidget<DataType, DataViewWidget>::Update()
{
	if (bEnabled)
	{
		if (BackgroundImage) BackgroundImage->Update();

		if (TitleLabel) TitleLabel->Update();

		if (FirstEntryToView > (Data.size() - DataWidgets.size()))
		{
			FirstEntryToView = (Data.size() - DataWidgets.size());
		}

		for (size_t Ix = 0; Ix < DataWidgets.size(); ++Ix)
		{
			std::shared_ptr<DataViewWidget> Widget = DataWidgets[Ix];
			if (Widget)
			{
				if (FirstEntryToView + Ix < Data.size())
				{
					Widget->Show();
					Widget->SetData(Data[FirstEntryToView + Ix]);
				}
				else
				{
					Widget->SetData(DataType());
					Widget->Hide();
				}
				Widget->Update();
			}
		}

		if (Scroller)
		{
			if (Data.size() <= DataWidgets.size())
			{
				Scroller->Hide();
			}
			else
			{
				Scroller->Show();
			}
			Scroller->Update();
		}
	}
}

template <typename DataType, typename DataViewWidget>
void FListViewWidget<DataType, DataViewWidget>::Render(FSpriteBatchPtr& Batch)
{
	if (!bShown)
		return;

	IWidget::Render(Batch);

	if (BackgroundImage) BackgroundImage->Render(Batch);

	if (TitleLabel) TitleLabel->Render(Batch);

	if (bEntriesVisible)
	{
		for (size_t Index = 0; Index < DataWidgets.size(); ++Index)
		{
			std::shared_ptr<DataViewWidget> Widget = DataWidgets[Index];

			if (Widget)
			{
				//don't render empty widgets
				if (Index < Data.size() && GetDataEntry(Index) != DataType())
				{
					Widget->Render(Batch);
				}
			}
		}

		if (Scroller) Scroller->Render(Batch);
	}
}

#ifdef _DEBUG
template <typename DataType, typename DataViewWidget>
void FListViewWidget<DataType, DataViewWidget>::DebugRender()
{
	IWidget::DebugRender();

	if (BackgroundImage) BackgroundImage->DebugRender();
	if (TitleLabel) TitleLabel->DebugRender();

	for (std::shared_ptr<DataViewWidget> Widget : DataWidgets)
	{
		Widget->DebugRender();
	}

	Scroller->DebugRender();
}
#endif

template <typename DataType, typename DataViewWidget>
void FListViewWidget<DataType, DataViewWidget>::SetPosition(Vector2 Pos)
{

	const float VerticalOffset = Pos.y - GetPosition().y;

	IWidget::SetPosition(Pos);

	if (BackgroundImage) BackgroundImage->SetPosition(Vector2(Position.x, Position.y));

	// Title Label
	if (TitleLabel) TitleLabel->SetPosition(Vector2(Pos.x, Pos.y + 4.f));

	if (Scroller && TitleLabel)
	{
		// Scroller
		Vector2 ScrollerPos = Vector2(Pos.x + Size.x - Scroller->GetSize().x - 5.f,
			Pos.y + TitleLabel->GetSize().y + LabelVerticalOffset);
		Scroller->SetPosition(ScrollerPos);
	}

	// Data entries
	for (std::shared_ptr<DataViewWidget> DataWidget : DataWidgets)
	{
		Vector2 EntryPos = Vector2(Pos.x, DataWidget->GetPosition().y + VerticalOffset);
		DataWidget->SetPosition(EntryPos);
	}
}

template <typename DataType, typename DataViewWidget>
void FListViewWidget<DataType, DataViewWidget>::SetSize(Vector2 NewSize)
{
	IWidget::SetSize(NewSize);

	if (BackgroundImage) BackgroundImage->SetSize(Vector2(NewSize.x, NewSize.y - 10.f));

	if (Scroller) Scroller->SetSize(Vector2(Scroller->GetSize().x, Size.y - LabelHeight - LabelVerticalOffset));

	// Recreate widgets
	ClearListEntries();
	CreateListEntries();
}

template <typename DataType, typename DataViewWidget>
void FListViewWidget<DataType, DataViewWidget>::OnUIEvent(const FUIEvent& Event)
{
	if (!bEnabled || !bEntriesVisible)
		return;

	if (Event.GetType() == EUIEventType::MousePressed || Event.GetType() == EUIEventType::MouseReleased)
	{
		for (size_t Ix = 0; Ix < DataWidgets.size(); ++Ix)
		{
			std::shared_ptr<DataViewWidget> Widget = DataWidgets[Ix];
			if (Widget->CheckCollision(Event.GetVector()))
			{
				//switch focus/selection logic
				if (Widget && !Widget->IsFocused())
				{
					//clear previous focus
					SetFocused(false);
					SetFocused(true);
					Widget->SetFocused(true);

					if (CallbackOnEntrySelected)
					{
						CallbackOnEntrySelected(FirstEntryToView + Ix);
					}
				}

				Widget->OnUIEvent(Event);
			}
		}

		if (Scroller && Scroller->CheckCollision(Event.GetVector()))
		{
			Scroller->OnUIEvent(Event);
		}
	}
	else if (Event.GetType() == EUIEventType::KeyPressed ||
		Event.GetType() == EUIEventType::MouseWheelScrolled ||
		Event.GetType() == EUIEventType::TextInput)
	{
		if (Scroller)
		{
			Scroller->OnUIEvent(Event);
		}

		for (std::shared_ptr<DataViewWidget> Widget : DataWidgets)
		{
			if (Widget->IsFocused())
			{
				Widget->OnUIEvent(Event);
			}
		}
	}
	else
	{
		for (std::shared_ptr<DataViewWidget> Widget : DataWidgets)
		{
			Widget->OnUIEvent(Event);
		}
	}
}

template <typename DataType, typename DataViewWidget>
void FListViewWidget<DataType, DataViewWidget>::SetFocused(bool bValue)
{
	IWidget::SetFocused(bValue);

	if (!bValue)
	{
		if (TitleLabel) TitleLabel->SetFocused(false);
		for (std::shared_ptr<DataViewWidget> Widget : DataWidgets)
		{
			if (Widget)
			{
				Widget->SetFocused(false);
			}
		}
	}
}

template <typename DataType, typename DataViewWidget>
void FListViewWidget<DataType, DataViewWidget>::ScrollUp(size_t length)
{
	if (FirstEntryToView < length)
	{
		FirstEntryToView = 0;
	}
	else
	{
		FirstEntryToView -= length;
	}

	//Make widgets lose focus (if any) after scrolling
	SetFocused(false);
}

template <typename DataType, typename DataViewWidget>
void FListViewWidget<DataType, DataViewWidget>::ScrollDown(size_t length)
{
	FirstEntryToView += length;
	if (DataWidgets.size() > Data.size())
	{
		FirstEntryToView = 0;
		return;
	}

	if (FirstEntryToView > (Data.size() - DataWidgets.size()))
	{
		FirstEntryToView = (Data.size() - DataWidgets.size());
	}

	//Make widgets lose focus (if any) after scrolling
	SetFocused(false);
}
template <typename DataType, typename DataViewWidget>
void FListViewWidget<DataType, DataViewWidget>::ScrollToTop()
{
	ScrollUp(Data.size());
}

template <typename DataType, typename DataViewWidget>
void FListViewWidget<DataType, DataViewWidget>::ScrollToBottom()
{
	ScrollDown(Data.size());
}


template <typename DataType, typename DataViewWidget>
size_t FListViewWidget<DataType, DataViewWidget>::NumEntries() const
{
	return Data.size();
}

template <typename DataType, typename DataViewWidget>
size_t FListViewWidget<DataType, DataViewWidget>::GetNumLinesPerPage() const
{
	return DataWidgets.size();
}

template <typename DataType, typename DataViewWidget>
size_t FListViewWidget<DataType, DataViewWidget>::FirstViewedEntry() const
{
	return FirstEntryToView;
}

template <typename DataType, typename DataViewWidget>
size_t FListViewWidget<DataType, DataViewWidget>::LastViewedEntry() const
{
	return FirstEntryToView + DataWidgets.size() - 1;
}

template <typename DataType, typename DataViewWidget>
void FListViewWidget<DataType, DataViewWidget>::RefreshData(const std::vector<DataType>& DataEntries)
{
	Data = DataEntries;

	if (Data.empty())
	{
		for (std::shared_ptr<DataViewWidget> NextWidget : DataWidgets)
		{
			NextWidget->SetData(DataType());
		}
	}
}

template <typename DataType, typename DataViewWidget>
void FListViewWidget<DataType, DataViewWidget>::SetTitleText(const std::wstring& Text)
{
	if (TitleLabel)
	{
		TitleLabel->SetText(Text);
	}
}

template <typename DataType, typename DataViewWidget>
void FListViewWidget<DataType, DataViewWidget>::ClearListEntries()
{
	for (std::shared_ptr<DataViewWidget> Widget : DataWidgets)
	{
		if (Widget)
		{
			Widget->Release();
			Widget.reset();
		}
	}

	DataWidgets.clear();
}

template <typename DataType, typename DataViewWidget>
void FListViewWidget<DataType, DataViewWidget>::Clear()
{
	Data.clear();
	ClearListEntries();
}

template <typename DataType, typename DataViewWidget>
void FListViewWidget<DataType, DataViewWidget>::Reset()
{
	Clear();
	CreateListEntries();
}

template <typename DataType, typename DataViewWidget>
void FListViewWidget<DataType, DataViewWidget>::CreateListEntries()
{
	const size_t NumEntriesOnScreen = size_t((Size.y - LabelHeight) / EntryHeight);
	DataWidgets.resize(NumEntriesOnScreen);
	for (size_t Ix = 0; Ix < NumEntriesOnScreen; ++Ix)
	{
		DataWidgets[Ix] = CreateListEntry<DataType, DataViewWidget>(Vector2(Position.x, Position.y + LabelHeight + (EntryHeight * Ix) + LabelVerticalOffset),
			Vector2(Size.x - ScrollerWidth - 2.f, EntryHeight),
			Layer - 1,
			DataType());

		if (DataWidgets[Ix])
		{
			DataWidgets[Ix]->Create();
			DataWidgets[Ix]->Hide();
			DataWidgets[Ix]->SetFont(NormalFont);

			if (Scroller)
			{
				Vector2 EntrySize = Vector2(Size.x - Scroller->GetSize().x - 5.f, DataWidgets[Ix]->GetSize().y);
				DataWidgets[Ix]->SetSize(EntrySize);
			}
		}
	}
}

template <typename DataType, typename DataViewWidget>
void FListViewWidget<DataType, DataViewWidget>::SetFonts(FontPtr InNormalFont, FontPtr InTitleFont)
{
	NormalFont = InNormalFont;
	TitleFont = InTitleFont;

	if (TitleLabel)
	{
		TitleLabel->SetFont(TitleFont);
	}

	if (!DataWidgets.empty())
	{
		for (std::shared_ptr<DataViewWidget> NextWidget : DataWidgets)
		{
			if (NextWidget)
			{
				NextWidget->SetFont(NormalFont);
			}
		}
	}
}

//Default implementation simply passes empty data
template<typename DataType, typename DataViewWidget>
std::shared_ptr<DataViewWidget> CreateListEntry(Vector2 Pos, Vector2 Size, UILayer Layer, const DataType& Data)
{
	return std::make_shared<DataViewWidget>(Pos, Size, Layer, Data, L"");
}
