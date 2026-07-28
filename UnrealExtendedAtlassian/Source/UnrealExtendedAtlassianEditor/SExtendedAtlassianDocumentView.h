// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianDocBlock.h"
#include "Widgets/SCompoundWidget.h"

class SScrollBox;
class SVerticalBox;

/**
 * Renders a document block list as styled Slate widgets.
 *
 * Block structure becomes separate widgets; inline formatting is rich-text markup within each
 * block. Content is width-capped, because text running the full width of a docked tab is well past
 * the length at which the eye reliably finds the next line.
 *
 * Contains nothing Atlassian-specific, so it can be lifted into a shared plugin unchanged.
 *
 * Known ceiling: blocks are built eagerly into a scroll box rather than virtualised, so a document
 * of many thousands of blocks will be slow to build. Real pages are far below that; virtualising
 * would cost text selection and correct wrapping at variable heights.
 */
class SExtendedAtlassianDocumentView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SExtendedAtlassianDocumentView)
		: _MaxReadingWidth(920.0f)
	{}
		/** Content is capped to this width for legibility. */
		SLATE_ARGUMENT(float, MaxReadingWidth)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Replaces the displayed document. */
	void SetBlocks(const TArray<FExtendedAtlassianDocBlock>& InBlocks);

	void Clear();

private:
	TSharedRef<SWidget> BuildBlockWidget(const FExtendedAtlassianDocBlock& Block) const;
	TSharedRef<SWidget> BuildTableRow(const FExtendedAtlassianDocBlock& Block) const;

	/** Spacing above a block, so sections separate without manual blank lines. */
	static float GetTopPadding(const FExtendedAtlassianDocBlock& Block, bool bIsFirst);

	TSharedPtr<SVerticalBox> ContentBox;
	TSharedPtr<SScrollBox> ScrollBox;
	float MaxReadingWidth = 920.0f;
};
