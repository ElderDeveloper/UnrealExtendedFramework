// Copyright Epic Games, Inc. All Rights Reserved.

#include "pch.h"
#include "Main.h"
#include "SelectableStringViewListEntry.h"

void FSelectableStringViewListEntry::SetFocused(bool bValue)
{
	if (BackgroundImage && bValue != IsFocused())
	{
		FColor CurrentColor = BackgroundImage->GetBackgroundColor();

		const float Diff = 0.3f;
		FColor Adjustment = (bValue) ? FColor(Diff, Diff, Diff, Diff) : FColor(-Diff, -Diff, -Diff, 0.0f);

		CurrentColor.A += Adjustment.A;
		CurrentColor.R += Adjustment.R;
		CurrentColor.G += Adjustment.G;
		CurrentColor.B += Adjustment.B;

		BackgroundImage->SetBackgroundColor(CurrentColor);
	}

	FTextLabelWidget::SetFocused(bValue);
}

template<>
std::shared_ptr<FSelectableStringViewListEntry> CreateListEntry(Vector2 Pos, Vector2 Size, UILayer Layer, const std::wstring& Data)
{
	return std::make_shared<FSelectableStringViewListEntry>(Pos, Size, Layer, Data, L"Assets/black_grey_button.dds");
}