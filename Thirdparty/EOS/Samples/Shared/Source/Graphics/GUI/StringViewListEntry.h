// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widget.h"
#include "Font.h"
#include "TextLabel.h"
#include "ListView.h"
#include "Button.h"

class FStringViewListEntry : public FTextLabelWidget
{
public:
	FStringViewListEntry(Vector2 LabelPos,
		Vector2 LabelSize,
		UILayer LabelLayer,
		const std::wstring& LabelText,
		const std::wstring& LabelAssetFile) : FTextLabelWidget(LabelPos, LabelSize, LabelLayer, LabelText, LabelAssetFile, Color::UIBackgroundGrey)
	{}

	void SetFocused(bool bValue) override;
};

template<>
std::shared_ptr<FStringViewListEntry> CreateListEntry(Vector2 Pos, Vector2 Size, UILayer Layer, const std::wstring& Data);