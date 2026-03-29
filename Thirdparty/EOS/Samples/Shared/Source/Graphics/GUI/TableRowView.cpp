// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "TableRowView.h"
#include "TextLabel.h"
#include "Button.h"

FTableRowView::FTableRowView(Vector2 Pos, Vector2 Size, UILayer Layer, const std::wstring& InAssetFile, const FTableRowData& InData, FColor InBackgroundColor, FColor InTextColor)
	: FDialog(Pos, Size, Layer), Data(InData), AssetFile(InAssetFile), BackgroundColor(InBackgroundColor), TextColor(InTextColor)
{
	Vector2 LabelSize = Vector2(Size.x / InData.Values.size(), Size.y);

	//create labels
	for (size_t i = 0; i < Data.Values.size(); ++i)
	{
		std::shared_ptr<FTextLabelWidget> NextLabel = std::make_shared<FTextLabelWidget>(
			Vector2(Pos.x + LabelSize.x * i, Pos.y),
			LabelSize,
			Layer - 1,
			Data.Values[i],
			AssetFile,
			BackgroundColor,
			TextColor);

		RowWidgets.push_back(NextLabel);
		AddWidget(NextLabel);
	}
}

void FTableRowView::SetFocused(bool bValue)
{
	FDialog::SetFocused(bValue);

	if (bValue)
	{
		SetBorderColor(Color::UIDarkGrey);
	}
	else
	{
		ClearBorderColor();
	}
}

void FTableRowView::SetPosition(Vector2 Pos)
{
	FDialog::SetPosition(Pos);

	ReadjustLayout();
}

void FTableRowView::SetSize(Vector2 NewSize)
{
	FDialog::SetSize(NewSize);

	ReadjustLayout();
}

void FTableRowView::Enable()
{
	FDialog::Enable();
}

void FTableRowView::SetData(const FTableRowData& InData)
{
	if (InData.Values.size() != RowWidgets.size())
	{
		RemoveWidgets(RowWidgets);
		RowWidgets.clear();

		Vector2 LabelSize = Vector2(Size.x / InData.Values.size(), Size.y);

		//create labels
		for (size_t i = 0; i < InData.Values.size(); ++i)
		{
			std::shared_ptr<FTextLabelWidget> NextLabel = std::make_shared<FTextLabelWidget>(
				Vector2(Position.x + LabelSize.x * i, Position.y),
				LabelSize,
				Layer - 1,
				InData.Values[i],
				AssetFile,
				BackgroundColor,
				TextColor);
			NextLabel->Create();
			NextLabel->SetFont(Font);
			RowWidgets.push_back(NextLabel);
			AddWidget(NextLabel);
		}
	}

	Data = InData;

	for (size_t i = 0; i < RowWidgets.size(); ++i)
	{
		std::wstring DataString = (i < InData.Values.size()) ? InData.Values[i] : L"-";
		RowWidgets[i]->SetText(DataString);
	}

	ReadjustLayout();
}

void FTableRowView::SetFont(FontPtr InFont)
{
	Font = InFont;

	for (auto Widget : RowWidgets)
	{
		Widget->SetFont(InFont);
	}
}

void FTableRowView::ReadjustLayout()
{
	//we reserve 2 table slots for 3 actions
	Vector2 LabelSize = Vector2(Size.x / Data.Values.size(), Size.y);

	//resize labels
	for (size_t i = 0; i < RowWidgets.size(); ++i)
	{
		RowWidgets[i]->SetPosition(Vector2(Position.x + LabelSize.x * i, Position.y));
		RowWidgets[i]->SetSize(LabelSize);
	}
}

void FTableRowView::OnPressed(size_t ActionIndex)
{

}

template<>
std::shared_ptr<FTableRowView> CreateListEntry(Vector2 Pos, Vector2 Size, UILayer Layer, const FTableRowData& Data)
{
	return std::make_shared<FTableRowView>(Pos, Size, Layer, L"", Data, Color::DarkGray, Color::White);
}