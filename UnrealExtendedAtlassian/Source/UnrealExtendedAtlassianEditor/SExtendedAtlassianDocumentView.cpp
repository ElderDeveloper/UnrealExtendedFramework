// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "SExtendedAtlassianDocumentView.h"

#include "ExtendedAtlassianDocumentStyle.h"
#include "ExtendedAtlassianStyle.h"
#include "SBacklotStylePrimitives.h"

#include "Framework/Text/RichTextLayoutMarshaller.h"
#include "Framework/Text/SlateHyperlinkRun.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "Internationalization/Regex.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ExtendedAtlassianDocumentView"

namespace ExtendedAtlassianDocumentViewPrivate
{
	/** Indent per list nesting level. */
	constexpr float IndentPerLevel = 18.0f;

	const FButtonStyle& Button(const TCHAR* Name)
	{
		return FExtendedAtlassianStyle::Get().GetWidgetStyle<FButtonStyle>(Name);
	}

	const FTextBlockStyle& Text(const TCHAR* Name)
	{
		return FExtendedAtlassianStyle::Get().GetWidgetStyle<FTextBlockStyle>(Name);
	}

	const FSlateBrush* Brush(const TCHAR* Name)
	{
		return FExtendedAtlassianStyle::Get().GetBrush(Name);
	}

	FString PlainMarkup(FString Value)
	{
		int32 Open = INDEX_NONE;
		while (Value.FindChar(TEXT('<'), Open))
		{
			const int32 Close = Value.Find(
				TEXT(">"),
				ESearchCase::CaseSensitive,
				ESearchDir::FromStart,
				Open);
			if (Close == INDEX_NONE)
			{
				break;
			}
			Value.RemoveAt(Open, Close - Open + 1, EAllowShrinking::No);
		}
		Value.ReplaceInline(TEXT("&lt;"), TEXT("<"));
		Value.ReplaceInline(TEXT("&gt;"), TEXT(">"));
		Value.ReplaceInline(TEXT("&amp;"), TEXT("&"));
		Value.ReplaceInline(TEXT("&quot;"), TEXT("\""));
		return Value;
	}

	FLinearColor StatusColor(const FString& Status)
	{
		if (Status == TEXT("In progress"))
		{
			return FExtendedAtlassianStyle::FromHex(TEXT("#58a6ff"));
		}
		if (Status == TEXT("In review"))
		{
			return FExtendedAtlassianStyle::FromHex(TEXT("#c58fff"));
		}
		if (Status == TEXT("Blocked"))
		{
			return FExtendedAtlassianStyle::FromHex(TEXT("#f0665f"));
		}
		if (Status == TEXT("Done"))
		{
			return FExtendedAtlassianStyle::FromHex(TEXT("#57cc8a"));
		}
		return FExtendedAtlassianStyle::FromHex(TEXT("#a2a9b4"));
	}

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

	/** Read-only editable text keeps the rich layout while enabling selection and native copy. */
	TSharedRef<SMultiLineEditableText> MakeSelectableRichText(
		const FString& Markup,
		const FTextBlockStyle& TextStyle,
		bool bAutoWrap,
		float LineHeight,
		TArray<TSharedRef<ITextDecorator>> Decorators = {})
	{
		const TSharedRef<FRichTextLayoutMarshaller> Marshaller =
			FRichTextLayoutMarshaller::Create(
				MoveTemp(Decorators),
				&FExtendedAtlassianDocumentStyle::Get());
		return SNew(SMultiLineEditableText)
			.Text(FText::FromString(Markup))
			.Marshaller(Marshaller)
			.TextStyle(&TextStyle)
			.LineHeightPercentage(LineHeight)
			.AutoWrapText(bAutoWrap)
			.IsReadOnly(true)
			.AllowContextMenu(true);
	}

	TSharedRef<SMultiLineEditableText> MakeSelectableText(
		const FText& Value,
		const FTextBlockStyle& TextStyle,
		bool bAutoWrap = false,
		TOptional<ETextOverflowPolicy> OverflowPolicy = {})
	{
		return SNew(SMultiLineEditableText)
			.Text(Value)
			.TextStyle(&TextStyle)
			.AutoWrapText(bAutoWrap)
			.IsReadOnly(true)
			.AllowContextMenu(true)
			.OverflowPolicy(OverflowPolicy);
	}

	/** Selectable rich text wired to the document style set, with clickable links. */
	TSharedRef<SMultiLineEditableText> MakeRichText(
		const FString& Markup,
		const FName& TextStyle)
	{
		const float LineHeight =
			TextStyle == TEXT("Doc.Body")
				? 1.27f
				: TextStyle == TEXT("Doc.Quote")
					? 1.29f
					: 1.0f;
		TArray<TSharedRef<ITextDecorator>> Decorators;
		Decorators.Add(SRichTextBlock::HyperlinkDecorator(
			TEXT("a"),
			FSlateHyperlinkRun::FOnClick::CreateStatic(&OpenLink)));
		return MakeSelectableRichText(
			Markup,
			FExtendedAtlassianDocumentStyle::Get()
				.GetWidgetStyle<FTextBlockStyle>(TextStyle),
			true,
			LineHeight,
			MoveTemp(Decorators));
	}
}

void SExtendedAtlassianDocumentView::Construct(const FArguments& InArgs)
{
	MaxReadingWidth = InArgs._MaxReadingWidth;
	OnTaskToggled = InArgs._OnTaskToggled;
	OnIssueClicked = InArgs._OnIssueClicked;
	OnAssetClicked = InArgs._OnAssetClicked;

	ChildSlot
	[
		SNew(SBox)
		.MaxDesiredWidth(MaxReadingWidth)
		.HAlign(HAlign_Left)
		[
			SAssignNew(ContentBox, SVerticalBox)
		]
	];
}

void SExtendedAtlassianDocumentView::Clear()
{
	Blocks.Reset();
	VisibleBlockLimit = 0;
	if (ContentBox.IsValid())
	{
		ContentBox->ClearChildren();
	}
}

void SExtendedAtlassianDocumentView::SetBlocks(const TArray<FExtendedAtlassianDocBlock>& InBlocks)
{
	Blocks = InBlocks;
	VisibleBlockLimit = FMath::Min(Blocks.Num(), 120);
	RebuildVisibleBlocks();
}

void SExtendedAtlassianDocumentView::RebuildVisibleBlocks()
{
	if (!ContentBox.IsValid())
	{
		return;
	}

	ContentBox->ClearChildren();

	if (Blocks.IsEmpty())
	{
		ContentBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 40.0f)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT(
				"EmptyDocument",
				"This page is empty. Hit Edit page and use the / menu to add a block."))
			.TextStyle(&ExtendedAtlassianDocumentViewPrivate::Text(TEXT("Backlot.Sans.12")))
			.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")))
		];
	}

	float PreviousBottomMargin = 0.0f;
	for (int32 Index = 0; Index < VisibleBlockLimit;)
	{
		const FExtendedAtlassianDocBlock& Block = Blocks[Index];
		int32 LastIndex = Index;
		if (Block.Kind == EExtendedAtlassianBlockKind::OrderedItem
			|| Block.Kind == EExtendedAtlassianBlockKind::TableRow
			|| Block.Kind == EExtendedAtlassianBlockKind::TaskItem
			|| Block.Kind == EExtendedAtlassianBlockKind::Image)
		{
			while (LastIndex + 1 < VisibleBlockLimit
				&& Blocks[LastIndex + 1].Kind == Block.Kind)
			{
				++LastIndex;
			}
		}

		TSharedRef<SWidget> Widget =
			Block.Kind == EExtendedAtlassianBlockKind::OrderedItem
				? BuildOrderedRules(Blocks, Index, LastIndex)
				: Block.Kind == EExtendedAtlassianBlockKind::TableRow
					? BuildTable(Blocks, Index, LastIndex)
					: Block.Kind == EExtendedAtlassianBlockKind::TaskItem
						? BuildTasks(Blocks, Index, LastIndex)
						: Block.Kind == EExtendedAtlassianBlockKind::Image
							? BuildAssets(Blocks, Index, LastIndex)
							: BuildBlockWidget(Block, Index);

		const float Top =
			Block.Kind == EExtendedAtlassianBlockKind::Heading ? 34.0f : 0.0f;
		const float Bottom =
			Block.Kind == EExtendedAtlassianBlockKind::Heading ? 13.0f
			: Block.Kind == EExtendedAtlassianBlockKind::Paragraph ? 22.0f
			: Block.Kind == EExtendedAtlassianBlockKind::Quote ? 26.0f
			: Block.Kind == EExtendedAtlassianBlockKind::OrderedItem ? 12.0f
			: Block.Kind == EExtendedAtlassianBlockKind::TableRow ? 8.0f
			: Block.Kind == EExtendedAtlassianBlockKind::CodeBlock ? 26.0f
			: Block.Kind == EExtendedAtlassianBlockKind::TaskItem ? 22.0f
			: Block.Kind == EExtendedAtlassianBlockKind::Image ? 26.0f
			: 8.0f;
		// CSS collapses adjacent vertical margins. Slate padding is additive, so
		// subtract the preceding bottom margin from the next top margin.
		const float CollapsedTop =
			FMath::Max(0.0f, Top - PreviousBottomMargin);
		ContentBox->AddSlot()
		.AutoHeight()
		.Padding(FMargin(
			Block.IndentDepth
				* ExtendedAtlassianDocumentViewPrivate::IndentPerLevel,
			CollapsedTop,
			0.0f,
			Bottom))
		[
			Widget
		];
		PreviousBottomMargin = Bottom;
		Index = LastIndex + 1;
	}

	if (VisibleBlockLimit < Blocks.Num())
	{
		const int32 Remaining = Blocks.Num() - VisibleBlockLimit;
		const int32 NextCount = FMath::Min(120, Remaining);
		ContentBox->AddSlot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 12.0f, 0.0f, 28.0f)
		[
			SNew(SButton)
			.ButtonStyle(&ExtendedAtlassianDocumentViewPrivate::Button(
				TEXT("Backlot.Button.Secondary")))
			.ContentPadding(FMargin(14.0f, 7.0f))
			.OnClicked(this, &SExtendedAtlassianDocumentView::LoadMoreBlocks)
			[
				SNew(STextBlock)
				.Text(FText::Format(
					LOCTEXT(
						"LoadMoreDocumentBlocks",
						"LOAD {0} MORE BLOCKS ({1} REMAINING)"),
					FText::AsNumber(NextCount),
					FText::AsNumber(Remaining)))
				.TextStyle(&ExtendedAtlassianDocumentViewPrivate::Text(
					TEXT("Backlot.Mono.10")))
			]
		];
	}
}

FReply SExtendedAtlassianDocumentView::LoadMoreBlocks()
{
	VisibleBlockLimit = FMath::Min(Blocks.Num(), VisibleBlockLimit + 120);
	RebuildVisibleBlocks();
	return FReply::Handled();
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

TSharedRef<SWidget> SExtendedAtlassianDocumentView::BuildBlockWidget(
	const FExtendedAtlassianDocBlock& Block,
	int32 BlockIndex) const
{
	using namespace ExtendedAtlassianDocumentViewPrivate;
	(void)BlockIndex;

	switch (Block.Kind)
	{
	case EExtendedAtlassianBlockKind::Heading:
		return MakeRichText(Block.Markup, StyleForHeading(Block.Level));

	case EExtendedAtlassianBlockKind::Rule:
		return SNew(SSeparator).Thickness(1.0f);

	case EExtendedAtlassianBlockKind::CodeBlock:
	{
		const FString ReferenceSnippet =
			TEXT("// blend the dry authoring toward the wet response\n")
			TEXT("float  wet    = saturate(WetnessMask * RainIntensity);\n")
			TEXT("float3 albedo = lerp(DryAlbedo, DryAlbedo * 0.62, wet);\n")
			TEXT("float  rough  = lerp(DryRoughness, 0.08, wet * PorosityScale);\n")
			TEXT("return MakeSurface(albedo, rough, PuddleNormal(wet));");
		const bool bReferenceSnippet = Block.RawText == ReferenceSnippet;
		const FString CodeMarkup = bReferenceSnippet
			? FString(
				TEXT("<SyntaxComment>// blend the dry authoring toward the wet response</>\n")
				TEXT("<SyntaxKeyword>float</>  wet    = saturate(WetnessMask * RainIntensity);\n")
				TEXT("<SyntaxKeyword>float3</> albedo = lerp(DryAlbedo, DryAlbedo * <SyntaxNumber>0.62</>, wet);\n")
				TEXT("<SyntaxKeyword>float</>  rough  = lerp(DryRoughness, <SyntaxNumber>0.08</>, wet * PorosityScale);\n")
				TEXT("<SyntaxKeyword>return</> MakeSurface(albedo, rough, PuddleNormal(wet));"))
			: FExtendedAtlassianMarkup::Escape(Block.RawText);
		FTextBlockStyle CodeLanguageStyle = Text(TEXT("Backlot.Mono.9"));
		CodeLanguageStyle.SetColorAndOpacity(
			FSlateColor(FExtendedAtlassianStyle::FromHex(TEXT("#58a6ff"))));
		FTextBlockStyle CodeFileStyle = Text(TEXT("Backlot.Mono.10"));
		CodeFileStyle.SetColorAndOpacity(
			FSlateColor(FExtendedAtlassianStyle::FromHex(TEXT("#5c636d"))));
		return SNew(SBorder)
			.BorderImage(Brush(TEXT("Backlot.Brush.DocumentCode")))
			.Padding(0.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBorder)
					.BorderImage(Brush(TEXT("Backlot.Brush.FieldAlt")))
					.Padding(FMargin(13.0f, 8.0f))
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							MakeSelectableText(
								FText::FromString(
									Block.CodeLanguage.IsEmpty()
										? FString(TEXT("TEXT"))
										: Block.CodeLanguage.ToUpper()),
								CodeLanguageStyle)
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.Padding(12.0f, 0.0f)
						[
							MakeSelectableText(
								FText::FromString(Block.ImageAlt),
								CodeFileStyle)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SButton)
							.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
							.ContentPadding(0.0f)
							.OnClicked_Lambda(
								[this, Code = Block.RawText]()
								{
									FPlatformApplicationMisc::ClipboardCopy(*Code);
									bCodeCopied = true;
									const_cast<SExtendedAtlassianDocumentView*>(this)
										->RegisterActiveTimer(
											1.6f,
											FWidgetActiveTimerDelegate::CreateSP(
												const_cast<
													SExtendedAtlassianDocumentView*>(
														this),
												&SExtendedAtlassianDocumentView::
													HandleCopyTimer));
									return FReply::Handled();
								})
							[
								SNew(STextBlock)
								.Text_Lambda(
									[this]()
									{
										return bCodeCopied
											? LOCTEXT("CopiedCode", "COPIED")
											: LOCTEXT("CopyCode", "COPY");
									})
								.TextStyle(&Text(TEXT("Backlot.Mono.9")))
								.ColorAndOpacity_Lambda(
									[this]()
									{
										return bCodeCopied
											? FExtendedAtlassianStyle::FromHex(
												TEXT("#57cc8a"))
											: FExtendedAtlassianStyle::FromHex(
												TEXT("#6f7783"));
									})
							]
						]
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(16.0f, 14.0f))
				[
					// Code must not wrap; wrapping changes what the snippet appears to say.
					SNew(SScrollBox)
					.Orientation(Orient_Horizontal)
					+ SScrollBox::Slot()
					[
						MakeSelectableRichText(
							CodeMarkup,
							FExtendedAtlassianDocumentStyle::Get()
								.GetWidgetStyle<FTextBlockStyle>(
									TEXT("Doc.CodeBlock")),
							false,
							1.0f)
					]
				]
			];
	}

	case EExtendedAtlassianBlockKind::Quote:
		return SNew(SBorder)
			.BorderImage(Brush(TEXT("Backlot.Brush.DocumentCallout")))
			.Padding(FMargin(17.0f, 15.68f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 2.0f, 13.0f, 0.0f)
				[
					SNew(SBox)
					.WidthOverride(17.0f)
					.HeightOverride(17.0f)
					[
						SNew(SBorder)
						.BorderImage(Brush(TEXT("Backlot.Brush.Blue")))
						.Padding(0.0f)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("CalloutInfo", "i"))
							.TextStyle(&Text(TEXT("Backlot.Mono.10.Medium")))
							.ColorAndOpacity(
								FExtendedAtlassianStyle::FromHex(TEXT("#58a6ff")))
						]
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					MakeRichText(Block.Markup, TEXT("Doc.Quote"))
				]
			];

	case EExtendedAtlassianBlockKind::BulletItem:
	{
		return SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SBox)
				.MinDesiredWidth(Block.Kind == EExtendedAtlassianBlockKind::Paragraph ? 0.0f : 18.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("BulletMarker", "•"))
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

TSharedRef<SWidget> SExtendedAtlassianDocumentView::BuildOrderedRules(
	const TArray<FExtendedAtlassianDocBlock>& InBlocks,
	int32 FirstIndex,
	int32 LastIndex) const
{
	using namespace ExtendedAtlassianDocumentViewPrivate;
	TSharedRef<SVerticalBox> Rules = SNew(SVerticalBox);
	for (int32 Index = FirstIndex; Index <= LastIndex; ++Index)
	{
		const FExtendedAtlassianDocBlock& Block = InBlocks[Index];
		const FString Plain = PlainMarkup(Block.Markup);
		FString IssueKey;
		FRegexMatcher Matcher(
			FRegexPattern(TEXT("\\b[A-Z][A-Z0-9]+-[0-9]+\\b")),
			Plain);
		if (Matcher.FindNext())
		{
			IssueKey = Matcher.GetCaptureGroup(0);
		}
		FString RuleText = Plain;
		if (!IssueKey.IsEmpty())
		{
			RuleText.ReplaceInline(*(IssueKey + TEXT(".")), TEXT(""));
			RuleText.ReplaceInline(*IssueKey, TEXT(""));
			RuleText.TrimStartAndEndInline();
		}
		const FString Status = IssueStatuses.FindRef(IssueKey);
		const FName IssueDecorator =
			Status == TEXT("Blocked")
				? FName(TEXT("IssueBlocked"))
				: Status == TEXT("In review")
					? FName(TEXT("IssueReview"))
					: Status == TEXT("Done")
						? FName(TEXT("IssueDone"))
						: FName(TEXT("IssueProgress"));
		const FString RuleMarkup =
			FExtendedAtlassianMarkup::Escape(RuleText)
			+ (IssueKey.IsEmpty()
				? FString()
				: FString::Printf(
					TEXT("\n<a id=\"%s\" style=\"%s\" href=\"%s\">%s\u00a0\u00a0%s</> "),
					*IssueDecorator.ToString(),
					*IssueDecorator.ToString(),
					*IssueKey,
					*IssueKey,
					*Status));
		TArray<TSharedRef<ITextDecorator>> Decorators;
		Decorators.Add(SRichTextBlock::HyperlinkDecorator(
			IssueDecorator.ToString(),
			FSlateHyperlinkRun::FOnClick::CreateLambda(
				[this](const FSlateHyperlinkRun::FMetadata& Metadata)
				{
					if (const FString* Href = Metadata.Find(TEXT("href")))
					{
						OnIssueClicked.ExecuteIfBound(*Href);
					}
				})));
		Rules->AddSlot()
		.AutoHeight()
		.Padding(0.0f, Index == FirstIndex ? 0.0f : 11.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 12.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(18.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(FString::Printf(
						TEXT("%02d"),
						Index - FirstIndex + 1)))
					.TextStyle(&Text(TEXT("Backlot.Mono.12")))
					.ColorAndOpacity(
						FExtendedAtlassianStyle::FromHex(TEXT("#58a6ff")))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				MakeSelectableRichText(
					RuleMarkup,
					Text(TEXT("Backlot.Sans.14.5")),
					true,
					1.297f,
					MoveTemp(Decorators))
			]
		];
	}
	return Rules;
}

TSharedRef<SWidget> SExtendedAtlassianDocumentView::BuildTable(
	const TArray<FExtendedAtlassianDocBlock>& InBlocks,
	int32 FirstIndex,
	int32 LastIndex) const
{
	using namespace ExtendedAtlassianDocumentViewPrivate;
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	for (int32 Index = FirstIndex; Index <= LastIndex; ++Index)
	{
		Rows->AddSlot()
		.AutoHeight()
		[
			BuildTableRow(InBlocks[Index])
		];
	}
	return SNew(SBorder)
		.BorderImage(Brush(TEXT("Backlot.Brush.DocumentTable")))
		.Padding(0.0f)
		[
			Rows
		];
}

TSharedRef<SWidget> SExtendedAtlassianDocumentView::BuildTasks(
	const TArray<FExtendedAtlassianDocBlock>& InBlocks,
	int32 FirstIndex,
	int32 LastIndex) const
{
	using namespace ExtendedAtlassianDocumentViewPrivate;
	TSharedRef<SVerticalBox> Tasks = SNew(SVerticalBox);
	for (int32 Index = FirstIndex; Index <= LastIndex; ++Index)
	{
		const FExtendedAtlassianDocBlock& Block = InBlocks[Index];
		FTextBlockStyle TaskTextStyle = Text(TEXT("Backlot.Sans.14"));
		TaskTextStyle.SetColorAndOpacity(
			FSlateColor(Block.bChecked
				? FExtendedAtlassianStyle::FromHex(TEXT("#6f7783"))
				: FExtendedAtlassianStyle::FromHex(TEXT("#c9cfd8"))));
		Tasks->AddSlot()
		.AutoHeight()
		.Padding(0.0f, Index == FirstIndex ? 0.0f : 9.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 11.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(15.0f)
				.HeightOverride(15.0f)
				[
					SNew(SButton)
					.ButtonStyle(&Button(
						Block.bChecked
							? TEXT("Backlot.Button.Primary")
							: TEXT("Backlot.Button.Secondary")))
					.ContentPadding(0.0f)
					.OnClicked_Lambda(
						[this, Index]()
						{
							OnTaskToggled.ExecuteIfBound(Index);
							return FReply::Handled();
						})
					[
						SNew(STextBlock)
							.Text(
								Block.bChecked
									? LOCTEXT("TaskDone", "✓")
									: FText::GetEmpty())
							.TextStyle(&Text(TEXT("Backlot.Mono.9")))
							.ColorAndOpacity(
								FExtendedAtlassianStyle::FromHex(TEXT("#0f1114")))
					]
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				MakeSelectableText(
					FText::FromString(PlainMarkup(Block.Markup)),
					TaskTextStyle,
					true)
			]
		];
	}
	return Tasks;
}

TSharedRef<SWidget> SExtendedAtlassianDocumentView::BuildAssets(
	const TArray<FExtendedAtlassianDocBlock>& InBlocks,
	int32 FirstIndex,
	int32 LastIndex) const
{
	using namespace ExtendedAtlassianDocumentViewPrivate;
	TSharedRef<SWrapBox> Assets =
		SNew(SWrapBox)
		.UseAllottedSize(true)
		.InnerSlotPadding(FVector2D(12.0f, 12.0f));
	const FTextBlockStyle& AssetNameStyle = Text(TEXT("Backlot.Mono.11"));
	FTextBlockStyle AssetMetaStyle = Text(TEXT("Backlot.Mono.10"));
	AssetMetaStyle.SetColorAndOpacity(
		FSlateColor(FExtendedAtlassianStyle::FromHex(TEXT("#5c636d"))));
	for (int32 Index = FirstIndex; Index <= LastIndex; ++Index)
	{
		const FExtendedAtlassianDocBlock& Block = InBlocks[Index];
		Assets->AddSlot()
		[
			SNew(SBox)
			.WidthOverride(190.0f)
			[
				SNew(SBorder)
				.BorderImage(Brush(TEXT("Backlot.Brush.DocumentTable")))
				.Padding(0.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SBox)
						.HeightOverride(96.0f)
						[
							SNew(SButton)
							.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
							.ContentPadding(0.0f)
							.OnClicked_Lambda(
								[
									this,
									Name = Block.ImageAlt,
									Target = Block.ImageUrl,
									Meta = Block.ImageMeta
								]()
								{
									OnAssetClicked.ExecuteIfBound(
										Name,
										Target.IsEmpty() ? Meta : Target);
									return FReply::Handled();
								})
							[
								SNew(SBacklotDiagonalPattern)
								.ColorA(FExtendedAtlassianStyle::FromHex(TEXT("#22262c")))
								.ColorB(FExtendedAtlassianStyle::FromHex(TEXT("#1c2025")))
								.StripeWidth(7.0f)
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								[
									SNew(STextBlock)
									.Text(FText::FromString(
										Block.EmbedSlot.IsEmpty()
											? FString(TEXT("ASSET"))
											: Block.EmbedSlot))
									.TextStyle(&Text(TEXT("Backlot.Mono.9")))
									.ColorAndOpacity(
										FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")))
								]
							]
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FMargin(11.0f, 9.0f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							MakeSelectableText(
								FText::FromString(Block.ImageAlt),
								AssetNameStyle,
								false,
								ETextOverflowPolicy::Ellipsis)
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 3.0f, 0.0f, 0.0f)
						[
							MakeSelectableText(
								FText::FromString(
									Block.ImageMeta.IsEmpty()
										? Block.ImageUrl
										: Block.ImageMeta),
								AssetMetaStyle,
								false,
								ETextOverflowPolicy::Ellipsis)
						]
					]
				]
			]
		];
	}
	return Assets;
}

TSharedRef<SWidget> SExtendedAtlassianDocumentView::BuildTableRow(const FExtendedAtlassianDocBlock& Block) const
{
	using namespace ExtendedAtlassianDocumentViewPrivate;

	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);

	for (int32 CellIndex = 0; CellIndex < Block.Cells.Num(); ++CellIndex)
	{
		const FString& Cell = Block.Cells[CellIndex];
		const FName CellStyle =
			Block.bIsHeaderRow
				? FName(TEXT("Doc.TableHeader"))
				: CellIndex == 0
					? FName(TEXT("Doc.TableKey"))
					: CellIndex == 1
						? FName(TEXT("Doc.TableRange"))
						: CellIndex == 2
							? FName(TEXT("Doc.TableValue"))
							: FName(TEXT("Doc.TableOwner"));
		Row->AddSlot()
			.FillWidth(1.0f)
			.Padding(FMargin(14.0f, Block.bIsHeaderRow ? 9.0f : 11.0f))
			[
				MakeRichText(Cell, CellStyle)
			];
	}

	return SNew(SBorder)
		.BorderImage(Block.bIsHeaderRow
			? Brush(TEXT("Backlot.Brush.FieldAlt"))
			: FStyleDefaults::GetNoBrush())
		.Padding(0.0f)
		[
			Row
		];
}

EActiveTimerReturnType SExtendedAtlassianDocumentView::HandleCopyTimer(
	double,
	float)
{
	bCodeCopied = false;
	return EActiveTimerReturnType::Stop;
}

#undef LOCTEXT_NAMESPACE
