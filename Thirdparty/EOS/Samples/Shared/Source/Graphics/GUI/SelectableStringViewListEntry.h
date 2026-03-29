// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widget.h"
#include "Font.h"
#include "TextLabel.h"
#include "ListView.h"
#include "Button.h"

class FSelectableStringViewListEntry : public FTextLabelWidget
{
public:
	FSelectableStringViewListEntry(Vector2 LabelPos,
		Vector2 LabelSize,
		UILayer LabelLayer,
		const std::wstring& LabelText,
		const std::wstring& LabelAssetFile) : FTextLabelWidget(LabelPos, LabelSize, LabelLayer, LabelText, LabelAssetFile, Color::White)
	{}

	void SetFocused(bool bValue) override;
};

template<>
std::shared_ptr<FSelectableStringViewListEntry> CreateListEntry(Vector2 Pos, Vector2 Size, UILayer Layer, const std::wstring& Data);