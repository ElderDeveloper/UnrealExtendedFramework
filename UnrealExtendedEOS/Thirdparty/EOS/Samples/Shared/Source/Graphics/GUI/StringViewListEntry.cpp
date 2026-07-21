// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "Main.h"
#include "StringViewListEntry.h"

void FStringViewListEntry::SetFocused(bool bValue)
{
	FTextLabelWidget::SetFocused(bValue);

	/*
	if (Value)
	{
		SetBackgroundColor(assets::DefaultButtonColors[2]);
	}
	else
	{
		SetBackgroundColor(assets::DefaultButtonColors[0]);
	}
	*/
}

template<>
std::shared_ptr<FStringViewListEntry> CreateListEntry(Vector2 Pos, Vector2 Size, UILayer Layer, const std::wstring& Data)
{
	return std::make_shared<FStringViewListEntry>(Pos, Size, Layer, Data, L"Assets/textfield.dds");
}