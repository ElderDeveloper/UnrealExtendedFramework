// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widget.h"
#include "Font.h"
#include "ListView.h"
#include "Dialog.h"

class FTextLabelWidget;
class FButtonWidget;

struct FTableRowData
{
	std::vector<std::wstring> Values;

	bool operator!=(const FTableRowData& Other) const
	{
		return Values != Other.Values;
	}
};

class FTableRowView : public FDialog
{
public:
	FTableRowView(Vector2 Pos,
		Vector2 Size,
		UILayer Layer,
		const std::wstring& AssetFile,
		const FTableRowData& InData,
		FColor BackgroundColor,
		FColor TextColor);

	void SetFocused(bool bValue) override;

	/** Set Position */
	void SetPosition(Vector2 Pos) override;

	/** Set Size */
	void SetSize(Vector2 NewSize) override;

	void Enable() override;

	void SetData(const FTableRowData& InData);
	void SetFont(FontPtr InFont);

protected:
	void ReadjustLayout();
	void OnPressed(size_t ActionIndex);

	FTableRowData Data;

	std::vector<std::shared_ptr<FTextLabelWidget>> RowWidgets;
	std::wstring AssetFile;
	FColor BackgroundColor;
	FColor TextColor;
	FontPtr Font;
};

template<>
std::shared_ptr<FTableRowView> CreateListEntry(Vector2 Pos, Vector2 Size, UILayer Layer, const FTableRowData& Data);