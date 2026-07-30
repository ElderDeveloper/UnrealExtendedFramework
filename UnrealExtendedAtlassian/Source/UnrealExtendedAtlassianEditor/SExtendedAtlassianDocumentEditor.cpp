// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "SExtendedAtlassianDocumentEditor.h"

#include "ExtendedAtlassianMarkdown.h"
#include "ExtendedAtlassianStyle.h"

#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ExtendedAtlassianDocumentEditor"

namespace ExtendedAtlassianDocumentEditorPrivate
{
	constexpr float ChangeNotificationSeconds = 0.18f;

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
			int32 Close = INDEX_NONE;
			if (!Value.FindChar(TEXT('>'), Close) || Close < Open)
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

	struct FBlockPresentation
	{
		const TCHAR* Label;
		const TCHAR* Glyph;
		const TCHAR* Hint;
		const TCHAR* Color;
	};

	FBlockPresentation Presentation(EExtendedAtlassianBlockKind Kind)
	{
		switch (Kind)
		{
		case EExtendedAtlassianBlockKind::Heading:
			return { TEXT("Heading"), TEXT("H"), TEXT("H2"), TEXT("#e6e8ec") };
		case EExtendedAtlassianBlockKind::Quote:
			return { TEXT("Callout"), TEXT("!"), TEXT("INFO"), TEXT("#e3a54a") };
		case EExtendedAtlassianBlockKind::OrderedItem:
			return { TEXT("Numbered rules"), TEXT("1."), TEXT("LIST"), TEXT("#58a6ff") };
		case EExtendedAtlassianBlockKind::TableRow:
			return { TEXT("Table"), TEXT("▦"), TEXT("4 COL"), TEXT("#a2a9b4") };
		case EExtendedAtlassianBlockKind::CodeBlock:
			return { TEXT("Code block"), TEXT("{ }"), TEXT("C++"), TEXT("#58a6ff") };
		case EExtendedAtlassianBlockKind::TaskItem:
			return { TEXT("To-do list"), TEXT("☑"), TEXT("TASK"), TEXT("#57cc8a") };
		case EExtendedAtlassianBlockKind::Image:
			return { TEXT("Asset embed"), TEXT("◈"), TEXT("DROP"), TEXT("#b6a9ff") };
		default:
			return { TEXT("Paragraph"), TEXT("¶"), TEXT("TEXT"), TEXT("#a2a9b4") };
		}
	}
}

void SExtendedAtlassianDocumentEditor::Construct(const FArguments& InArgs)
{
	using namespace ExtendedAtlassianDocumentEditorPrivate;
	OnMarkdownChanged = InArgs._OnMarkdownChanged;
	SlashMenuCurve = SlashMenuSequence.AddCurve(
		0.0f,
		0.13f,
		ECurveEaseFunction::QuadOut);
	SlashMenuSequence.JumpToEnd();

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
				.BorderImage(Brush(TEXT("Backlot.Brush.Card")))
				.Padding(FMargin(11.0f, 8.0f))
				.Visibility_Lambda(
					[this]()
					{
						return bReadOnly
							? EVisibility::Visible
							: EVisibility::Collapsed;
					})
				[
					SNew(STextBlock)
						.Text_Lambda([this]() { return ReadOnlyReason; })
						.TextStyle(&Text(TEXT("Backlot.Sans.11")))
						.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#e3a54a")))
						.AutoWrapText(true)
				]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SScrollBox)
				.ScrollBarStyle(
					&FExtendedAtlassianStyle::Get().GetWidgetStyle<FScrollBarStyle>(
						TEXT("Backlot.ScrollBar")))
				+ SScrollBox::Slot()
				.Padding(0.0f, 4.0f, 0.0f, 20.0f)
				[
					SNew(SBox)
					.MaxDesiredWidth(710.4f)
					.HAlign(HAlign_Left)
					[
						SAssignNew(ContentBox, SVerticalBox)
					]
				]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			BuildBlockPalette()
		]
	];
	if (SlashMenuBox.IsValid())
	{
		SlashMenuBox->SetRenderTransformPivot(FVector2D(0.5f, 0.0f));
		SlashMenuBox->SetRenderTransform(
			TAttribute<TOptional<FSlateRenderTransform>>::CreateLambda(
				[this]()
				{
					const float Alpha = SlashMenuCurve.GetLerp();
					return TOptional<FSlateRenderTransform>(
						FSlateRenderTransform(
							FMath::Lerp(0.99f, 1.0f, Alpha),
							FVector2D(
								0.0f,
								FMath::Lerp(6.0f, 0.0f, Alpha))));
				}));
	}

	RegisterActiveTimer(
		ChangeNotificationSeconds,
		FWidgetActiveTimerDelegate::CreateSP(
			this,
			&SExtendedAtlassianDocumentEditor::HandlePreviewTimer));
	RebuildBlocks();
}

void SExtendedAtlassianDocumentEditor::Tick(
	const FGeometry& AllottedGeometry,
	double InCurrentTime,
	float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (SlashMenuBox.IsValid())
	{
		SlashMenuBox->SetRenderOpacity(SlashMenuCurve.GetLerp());
	}
}

void SExtendedAtlassianDocumentEditor::SetMarkdown(const FString& InMarkdown)
{
	Markdown = InMarkdown;
	Blocks = FExtendedAtlassianMarkdown::ToBlocks(Markdown);
	bDirty = false;
	bChangeNotificationPending = false;
	RebuildBlocks();
}

void SExtendedAtlassianDocumentEditor::SetReadOnly(
	bool bInReadOnly,
	const FText& Reason)
{
	bReadOnly = bInReadOnly;
	ReadOnlyReason = Reason;
}

void SExtendedAtlassianDocumentEditor::RebuildBlocks()
{
	using namespace ExtendedAtlassianDocumentEditorPrivate;
	if (!ContentBox.IsValid())
	{
		return;
	}
	ContentBox->ClearChildren();
	if (Blocks.IsEmpty())
	{
		ContentBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 44.0f)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock)
				.Text(LOCTEXT(
					"EmptyBlocks",
					"THIS PAGE IS EMPTY\nChoose a block below to start writing."))
				.Justification(ETextJustify::Center)
				.TextStyle(&Text(TEXT("Backlot.Mono.10")))
				.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")))
		];
		return;
	}

	for (int32 BlockIndex = 0; BlockIndex < Blocks.Num(); ++BlockIndex)
	{
		const EExtendedAtlassianBlockKind Kind = Blocks[BlockIndex].Kind;
		const float Top =
			Kind == EExtendedAtlassianBlockKind::Heading ? 34.0f : 0.0f;
		const float Bottom =
			Kind == EExtendedAtlassianBlockKind::Heading ? 13.0f
			: Kind == EExtendedAtlassianBlockKind::Quote
				|| Kind == EExtendedAtlassianBlockKind::CodeBlock
				|| Kind == EExtendedAtlassianBlockKind::Image
				? 26.0f
			: Kind == EExtendedAtlassianBlockKind::Paragraph
				|| Kind == EExtendedAtlassianBlockKind::TaskItem
				? 22.0f
				: 8.0f;
		ContentBox->AddSlot()
		.AutoHeight()
		.Padding(0.0f, Top, 0.0f, Bottom)
		[
			BuildBlockEditor(BlockIndex)
		];
	}
}

TSharedRef<SWidget> SExtendedAtlassianDocumentEditor::BuildBlockEditor(
	int32 BlockIndex)
{
	using namespace ExtendedAtlassianDocumentEditorPrivate;
	const FExtendedAtlassianDocBlock& Block = Blocks[BlockIndex];
	const FBlockPresentation Info = Presentation(Block.Kind);

	TSharedRef<SVerticalBox> Fields = SNew(SVerticalBox);
	switch (Block.Kind)
	{
	case EExtendedAtlassianBlockKind::CodeBlock:
		Fields->AddSlot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.35f)
			[
				SNew(SEditableTextBox)
					.Style(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(
						TEXT("Backlot.Field")))
					.HintText(LOCTEXT("CodeLanguageHint", "LANGUAGE"))
					.Text(FText::FromString(Block.CodeLanguage))
					.IsReadOnly_Lambda([this]() { return bReadOnly; })
					.OnTextChanged(
						this,
						&SExtendedAtlassianDocumentEditor::SetCodeLanguage,
						BlockIndex)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.65f)
			.Padding(6.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SEditableTextBox)
					.Style(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(
						TEXT("Backlot.Field")))
					.HintText(LOCTEXT("CodeFileHint", "FILE"))
					.Text(FText::FromString(Block.ImageAlt))
					.IsReadOnly_Lambda([this]() { return bReadOnly; })
					.OnTextChanged(
						this,
						&SExtendedAtlassianDocumentEditor::SetCodeFile,
						BlockIndex)
			]
		];
		Fields->AddSlot()
		.AutoHeight()
		.Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			SNew(SMultiLineEditableTextBox)
				.Style(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(
					TEXT("Backlot.Field")))
				.Text(FText::FromString(Block.RawText))
				.IsReadOnly_Lambda([this]() { return bReadOnly; })
				.Font(Text(TEXT("Backlot.Mono.12")).Font)
				.OnTextChanged(
					this,
					&SExtendedAtlassianDocumentEditor::SetRawText,
					BlockIndex)
		];
		break;

	case EExtendedAtlassianBlockKind::TableRow:
		{
			TSharedRef<SHorizontalBox> Cells = SNew(SHorizontalBox);
			for (int32 CellIndex = 0; CellIndex < Block.Cells.Num(); ++CellIndex)
			{
				Cells->AddSlot()
				.FillWidth(1.0f)
				.Padding(CellIndex == 0 ? 0.0f : 4.0f, 0.0f)
				[
					SNew(SEditableTextBox)
						.Style(
							&FExtendedAtlassianStyle::Get().GetWidgetStyle<
								FEditableTextBoxStyle>(TEXT("Backlot.Field")))
						.Text(FText::FromString(PlainMarkup(Block.Cells[CellIndex])))
						.IsReadOnly_Lambda([this]() { return bReadOnly; })
						.OnTextChanged(
							this,
							&SExtendedAtlassianDocumentEditor::SetTableCell,
							BlockIndex,
							CellIndex)
				];
			}
			Fields->AddSlot().AutoHeight()[Cells];
		}
		break;

	case EExtendedAtlassianBlockKind::TaskItem:
		Fields->AddSlot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 2.0f, 8.0f, 0.0f)
			[
				SNew(SButton)
					.ButtonStyle(&Button(TEXT("Backlot.Button.Secondary")))
					.ContentPadding(FMargin(7.0f, 5.0f))
					.IsEnabled_Lambda([this]() { return !bReadOnly; })
					.OnClicked_Lambda(
						[this, BlockIndex]()
						{
							ToggleTask(BlockIndex);
							return FReply::Handled();
						})
					[
						SNew(STextBlock)
							.Text(Block.bChecked
								? LOCTEXT("TaskChecked", "✓")
								: LOCTEXT("TaskOpen", " "))
							.TextStyle(&Text(TEXT("Backlot.Mono.10.Medium")))
							.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#57cc8a")))
					]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SEditableTextBox)
					.Style(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(
						TEXT("Backlot.Field")))
					.Text(FText::FromString(PlainMarkup(Block.Markup)))
					.IsReadOnly_Lambda([this]() { return bReadOnly; })
					.OnTextChanged(
						this,
						&SExtendedAtlassianDocumentEditor::SetMarkupText,
						BlockIndex)
			]
		];
		break;

	case EExtendedAtlassianBlockKind::Image:
		Fields->AddSlot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.55f)
			[
				SNew(SEditableTextBox)
					.Style(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(
						TEXT("Backlot.Field")))
					.HintText(LOCTEXT("AssetNameHint", "ASSET NAME"))
					.Text(FText::FromString(Block.ImageAlt))
					.IsReadOnly_Lambda([this]() { return bReadOnly; })
					.OnTextChanged_Lambda(
						[this, BlockIndex](const FText& Value)
						{
							if (Blocks.IsValidIndex(BlockIndex))
							{
								Blocks[BlockIndex].ImageAlt = Value.ToString();
								CommitBlocks();
							}
						})
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.45f)
			.Padding(6.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SEditableTextBox)
					.Style(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(
						TEXT("Backlot.Field")))
					.HintText(LOCTEXT("AssetMetaHint", "META"))
					.Text(FText::FromString(
						Block.ImageMeta.IsEmpty()
							? Block.ImageUrl
							: Block.ImageMeta))
					.IsReadOnly_Lambda([this]() { return bReadOnly; })
					.OnTextChanged_Lambda(
						[this, BlockIndex](const FText& Value)
						{
							if (Blocks.IsValidIndex(BlockIndex))
							{
								Blocks[BlockIndex].ImageMeta = Value.ToString();
								CommitBlocks();
							}
						})
			]
		];
		break;

	default:
		Fields->AddSlot()
		.AutoHeight()
		[
			SNew(SMultiLineEditableTextBox)
				.Style(&FExtendedAtlassianStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(
					TEXT("Backlot.Field")))
				.Text(FText::FromString(PlainMarkup(Block.Markup)))
				.IsReadOnly_Lambda([this]() { return bReadOnly; })
				.Font(
					Block.Kind == EExtendedAtlassianBlockKind::Heading
						? Text(TEXT("Backlot.Sans.20.Semibold")).Font
						: Text(TEXT("Backlot.Sans.14")).Font)
				.OnTextChanged(
					this,
					&SExtendedAtlassianDocumentEditor::SetMarkupText,
					BlockIndex)
		];
		break;
	}

	TSharedRef<SHorizontalBox> ToolCluster = SNew(SHorizontalBox);
	auto AddTool = [&ToolCluster, this, BlockIndex](
		const FText& Label,
		const FText& Tooltip,
		bool bEnabled,
		TFunction<void()> Action,
		const FString& Color,
		bool bLast)
	{
		ToolCluster->AddSlot()
		.AutoWidth()
		.Padding(0.0f, 0.0f, bLast ? 0.0f : 2.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(21.0f)
			.HeightOverride(21.0f)
			[
				SNew(SButton)
				.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
				.ContentPadding(0.0f)
				.ToolTipText(Tooltip)
				.IsEnabled(bEnabled)
				.OnClicked_Lambda(
					[Action = MoveTemp(Action)]()
					{
						Action();
						return FReply::Handled();
					})
				[
					SNew(STextBlock)
						.Text(Label)
						.TextStyle(&Text(TEXT("Backlot.Mono.10")))
						.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(*Color))
						.Justification(ETextJustify::Center)
				]
			]
		];
	};
	AddTool(
		LOCTEXT("InsertAfterBlock", "+"),
		LOCTEXT("InsertAfterBlockTip", "Insert a block after this one"),
		!bReadOnly,
		[this, BlockIndex]() { ToggleInsertMenu(BlockIndex); },
		TEXT("#58a6ff"),
		false);
	AddTool(
		LOCTEXT("MoveBlockUp", "↑"),
		LOCTEXT("MoveBlockUpTip", "Move block up"),
		!bReadOnly && BlockIndex > 0,
		[this, BlockIndex]() { MoveBlock(BlockIndex, -1); },
		TEXT("#a2a9b4"),
		false);
	AddTool(
		LOCTEXT("MoveBlockDown", "↓"),
		LOCTEXT("MoveBlockDownTip", "Move block down"),
		!bReadOnly && BlockIndex + 1 < Blocks.Num(),
		[this, BlockIndex]() { MoveBlock(BlockIndex, 1); },
		TEXT("#a2a9b4"),
		false);
	AddTool(
		LOCTEXT("RemoveBlock", "×"),
		LOCTEXT("RemoveBlockTip", "Remove block"),
		!bReadOnly,
		[this, BlockIndex]() { RemoveBlock(BlockIndex); },
		TEXT("#f0665f"),
		true);

	TSharedRef<SVerticalBox> Result = SNew(SVerticalBox);
	Result->AddSlot()
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			SNew(SBox)
			.WidthOverride(90.0f)
			[
				ToolCluster
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SBorder)
			.BorderImage(Brush(TEXT("Backlot.Brush.FieldAlt")))
			.Padding(FMargin(9.0f, 8.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 6.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(
						FString(Info.Label).ToUpper()
						+ TEXT("   ·   ")
						+ Info.Hint))
					.TextStyle(&Text(TEXT("Backlot.Mono.9")))
					.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					Fields
				]
			]
		]
	];
	if (!bReadOnly
		&& (Block.Kind == EExtendedAtlassianBlockKind::OrderedItem
			|| Block.Kind == EExtendedAtlassianBlockKind::TableRow
			|| Block.Kind == EExtendedAtlassianBlockKind::TaskItem
			|| Block.Kind == EExtendedAtlassianBlockKind::Image))
	{
		Result->AddSlot()
		.AutoHeight()
		.Padding(98.0f, 5.0f, 0.0f, 0.0f)
		[
			SNew(SButton)
			.ButtonStyle(&Button(TEXT("Backlot.Button.Secondary")))
			.ContentPadding(FMargin(10.0f, 5.0f))
			.OnClicked_Lambda(
				[this, BlockIndex, Kind = Block.Kind]()
				{
					InsertBlock(Kind, BlockIndex);
					return FReply::Handled();
				})
			[
				SNew(STextBlock)
				.Text(FText::Format(
					LOCTEXT("AddDocumentItem", "+  Add {0}"),
					FText::FromString(
						Block.Kind == EExtendedAtlassianBlockKind::OrderedItem
							? FString(TEXT("rule"))
							: Block.Kind == EExtendedAtlassianBlockKind::TableRow
								? FString(TEXT("row"))
								: Block.Kind == EExtendedAtlassianBlockKind::TaskItem
									? FString(TEXT("to-do"))
									: FString(TEXT("asset")))))
				.TextStyle(&Text(TEXT("Backlot.Sans.11")))
			]
		];
	}
	if (InsertMenuBlockIndex == BlockIndex)
	{
		Result->AddSlot()
		.AutoHeight()
		.Padding(98.0f, 5.0f, 0.0f, 0.0f)
		[
			BuildInsertMenu(BlockIndex)
		];
	}
	return Result;
}

TSharedRef<SWidget> SExtendedAtlassianDocumentEditor::BuildBlockPalette()
{
	using namespace ExtendedAtlassianDocumentEditorPrivate;
	const EExtendedAtlassianBlockKind Kinds[] = {
		EExtendedAtlassianBlockKind::Heading,
		EExtendedAtlassianBlockKind::Paragraph,
		EExtendedAtlassianBlockKind::Quote,
		EExtendedAtlassianBlockKind::OrderedItem,
		EExtendedAtlassianBlockKind::TableRow,
		EExtendedAtlassianBlockKind::CodeBlock,
		EExtendedAtlassianBlockKind::TaskItem,
		EExtendedAtlassianBlockKind::Image
	};
	TSharedRef<SVerticalBox> Items = SNew(SVerticalBox);
	for (EExtendedAtlassianBlockKind Kind : Kinds)
	{
		const FBlockPresentation Info = Presentation(Kind);
		Items->AddSlot()
		.AutoHeight()
		[
			SNew(SButton)
			.Visibility_Lambda(
				[this, Kind]()
				{
					return SlashText.StartsWith(TEXT("/"))
						&& SlashMatches(Kind)
							? EVisibility::Visible
							: EVisibility::Collapsed;
				})
			.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
			.ContentPadding(FMargin(9.0f, 6.0f))
			.OnClicked_Lambda(
				[this, Kind]()
				{
					AddBlock(Kind);
					SlashText.Reset();
					if (SlashInput.IsValid())
					{
						SlashInput->SetText(FText::GetEmpty());
					}
					return FReply::Handled();
				})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(24.0f)
					.HeightOverride(24.0f)
					[
						SNew(SBorder)
						.BorderImage(Brush(TEXT("Backlot.Brush.CardSelected")))
						.Padding(0.0f)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(FText::FromString(Info.Glyph))
							.TextStyle(&Text(TEXT("Backlot.Mono.10.Medium")))
							.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(Info.Color))
						]
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.Padding(11.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Info.Label))
					.TextStyle(&Text(TEXT("Backlot.Sans.12")))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Info.Hint))
					.TextStyle(&Text(TEXT("Backlot.Mono.10")))
					.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#5c636d")))
				]
			]
		];
	}

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 2.0f, 10.0f, 0.0f)
			[
				SNew(SBox)
				.WidthOverride(20.0f)
				.HeightOverride(20.0f)
				[
					SNew(SBorder)
						.BorderImage(Brush(TEXT("Backlot.Brush.CardSelected")))
						.Padding(0.0f)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("SlashInsertPlus", "+"))
							.TextStyle(&Text(TEXT("Backlot.Mono.10")))
						]
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SAssignNew(SlashInput, SEditableTextBox)
				.Style(
					&FExtendedAtlassianStyle::Get()
						.GetWidgetStyle<FEditableTextBoxStyle>(
							TEXT("Backlot.Field")))
				.HintText(LOCTEXT(
					"SlashInsertHint",
					"Type \"/\" to insert a block…"))
				.Text(FText::FromString(SlashText))
				.IsReadOnly_Lambda([this]() { return bReadOnly; })
				.OnTextChanged(
					this,
					&SExtendedAtlassianDocumentEditor::OnSlashChanged)
				.OnKeyDownHandler(
					this,
					&SExtendedAtlassianDocumentEditor::OnSlashKeyDown)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(30.0f, 6.0f, 0.0f, 0.0f)
		[
			SAssignNew(SlashMenuBox, SBox)
			.WidthOverride(316.0f)
			.Visibility_Lambda(
				[this]()
				{
					return SlashText.StartsWith(TEXT("/"))
						? EVisibility::Visible
						: EVisibility::Collapsed;
				})
			[
				SNew(SBorder)
				.BorderImage(Brush(TEXT("Backlot.Brush.Card")))
				.Padding(FMargin(6.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(9.0f, 7.0f, 9.0f, 8.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SlashInsertTitle", "INSERT"))
						.TextStyle(&Text(TEXT("Backlot.Mono.9")))
						.ColorAndOpacity(
							FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						Items
					]
				]
			]
		];
}

TSharedRef<SWidget> SExtendedAtlassianDocumentEditor::BuildInsertMenu(
	int32 AfterBlockIndex)
{
	using namespace ExtendedAtlassianDocumentEditorPrivate;
	const EExtendedAtlassianBlockKind Kinds[] = {
		EExtendedAtlassianBlockKind::Heading,
		EExtendedAtlassianBlockKind::Paragraph,
		EExtendedAtlassianBlockKind::Quote,
		EExtendedAtlassianBlockKind::OrderedItem,
		EExtendedAtlassianBlockKind::CodeBlock,
		EExtendedAtlassianBlockKind::TableRow,
		EExtendedAtlassianBlockKind::TaskItem,
		EExtendedAtlassianBlockKind::Image
	};
	TSharedRef<SVerticalBox> Items = SNew(SVerticalBox);
	Items->AddSlot()
	.AutoHeight()
	.Padding(9.0f, 7.0f, 9.0f, 8.0f)
	[
		SNew(STextBlock)
		.Text(LOCTEXT("InsertAfterTitle", "INSERT AFTER"))
		.TextStyle(&Text(TEXT("Backlot.Mono.9")))
		.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(TEXT("#6f7783")))
	];
	for (EExtendedAtlassianBlockKind Kind : Kinds)
	{
		const FBlockPresentation Info = Presentation(Kind);
		Items->AddSlot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(&Button(TEXT("Backlot.Button.Clear")))
			.ContentPadding(FMargin(9.0f, 6.0f))
			.OnClicked_Lambda(
				[this, Kind, AfterBlockIndex]()
				{
					InsertMenuBlockIndex = INDEX_NONE;
					InsertBlock(Kind, AfterBlockIndex);
					return FReply::Handled();
				})
			[
				SNew(STextBlock)
				.Text(FText::FromString(
					FString(Info.Glyph)
					+ TEXT("   ")
					+ Info.Label
					+ TEXT("   ")
					+ Info.Hint))
				.TextStyle(&Text(TEXT("Backlot.Sans.11")))
				.ColorAndOpacity(FExtendedAtlassianStyle::FromHex(Info.Color))
			]
		];
	}
	return SNew(SBox)
		.WidthOverride(196.0f)
		[
			SNew(SBorder)
			.BorderImage(Brush(TEXT("Backlot.Brush.Card")))
			.Padding(FMargin(6.0f))
			[
				Items
			]
		];
}

void SExtendedAtlassianDocumentEditor::AddBlock(
	EExtendedAtlassianBlockKind Kind)
{
	InsertBlock(Kind, INDEX_NONE);
}

void SExtendedAtlassianDocumentEditor::InsertBlock(
	EExtendedAtlassianBlockKind Kind,
	int32 AfterBlockIndex)
{
	if (bReadOnly)
	{
		return;
	}
	FExtendedAtlassianDocBlock Block;
	Block.Kind = Kind;
	Block.Level = Kind == EExtendedAtlassianBlockKind::Heading ? 2 : 0;
	Block.OrderedIndex =
		Kind == EExtendedAtlassianBlockKind::OrderedItem ? 1 : 0;
	if (Kind == EExtendedAtlassianBlockKind::Heading)
	{
		Block.Markup = TEXT("New heading");
	}
	else if (Kind == EExtendedAtlassianBlockKind::CodeBlock)
	{
		Block.CodeLanguage = TEXT("C++");
	}
	else if (Kind == EExtendedAtlassianBlockKind::Image)
	{
		Block.ImageAlt = TEXT("New asset");
		Block.ImageMeta = TEXT("DROP FROM THE CONTENT BROWSER");
		Block.EmbedSlot = TEXT("ASSET");
	}
	if (Kind == EExtendedAtlassianBlockKind::TableRow)
	{
		Block.bIsHeaderRow = !Blocks.ContainsByPredicate(
			[](const FExtendedAtlassianDocBlock& Candidate)
			{
				return Candidate.Kind
					== EExtendedAtlassianBlockKind::TableRow;
			});
		Block.Cells = Block.bIsHeaderRow
			? TArray<FString>{ TEXT("PARAMETER"), TEXT("RANGE"), TEXT("DEFAULT"), TEXT("OWNER") }
			: TArray<FString>{ FString(), FString(), FString(), FString() };
	}
	if (AfterBlockIndex == INDEX_NONE || !Blocks.IsValidIndex(AfterBlockIndex))
	{
		Blocks.Add(MoveTemp(Block));
	}
	else
	{
		Blocks.Insert(MoveTemp(Block), AfterBlockIndex + 1);
	}
	NormalizeBlocks();
	CommitBlocks();
	RebuildBlocks();
}

void SExtendedAtlassianDocumentEditor::MoveBlock(
	int32 BlockIndex,
	int32 Direction)
{
	const int32 TargetIndex = BlockIndex + Direction;
	if (bReadOnly
		|| !Blocks.IsValidIndex(BlockIndex)
		|| !Blocks.IsValidIndex(TargetIndex))
	{
		return;
	}
	Blocks.Swap(BlockIndex, TargetIndex);
	InsertMenuBlockIndex = INDEX_NONE;
	NormalizeBlocks();
	CommitBlocks();
	RebuildBlocks();
}

void SExtendedAtlassianDocumentEditor::ToggleInsertMenu(int32 BlockIndex)
{
	if (bReadOnly)
	{
		return;
	}
	InsertMenuBlockIndex =
		InsertMenuBlockIndex == BlockIndex ? INDEX_NONE : BlockIndex;
	RebuildBlocks();
}

void SExtendedAtlassianDocumentEditor::RemoveBlock(int32 BlockIndex)
{
	if (!bReadOnly && Blocks.IsValidIndex(BlockIndex))
	{
		Blocks.RemoveAt(BlockIndex);
		InsertMenuBlockIndex = INDEX_NONE;
		NormalizeBlocks();
		CommitBlocks();
		RebuildBlocks();
	}
}

void SExtendedAtlassianDocumentEditor::NormalizeBlocks()
{
	int32 OrderedIndex = 0;
	bool bInOrderedRun = false;
	bool bInTableRun = false;
	for (FExtendedAtlassianDocBlock& Block : Blocks)
	{
		if (Block.Kind == EExtendedAtlassianBlockKind::OrderedItem)
		{
			OrderedIndex = bInOrderedRun ? OrderedIndex + 1 : 1;
			bInOrderedRun = true;
			Block.OrderedIndex = OrderedIndex;
		}
		else
		{
			bInOrderedRun = false;
			OrderedIndex = 0;
		}
		if (Block.Kind == EExtendedAtlassianBlockKind::TableRow)
		{
			Block.bIsHeaderRow = !bInTableRun;
			bInTableRun = true;
			while (Block.Cells.Num() < 4)
			{
				Block.Cells.Add(FString());
			}
		}
		else
		{
			bInTableRun = false;
		}
	}
}

void SExtendedAtlassianDocumentEditor::CommitBlocks()
{
	Markdown = FExtendedAtlassianMarkdown::FromBlocks(Blocks);
	bDirty = true;
	bChangeNotificationPending = true;
}

void SExtendedAtlassianDocumentEditor::SetMarkupText(
	const FText& Value,
	int32 BlockIndex)
{
	if (!bReadOnly && Blocks.IsValidIndex(BlockIndex))
	{
		Blocks[BlockIndex].Markup =
			FExtendedAtlassianMarkup::Escape(Value.ToString());
		CommitBlocks();
	}
}

void SExtendedAtlassianDocumentEditor::SetRawText(
	const FText& Value,
	int32 BlockIndex)
{
	if (!bReadOnly && Blocks.IsValidIndex(BlockIndex))
	{
		Blocks[BlockIndex].RawText = Value.ToString();
		CommitBlocks();
	}
}

void SExtendedAtlassianDocumentEditor::SetCodeLanguage(
	const FText& Value,
	int32 BlockIndex)
{
	if (!bReadOnly && Blocks.IsValidIndex(BlockIndex))
	{
		Blocks[BlockIndex].CodeLanguage = Value.ToString();
		CommitBlocks();
	}
}

void SExtendedAtlassianDocumentEditor::SetCodeFile(
	const FText& Value,
	int32 BlockIndex)
{
	if (!bReadOnly && Blocks.IsValidIndex(BlockIndex))
	{
		Blocks[BlockIndex].ImageAlt = Value.ToString();
		CommitBlocks();
	}
}

void SExtendedAtlassianDocumentEditor::SetTableCell(
	const FText& Value,
	int32 BlockIndex,
	int32 CellIndex)
{
	if (!bReadOnly
		&& Blocks.IsValidIndex(BlockIndex)
		&& Blocks[BlockIndex].Cells.IsValidIndex(CellIndex))
	{
		Blocks[BlockIndex].Cells[CellIndex] =
			FExtendedAtlassianMarkup::Escape(Value.ToString());
		CommitBlocks();
	}
}

void SExtendedAtlassianDocumentEditor::ToggleTask(int32 BlockIndex)
{
	if (!bReadOnly && Blocks.IsValidIndex(BlockIndex))
	{
		Blocks[BlockIndex].bChecked = !Blocks[BlockIndex].bChecked;
		CommitBlocks();
		RebuildBlocks();
	}
}

void SExtendedAtlassianDocumentEditor::OnSlashChanged(const FText& Value)
{
	const bool bWasOpen = SlashText.StartsWith(TEXT("/"));
	SlashText = Value.ToString();
	if (!bWasOpen && SlashText.StartsWith(TEXT("/")))
	{
		SlashMenuSequence.JumpToStart();
		SlashMenuSequence.Play(AsShared());
	}
}

FReply SExtendedAtlassianDocumentEditor::OnSlashKeyDown(
	const FGeometry&,
	const FKeyEvent& KeyEvent)
{
	if (KeyEvent.GetKey() == EKeys::Escape && SlashText.StartsWith(TEXT("/")))
	{
		SlashText.Reset();
		if (SlashInput.IsValid())
		{
			SlashInput->SetText(FText::GetEmpty());
		}
		return FReply::Handled();
	}
	if (KeyEvent.GetKey() == EKeys::Enter && SlashText.StartsWith(TEXT("/")))
	{
		const EExtendedAtlassianBlockKind Kinds[] = {
			EExtendedAtlassianBlockKind::Heading,
			EExtendedAtlassianBlockKind::Paragraph,
			EExtendedAtlassianBlockKind::Quote,
			EExtendedAtlassianBlockKind::OrderedItem,
			EExtendedAtlassianBlockKind::CodeBlock,
			EExtendedAtlassianBlockKind::TableRow,
			EExtendedAtlassianBlockKind::TaskItem,
			EExtendedAtlassianBlockKind::Image
		};
		for (EExtendedAtlassianBlockKind Kind : Kinds)
		{
			if (SlashMatches(Kind))
			{
				AddBlock(Kind);
				SlashText.Reset();
				if (SlashInput.IsValid())
				{
					SlashInput->SetText(FText::GetEmpty());
				}
				return FReply::Handled();
			}
		}
	}
	return FReply::Unhandled();
}

bool SExtendedAtlassianDocumentEditor::SlashMatches(
	EExtendedAtlassianBlockKind Kind) const
{
	using namespace ExtendedAtlassianDocumentEditorPrivate;
	FString Query = SlashText;
	Query.RemoveFromStart(TEXT("/"));
	Query.TrimStartAndEndInline();
	if (Query.IsEmpty())
	{
		return true;
	}
	const FBlockPresentation Info = Presentation(Kind);
	const FString Label = Info.Label;
	FString Key;
	switch (Kind)
	{
	case EExtendedAtlassianBlockKind::Heading: Key = TEXT("h2"); break;
	case EExtendedAtlassianBlockKind::Paragraph: Key = TEXT("para"); break;
	case EExtendedAtlassianBlockKind::Quote: Key = TEXT("callout"); break;
	case EExtendedAtlassianBlockKind::OrderedItem: Key = TEXT("rules"); break;
	case EExtendedAtlassianBlockKind::CodeBlock: Key = TEXT("code"); break;
	case EExtendedAtlassianBlockKind::TableRow: Key = TEXT("table"); break;
	case EExtendedAtlassianBlockKind::TaskItem: Key = TEXT("todo"); break;
	case EExtendedAtlassianBlockKind::Image: Key = TEXT("embeds"); break;
	default: break;
	}
	return Label.StartsWith(Query, ESearchCase::IgnoreCase)
		|| Key.StartsWith(Query, ESearchCase::IgnoreCase);
}

EActiveTimerReturnType SExtendedAtlassianDocumentEditor::HandlePreviewTimer(
	double,
	float)
{
	if (bChangeNotificationPending)
	{
		bChangeNotificationPending = false;
		OnMarkdownChanged.ExecuteIfBound(Markdown);
	}
	return EActiveTimerReturnType::Continue;
}

#undef LOCTEXT_NAMESPACE
