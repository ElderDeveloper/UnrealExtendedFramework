// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "DebugLog.h"
#include "StringUtils.h"
#include "Main.h"
#include "Game.h"
#include "TextLabel.h"

#define TableViewLabelHeight (30.0f)

template <typename FTableRowDataType, typename FTableRowViewType>
FTableView<FTableRowDataType, FTableRowViewType>::FTableView(Vector2 TablePosition,
	Vector2 TableSize,
	UILayer Layer,
	const std::wstring& LabelText,
	float InScrollerWidth,
	const std::vector<FTableRowDataType>& InData,
	const FTableRowDataType& LabelRow,
	float InTableViewEntryHeight /*= 25.0f*/):
	FDialog(TablePosition, TableSize, Layer),
	Data(InData),
	ScrollerWidth(InScrollerWidth),
	TableViewEntryHeight(InTableViewEntryHeight)
{
	Label = std::make_shared<FTextLabelWidget>(
		TablePosition,
		Vector2(TableSize.x, TableViewLabelHeight),
		Layer,
		LabelText,
		L"",
		FColor(1.f, 1.f, 1.f, 1.f),
		FColor(1.f, 1.f, 1.f, 1.f),
		EAlignmentType::Left
		);

	ColumnLabels = std::make_shared<FTableRowViewType>(
		TablePosition + Vector2(0.0f, TableViewLabelHeight),
		Vector2(TableSize.x - (ScrollerWidth * 1.3f), TableViewLabelHeight),
		Layer,
		L"",
		LabelRow,
		Color::DarkGray,
		Color::White);
	
	TableList = std::make_shared<FTableListView>(
		TablePosition + Vector2(0.0f, TableViewLabelHeight * 2.0f),
		TableSize - Vector2(0.0f, TableViewLabelHeight * 2.0f),
		Layer,
		TableViewEntryHeight, //entry height
		0.0f, //label height (no label)
		ScrollerWidth, //scroller width
		L"", //background
		L"", //Title text
		nullptr, //Fonts are not present yet
		nullptr,
		nullptr,
		nullptr
		);

	TableList->RefreshData(Data);
	TableList->SetOnEntrySelectedCallback([this](size_t Index) { this->OnEntrySelected(Index); });

	AddWidget(Label);
	AddWidget(ColumnLabels);
	AddWidget(TableList);
}

template <typename FTableRowDataType, typename FTableRowViewType>
void FTableView<FTableRowDataType, FTableRowViewType>::OnUIEvent(const FUIEvent& Event)
{
	if (!bEnabled)
		return;

	FDialog::OnUIEvent(Event);
}

template <typename FTableRowDataType, typename FTableRowViewType>
void FTableView<FTableRowDataType, FTableRowViewType>::SetPosition(Vector2 Pos)
{
	FDialog::SetPosition(Pos);

	Label->SetPosition(Pos);
	ColumnLabels->SetPosition(Pos + Vector2(0.0f, TableViewLabelHeight));
	TableList->SetPosition(Pos + Vector2(0.0f, 2.0f * TableViewLabelHeight));
}

template <typename FTableRowDataType, typename FTableRowViewType>
void FTableView<FTableRowDataType, FTableRowViewType>::SetSize(Vector2 NewSize)
{
	FDialog::SetSize(NewSize);

	Label->SetSize(Vector2(NewSize.x, TableViewLabelHeight));
	ColumnLabels->SetSize(Vector2(NewSize.x - (ScrollerWidth * 1.3f), TableViewLabelHeight));
	TableList->SetSize(Vector2(NewSize.x, NewSize.y - 2 * TableViewLabelHeight));
}

template <typename FTableRowDataType, typename FTableRowViewType>
void FTableView<FTableRowDataType, FTableRowViewType>::SetOnSelectionCallback(std::function<void(const std::wstring& Element1, const std::string& Element2)> Callback)
{
	OnSelectionCallback = Callback;
}

template <typename FTableRowDataType, typename FTableRowViewType>
void FTableView<FTableRowDataType, FTableRowViewType>::SetFonts(FontPtr NormalFont, FontPtr TitleFont)
{
	Label->SetFont(TitleFont);
	ColumnLabels->SetFont(NormalFont);
	TableList->SetFonts(NormalFont, TitleFont);
}

template <typename FTableRowDataType, typename FTableRowViewType>
void FTableView<FTableRowDataType, FTableRowViewType>::SetLabelText(const std::wstring& LabelText)
{
	Label->SetText(LabelText);
}

template <typename FTableRowDataType, typename FTableRowViewType>
std::wstring FTableView<FTableRowDataType, FTableRowViewType>::GetLabelText()
{
	return Label->GetText();
}

template <typename FTableRowDataType, typename FTableRowViewType>
void FTableView<FTableRowDataType, FTableRowViewType>::RefreshData(std::vector<FTableRowDataType>&& InData)
{
	Data.swap(InData);

	if (TableList)
	{
		TableList->RefreshData(Data);
	}
}

template <typename FTableRowDataType, typename FTableRowViewType>
void FTableView<FTableRowDataType, FTableRowViewType>::RefreshLabels(FTableRowDataType&& Labels)
{
	if (ColumnLabels)
	{
		ColumnLabels->SetData(Labels);
	}
}

template <typename FTableRowDataType, typename FTableRowViewType>
void FTableView<FTableRowDataType, FTableRowViewType>::Clear()
{
	if (TableList)
	{
		TableList->Reset();
	}

	if (ColumnLabels)
	{
		ColumnLabels->SetData(std::move(FTableRowDataType()));
	}
}

template <typename FTableRowDataType, typename FTableRowViewType>
void FTableView<FTableRowDataType, FTableRowViewType>::ScrollToTop()
{
	if (TableList)
	{
		TableList->ScrollToTop();
	}
}

template <typename FTableRowDataType, typename FTableRowViewType>
void FTableView<FTableRowDataType, FTableRowViewType>::OnEntrySelected(size_t Index)
{
	if (Index < Data.size())
	{
		//save selection
		CurrentlySelectedOption = Data[Index].Values[0];

		if (OnSelectionCallback)
		{
			OnSelectionCallback(CurrentlySelectedOption, "");
		}
	}
}

template <typename FTableRowDataType, typename FTableRowViewType>
void FTableView<FTableRowDataType, FTableRowViewType>::Render(FSpriteBatchPtr& Batch)
{
	FDialog::Render(Batch);
}

#undef TableViewLabelHeight

