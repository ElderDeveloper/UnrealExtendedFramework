// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "SExtendedAtlassianDocumentView.h"

#include "ExtendedAtlassianDocumentStyle.h"

#include "Framework/Text/SlateHyperlinkRun.h"
#include "HAL/PlatformProcess.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ExtendedAtlassianDocumentView"

namespace ExtendedAtlassianDocumentViewPrivate
{
	/** Indent per list nesting level. */
	constexpr float IndentPerLevel = 18.0f;

	FName StyleForHeading(int32 Level)
	{
		switch (FMath::Clamp(Level, 1, 6))
		{
		case 1:  return TEXT("Doc.H1");
		case 2:  return TEXT("Doc.H2");
		case 3:  return TEXT("Doc.H3");
		case 4:  return TEXT("Doc.H4");
		case 5:  return TEXT("Doc.H5");
		default: return TEXT("Doc.H6");
		}
	}

	void OpenLink(const FSlateHyperlinkRun::FMetadata& Metadata)
	{
		if (const FString* Href = Metadata.Find(TEXT("href")))
		{
			if (Href->StartsWith(TEXT("http://")) || Href->StartsWith(TEXT("https://")))
			{
				FPlatformProcess::LaunchURL(**Href, nullptr, nullptr);
			}
		}
	}

	/** A rich text block wired to the document style set, with clickable links. */
	TSharedRef<SRichTextBlock> MakeRichText(const FString& Markup, const FName& TextStyle)
	{
		return SNew(SRichTextBlock)
			.Text(FText::FromString(Markup))
			.TextStyle(&FExtendedAtlassianDocumentStyle::Get().GetWidgetStyle<FTextBlockStyle>(TextStyle))
			.DecoratorStyleSet(&FExtendedAtlassianDocumentStyle::Get())
			.AutoWrapText(true)
			+ SRichTextBlock::HyperlinkDecorator(TEXT("a"), FSlateHyperlinkRun::FOnClick::CreateStatic(&OpenLink));
	}
}

void SExtendedAtlassianDocumentView::Construct(const FArguments& InArgs)
{
	MaxReadingWidth = InArgs._MaxReadingWidth;

	ChildSlot
	[
		SAssignNew(ScrollBox, SScrollBox)

		+ SScrollBox::Slot()
		.Padding(FMargin(16.0f, 12.0f, 16.0f, 24.0f))
		[
			SNew(SBox)
			.MaxDesiredWidth(MaxReadingWidth)
			.HAlign(HAlign_Left)
			[
				SAssignNew(ContentBox, SVerticalBox)
			]
		]
	];
}

void SExtendedAtlassianDocumentView::Clear()
{
	if (ContentBox.IsValid())
	{
		ContentBox->ClearChildren();
	}
}

void SExtendedAtlassianDocumentView::SetBlocks(const TArray<FExtendedAtlassianDocBlock>& InBlocks)
{
	if (!ContentBox.IsValid())
	{
		return;
	}

	ContentBox->ClearChildren();

	for (int32 Index = 0; Index < InBlocks.Num(); ++Index)
	{
		const FExtendedAtlassianDocBlock& Block = InBlocks[Index];

		ContentBox->AddSlot()
			.AutoHeight()
			.Padding(FMargin(
				Block.IndentDepth * ExtendedAtlassianDocumentViewPrivate::IndentPerLevel,
				GetTopPadding(Block, Index == 0),
				0.0f,
				0.0f))
			[
				BuildBlockWidget(Block)
			];
	}

	if (ScrollBox.IsValid())
	{
		ScrollBox->ScrollToStart();
	}
}

float SExtendedAtlassianDocumentView::GetTopPadding(const FExtendedAtlassianDocBlock& Block, bool bIsFirst)
{
	if (bIsFirst)
	{
		return 0.0f;
	}

	switch (Block.Kind)
	{
	// Headings get the most air: the gap above a heading is what makes sections readable as sections.
	case EExtendedAtlassianBlockKind::Heading:
		return Block.Level <= 2 ? 18.0f : 12.0f;

	case EExtendedAtlassianBlockKind::CodeBlock:
	case EExtendedAtlassianBlockKind::Quote:
	case EExtendedAtlassianBlockKind::Rule:
	case EExtendedAtlassianBlockKind::Image:
		return 10.0f;

	// List items sit tight together; separating them would break the run visually.
	case EExtendedAtlassianBlockKind::BulletItem:
	case EExtendedAtlassianBlockKind::OrderedItem:
	case EExtendedAtlassianBlockKind::TaskItem:
	case EExtendedAtlassianBlockKind::TableRow:
		return 2.0f;

	default:
		return 8.0f;
	}
}

TSharedRef<SWidget> SExtendedAtlassianDocumentView::BuildBlockWidget(const FExtendedAtlassianDocBlock& Block) const
{
	using namespace ExtendedAtlassianDocumentViewPrivate;

	switch (Block.Kind)
	{
	case EExtendedAtlassianBlockKind::Heading:
		return MakeRichText(Block.Markup, StyleForHeading(Block.Level));

	case EExtendedAtlassianBlockKind::Rule:
		return SNew(SSeparator).Thickness(1.0f);

	case EExtendedAtlassianBlockKind::CodeBlock:
		return SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush(TEXT("ToolPanel.DarkGroupBorder")))
			.Padding(FMargin(10.0f, 8.0f))
			[
				// Code must not wrap; a wrapped line changes what the code appears to say.
				SNew(SScrollBox)
				.Orientation(Orient_Horizontal)

				+ SScrollBox::Slot()
				[
					SNew(STextBlock)
					.Text(FText::FromString(Block.RawText))
					.TextStyle(&FExtendedAtlassianDocumentStyle::Get().GetWidgetStyle<FTextBlockStyle>(TEXT("Doc.CodeBlock")))
				]
			];

	case EExtendedAtlassianBlockKind::Quote:
		return SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SSeparator)
				.Orientation(Orient_Vertical)
				.Thickness(2.0f)
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				MakeRichText(Block.Markup, TEXT("Doc.Quote"))
			];

	case EExtendedAtlassianBlockKind::Image:
		return SNew(STextBlock)
			.Text(Block.ImageAlt.IsEmpty()
				? LOCTEXT("Image", "[image]")
				: FText::Format(LOCTEXT("ImageAlt", "[image: {0}]"), FText::FromString(Block.ImageAlt)))
			.TextStyle(&FExtendedAtlassianDocumentStyle::Get().GetWidgetStyle<FTextBlockStyle>(TEXT("Doc.Marker")));

	case EExtendedAtlassianBlockKind::TableRow:
		return BuildTableRow(Block);

	case EExtendedAtlassianBlockKind::BulletItem:
	case EExtendedAtlassianBlockKind::OrderedItem:
	case EExtendedAtlassianBlockKind::TaskItem:
	{
		FString Marker;
		switch (Block.Kind)
		{
		case EExtendedAtlassianBlockKind::OrderedItem:
			Marker = FString::Printf(TEXT("%d."), Block.OrderedIndex > 0 ? Block.OrderedIndex : 1);
			break;
		case EExtendedAtlassianBlockKind::TaskItem:
			Marker = Block.bChecked ? TEXT("[x]") : TEXT("[ ]");
			break;
		default:
			Marker = TEXT("•");
			break;
		}

		return SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SBox)
				.MinDesiredWidth(Block.Kind == EExtendedAtlassianBlockKind::Paragraph ? 0.0f : 18.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Marker))
					.TextStyle(&FExtendedAtlassianDocumentStyle::Get().GetWidgetStyle<FTextBlockStyle>(TEXT("Doc.Marker")))
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				MakeRichText(Block.Markup, TEXT("Doc.Body"))
			];
	}

	default:
		return MakeRichText(Block.Markup, TEXT("Doc.Body"));
	}
}

TSharedRef<SWidget> SExtendedAtlassianDocumentView::BuildTableRow(const FExtendedAtlassianDocBlock& Block) const
{
	using namespace ExtendedAtlassianDocumentViewPrivate;

	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);

	const FName CellStyle = Block.bIsHeaderRow ? FName(TEXT("Doc.TableHeader")) : FName(TEXT("Doc.Body"));

	for (const FString& Cell : Block.Cells)
	{
		// Even fill rather than measured columns: real column sizing needs a table-wide pass, and
		// even columns read acceptably for the reference tables these pages actually contain.
		Row->AddSlot()
			.FillWidth(1.0f)
			.Padding(FMargin(0.0f, 2.0f, 12.0f, 2.0f))
			[
				MakeRichText(Cell, CellStyle)
			];
	}

	return SNew(SBorder)
		.BorderImage(Block.bIsHeaderRow
			? FAppStyle::GetBrush(TEXT("ToolPanel.GroupBorder"))
			: FAppStyle::GetBrush(TEXT("NoBorder")))
		.Padding(FMargin(4.0f, 1.0f))
		[
			Row
		];
}

#undef LOCTEXT_NAMESPACE
