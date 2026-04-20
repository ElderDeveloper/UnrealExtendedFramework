// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "K2Node/SESQLQueryBuilderGraphNode.h"

#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Input/Reply.h"
#include "K2Node/ESQLAsyncTypedK2Nodes.h"
#include "K2Node/ESQLQueryBuilderK2Nodes.h"
#include "K2Node/ESQLRowQueryK2Nodes.h"
#include "SGraphPin.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
	const FLinearColor NodeOuterColor(0.11f, 0.14f, 0.18f, 1.0f);
	const FLinearColor NodeInnerColor(0.045f, 0.055f, 0.07f, 1.0f);
	const FLinearColor TableSectionColor(0.09f, 0.13f, 0.19f, 0.95f);
	const FLinearColor FilterSectionColor(0.07f, 0.15f, 0.11f, 0.95f);
	const FLinearColor SortSectionColor(0.16f, 0.11f, 0.06f, 0.95f);
	const FLinearColor WindowSectionColor(0.10f, 0.10f, 0.15f, 0.95f);
	const FLinearColor CardColor(0.08f, 0.09f, 0.12f, 0.92f);
	constexpr float NodeSectionSpacing = 6.0f;
	constexpr float NodeOuterPadding = 8.0f;
	constexpr float SectionPadding = 7.0f;
	constexpr float CardPadding = 6.0f;

	template <typename TNode>
	FESQLQueryClauseUiBase* CastQueryClauseUi(UEdGraphNode* Node)
	{
		if (TNode* TypedNode = Cast<TNode>(Node))
		{
			return static_cast<FESQLQueryClauseUiBase*>(TypedNode);
		}

		return nullptr;
	}

	FESQLQueryClauseUiBase* GetQueryClauseUi(UEdGraphNode* Node)
	{
		if (!Node)
		{
			return nullptr;
		}

		if (FESQLQueryClauseUiBase* QueryUi = CastQueryClauseUi<UK2Node_ESQLMakeQuerySpec>(Node))
		{
			return QueryUi;
		}
		if (FESQLQueryClauseUiBase* QueryUi = CastQueryClauseUi<UK2Node_ESQLQueryRows>(Node))
		{
			return QueryUi;
		}
		if (FESQLQueryClauseUiBase* QueryUi = CastQueryClauseUi<UK2Node_ESQLCountRowsQuery>(Node))
		{
			return QueryUi;
		}
		if (FESQLQueryClauseUiBase* QueryUi = CastQueryClauseUi<UK2Node_ESQLFindRows>(Node))
		{
			return QueryUi;
		}
		if (FESQLQueryClauseUiBase* QueryUi = CastQueryClauseUi<UK2Node_ESQLAsyncFindRows>(Node))
		{
			return QueryUi;
		}

		return nullptr;
	}
}

void SESQLQueryBuilderGraphNode::Construct(const FArguments& InArgs, UEdGraphNode* InNode)
{
	GraphNode = InNode;
	SetCursor(EMouseCursor::CardinalCross);
	UpdateGraphNode();
}

void SESQLQueryBuilderGraphNode::UpdateGraphNode()
{
	InputPins.Empty();
	OutputPins.Empty();
	LeftNodeBox.Reset();
	RightNodeBox.Reset();

	ContentScale.Bind(this, &SGraphNode::GetContentScale);

	GetOrAddSlot(ENodeZone::Center)
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
		.BorderBackgroundColor(NodeOuterColor)
		.Padding(1.0f)
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
			.BorderBackgroundColor(NodeInnerColor)
			.Padding(FMargin(NodeOuterPadding))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Fill)
				.Padding(FMargin(0.0f, 4.0f, 6.0f, 0.0f))
				[
					SAssignNew(LeftNodeBox, SVerticalBox)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					CreateNodeBody()
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Fill)
				.Padding(FMargin(6.0f, 4.0f, 0.0f, 0.0f))
				[
					SAssignNew(RightNodeBox, SVerticalBox)
				]
			]
		]
	];

	PopulateStandardPins();
}

TSharedRef<SWidget> SESQLQueryBuilderGraphNode::CreateNodeBody()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			CreateHeaderWidget()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0.0f, NodeSectionSpacing, 0.0f, 0.0f))
		[
			CreateTableAssetSection()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0.0f, NodeSectionSpacing, 0.0f, 0.0f))
		[
			CreateFiltersSection()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0.0f, NodeSectionSpacing, 0.0f, 0.0f))
		[
			CreateSortSection()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(0.0f, NodeSectionSpacing, 0.0f, 0.0f))
		[
			CreateWindowSection()
		];
}

TSharedRef<SWidget> SESQLQueryBuilderGraphNode::CreateHeaderWidget() const
{
	const FLinearColor TitleColor = GraphNode ? GraphNode->GetNodeTitleColor() : FLinearColor(0.16f, 0.53f, 0.82f, 1.0f);

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
		.BorderBackgroundColor(TitleColor.CopyWithNewOpacity(0.25f))
		.Padding(FMargin(10.0f, 7.0f))
		[
			SNew(STextBlock)
			.Text(GraphNode ? GraphNode->GetNodeTitle(ENodeTitleType::FullTitle) : FText::GetEmpty())
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		];
}

TSharedRef<SWidget> SESQLQueryBuilderGraphNode::CreateTableAssetSection()
{
	UEdGraphPin* TableAssetPin = GraphNode ? GraphNode->FindPin(FESQLQueryClauseUiBase::GetTableAssetPinName()) : nullptr;

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
		.BorderBackgroundColor(TableSectionColor)
		.Padding(FMargin(SectionPadding))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("ESQLQueryBuilderGraphNode", "TableSectionTitle", "Table"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
			[
				CreateInlinePinWidget(TableAssetPin)
			]
		];
}

TSharedRef<SWidget> SESQLQueryBuilderGraphNode::CreateFiltersSection()
{
	const TArray<FFilterPinSet> FilterPins = CollectFilterPins();
	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
					SNew(STextBlock)
					.Text(NSLOCTEXT("ESQLQueryBuilderGraphNode", "FiltersTitle", "Filters"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
				.Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(SButton)
				.Text(NSLOCTEXT("ESQLQueryBuilderGraphNode", "AddFilterButton", "+ Filter"))
				.IsEnabled(this, &SESQLQueryBuilderGraphNode::CanAddFilterClause)
				.OnClicked(this, &SESQLQueryBuilderGraphNode::HandleAddFilterClause)
			]
		];

	if (FilterPins.Num() == 0)
	{
		Content->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.0f, 6.0f, 0.0f, 0.0f))
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("ESQLQueryBuilderGraphNode", "EmptyFiltersText", "No filters added."))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
		];
	}
	else
	{
		for (const FFilterPinSet& FilterPin : FilterPins)
		{
			Content->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 6.0f, 0.0f, 0.0f))
			[
				CreateFilterClauseCard(FilterPin)
			];
		}
	}

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FilterSectionColor)
		.Padding(FMargin(SectionPadding))
		[
			Content
		];
}

TSharedRef<SWidget> SESQLQueryBuilderGraphNode::CreateSortSection()
{
	const TArray<FSortPinSet> SortPins = CollectSortPins();
	TSharedRef<SVerticalBox> Content = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
					SNew(STextBlock)
					.Text(NSLOCTEXT("ESQLQueryBuilderGraphNode", "SortTitle", "Sort"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
				.Padding(FMargin(6.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(SButton)
				.Text(NSLOCTEXT("ESQLQueryBuilderGraphNode", "AddSortButton", "+ Sort"))
				.IsEnabled(this, &SESQLQueryBuilderGraphNode::CanAddSortClause)
				.OnClicked(this, &SESQLQueryBuilderGraphNode::HandleAddSortClause)
			]
		];

	if (SortPins.Num() == 0)
	{
		Content->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.0f, 6.0f, 0.0f, 0.0f))
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("ESQLQueryBuilderGraphNode", "EmptySortsText", "No sort rules added."))
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
		];
	}
	else
	{
		for (const FSortPinSet& SortPin : SortPins)
		{
			Content->AddSlot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 6.0f, 0.0f, 0.0f))
			[
				CreateSortClauseCard(SortPin)
			];
		}
	}

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
		.BorderBackgroundColor(SortSectionColor)
		.Padding(FMargin(SectionPadding))
		[
			Content
		];
}

TSharedRef<SWidget> SESQLQueryBuilderGraphNode::CreateWindowSection()
{
	UEdGraphPin* LimitPin = GraphNode ? GraphNode->FindPin(FESQLQueryClauseUiBase::GetLimitPinName()) : nullptr;
	UEdGraphPin* OffsetPin = GraphNode ? GraphNode->FindPin(FESQLQueryClauseUiBase::GetOffsetPinName()) : nullptr;

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
		.BorderBackgroundColor(WindowSectionColor)
		.Padding(FMargin(SectionPadding))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("ESQLQueryBuilderGraphNode", "WindowSectionTitle", "Range"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
				[
					CreateInlinePinWidget(LimitPin)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
				[
					CreateInlinePinWidget(OffsetPin)
				]
			]
		];
}

TSharedRef<SWidget> SESQLQueryBuilderGraphNode::CreateFilterClauseCard(const FFilterPinSet& PinSet)
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
		.BorderBackgroundColor(CardColor)
		.Padding(FMargin(CardPadding))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::Format(NSLOCTEXT("ESQLQueryBuilderGraphNode", "FilterClauseTitle", "Filter {0}"), FText::AsNumber(PinSet.Index + 1)))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(NSLOCTEXT("ESQLQueryBuilderGraphNode", "RemoveFilterButton", "Remove"))
					.OnClicked(this, &SESQLQueryBuilderGraphNode::HandleRemoveClause, PinSet.FieldPin)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.2f)
				.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
				[
					CreateInlinePinWidget(PinSet.FieldPin)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(FMargin(4.0f, 0.0f, 4.0f, 0.0f))
				[
					CreateInlinePinWidget(PinSet.OperationPin)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.2f)
				.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
				[
					SNew(SBox)
					.Visibility(this, &SESQLQueryBuilderGraphNode::GetFilterValueVisibility, PinSet.OperationPin, PinSet.ValuePin)
					[
						CreateInlinePinWidget(PinSet.ValuePin)
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
			[
				SNew(SBox)
				.Visibility(this, &SESQLQueryBuilderGraphNode::GetFilterValuesVisibility, PinSet.OperationPin, PinSet.ValuesPin)
				[
					CreateInlinePinWidget(PinSet.ValuesPin)
				]
			]
		];
}

TSharedRef<SWidget> SESQLQueryBuilderGraphNode::CreateSortClauseCard(const FSortPinSet& PinSet)
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("WhiteBrush"))
		.BorderBackgroundColor(CardColor)
		.Padding(FMargin(CardPadding))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::Format(NSLOCTEXT("ESQLQueryBuilderGraphNode", "SortClauseTitle", "Sort {0}"), FText::AsNumber(PinSet.Index + 1)))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(NSLOCTEXT("ESQLQueryBuilderGraphNode", "RemoveSortButton", "Remove"))
					.OnClicked(this, &SESQLQueryBuilderGraphNode::HandleRemoveClause, PinSet.FieldPin)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 4.0f, 0.0f, 0.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.25f)
				.Padding(FMargin(0.0f, 0.0f, 4.0f, 0.0f))
				[
					CreateInlinePinWidget(PinSet.FieldPin)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.9f)
				.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
				[
					CreateInlinePinWidget(PinSet.AscendingPin)
				]
			]
		];
}

TSharedRef<SWidget> SESQLQueryBuilderGraphNode::CreateInlinePinWidget(UEdGraphPin* Pin)
{
	if (!Pin)
	{
		return SNullWidget::NullWidget;
	}

	TSharedPtr<SGraphPin> PinWidget = SGraphNode::CreatePinWidget(Pin);
	if (!PinWidget.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	PinWidget->SetOwner(SharedThis(this));
	return PinWidget.ToSharedRef();
}

void SESQLQueryBuilderGraphNode::PopulateStandardPins()
{
	if (!GraphNode)
	{
		return;
	}

	for (UEdGraphPin* Pin : GraphNode->Pins)
	{
		if (!Pin || Pin->bHidden || ShouldInlinePin(Pin))
		{
			continue;
		}

		TSharedPtr<SGraphPin> NewPin = SGraphNode::CreatePinWidget(Pin);
		if (NewPin.IsValid())
		{
			AddPin(NewPin.ToSharedRef());
		}
	}
}

TArray<SESQLQueryBuilderGraphNode::FFilterPinSet> SESQLQueryBuilderGraphNode::CollectFilterPins() const
{
	TMap<int32, FFilterPinSet> PinSets;
	if (!GraphNode)
	{
		return {};
	}

	for (UEdGraphPin* Pin : GraphNode->Pins)
	{
		if (!Pin || Pin->Direction != EGPD_Input || Pin->bHidden)
		{
			continue;
		}

		int32 ClauseIndex = INDEX_NONE;
		switch (FESQLQueryClauseUiBase::DescribeClausePinName(Pin->PinName, ClauseIndex))
		{
		case FESQLQueryClauseUiBase::EClausePinKind::FilterField:
			PinSets.FindOrAdd(ClauseIndex).FieldPin = Pin;
			PinSets.FindOrAdd(ClauseIndex).Index = ClauseIndex;
			break;
		case FESQLQueryClauseUiBase::EClausePinKind::FilterOperation:
			PinSets.FindOrAdd(ClauseIndex).OperationPin = Pin;
			PinSets.FindOrAdd(ClauseIndex).Index = ClauseIndex;
			break;
		case FESQLQueryClauseUiBase::EClausePinKind::FilterValue:
			PinSets.FindOrAdd(ClauseIndex).ValuePin = Pin;
			PinSets.FindOrAdd(ClauseIndex).Index = ClauseIndex;
			break;
		case FESQLQueryClauseUiBase::EClausePinKind::FilterValues:
			PinSets.FindOrAdd(ClauseIndex).ValuesPin = Pin;
			PinSets.FindOrAdd(ClauseIndex).Index = ClauseIndex;
			break;
		default:
			break;
		}
	}

	TArray<int32> SortedKeys;
	PinSets.GetKeys(SortedKeys);
	SortedKeys.Sort();

	TArray<FFilterPinSet> Result;
	for (int32 Key : SortedKeys)
	{
		Result.Add(PinSets[Key]);
	}

	return Result;
}

TArray<SESQLQueryBuilderGraphNode::FSortPinSet> SESQLQueryBuilderGraphNode::CollectSortPins() const
{
	TMap<int32, FSortPinSet> PinSets;
	if (!GraphNode)
	{
		return {};
	}

	for (UEdGraphPin* Pin : GraphNode->Pins)
	{
		if (!Pin || Pin->Direction != EGPD_Input || Pin->bHidden)
		{
			continue;
		}

		int32 ClauseIndex = INDEX_NONE;
		switch (FESQLQueryClauseUiBase::DescribeClausePinName(Pin->PinName, ClauseIndex))
		{
		case FESQLQueryClauseUiBase::EClausePinKind::SortField:
			PinSets.FindOrAdd(ClauseIndex).FieldPin = Pin;
			PinSets.FindOrAdd(ClauseIndex).Index = ClauseIndex;
			break;
		case FESQLQueryClauseUiBase::EClausePinKind::SortAscending:
			PinSets.FindOrAdd(ClauseIndex).AscendingPin = Pin;
			PinSets.FindOrAdd(ClauseIndex).Index = ClauseIndex;
			break;
		default:
			break;
		}
	}

	TArray<int32> SortedKeys;
	PinSets.GetKeys(SortedKeys);
	SortedKeys.Sort();

	TArray<FSortPinSet> Result;
	for (int32 Key : SortedKeys)
	{
		Result.Add(PinSets[Key]);
	}

	return Result;
}

FText SESQLQueryBuilderGraphNode::GetTableSubtitleText() const
{
	const UEdGraphPin* TableAssetPin = GraphNode ? GraphNode->FindPin(FESQLQueryClauseUiBase::GetTableAssetPinName()) : nullptr;
	if (!TableAssetPin)
	{
		return NSLOCTEXT("ESQLQueryBuilderGraphNode", "NoTablePinSubtitle", "Configure reusable filters, sort rules, and result windowing.");
	}

	if (TableAssetPin->DefaultObject)
	{
		return FText::Format(
			NSLOCTEXT("ESQLQueryBuilderGraphNode", "LiteralTableSubtitle", "Using table asset: {0}"),
			FText::FromString(TableAssetPin->DefaultObject->GetName()));
	}

	if (TableAssetPin->LinkedTo.Num() > 0)
	{
		return NSLOCTEXT("ESQLQueryBuilderGraphNode", "DynamicTableSubtitle", "Table Asset is dynamic. Field dropdowns and typed value pins depend on upstream connections.");
	}

	return NSLOCTEXT("ESQLQueryBuilderGraphNode", "UnsetTableSubtitle", "Set a Table Asset for schema-aware field dropdowns and typed values.");
}

EVisibility SESQLQueryBuilderGraphNode::GetFilterValueVisibility(UEdGraphPin* OperationPin, UEdGraphPin* ValuePin) const
{
	if (!OperationPin)
	{
		return EVisibility::Visible;
	}

	const bool bValuePinHasData = ValuePin && (ValuePin->LinkedTo.Num() > 0 || (!ValuePin->DefaultValue.IsEmpty() && ValuePin->DefaultValue != ValuePin->AutogeneratedDefaultValue));
	if (OperationPin->LinkedTo.Num() > 0)
	{
		return EVisibility::Visible;
	}

	if (FESQLQueryClauseUiBase::UsesMultiValuePins(OperationPin) && !bValuePinHasData)
	{
		return EVisibility::Collapsed;
	}

	return EVisibility::Visible;
}

EVisibility SESQLQueryBuilderGraphNode::GetFilterValuesVisibility(UEdGraphPin* OperationPin, UEdGraphPin* ValuesPin) const
{
	if (!OperationPin)
	{
		return EVisibility::Collapsed;
	}

	const bool bValuesPinHasData = ValuesPin && (ValuesPin->LinkedTo.Num() > 0 || (!ValuesPin->DefaultValue.IsEmpty() && ValuesPin->DefaultValue != ValuesPin->AutogeneratedDefaultValue));
	if (OperationPin->LinkedTo.Num() > 0)
	{
		return EVisibility::Visible;
	}

	if (FESQLQueryClauseUiBase::UsesMultiValuePins(OperationPin) || bValuesPinHasData)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Collapsed;
}

bool SESQLQueryBuilderGraphNode::ShouldInlinePin(const UEdGraphPin* Pin) const
{
	if (!Pin || Pin->Direction != EGPD_Input)
	{
		return false;
	}

	if (Pin->PinName == FESQLQueryClauseUiBase::GetTableAssetPinName()
		|| Pin->PinName == FESQLQueryClauseUiBase::GetLimitPinName()
		|| Pin->PinName == FESQLQueryClauseUiBase::GetOffsetPinName())
	{
		return true;
	}

	return FESQLQueryClauseUiBase::IsQueryClausePinName(Pin->PinName);
}

bool SESQLQueryBuilderGraphNode::CanAddFilterClause() const
{
	return CollectFilterPins().Num() < FESQLQueryClauseUiBase::MaxClauseCount;
}

bool SESQLQueryBuilderGraphNode::CanAddSortClause() const
{
	return CollectSortPins().Num() < FESQLQueryClauseUiBase::MaxClauseCount;
}

FReply SESQLQueryBuilderGraphNode::HandleAddFilterClause() const
{
	if (FESQLQueryClauseUiBase* QueryUi = GetQueryClauseUi(GraphNode))
	{
		QueryUi->AddFilterClause();
	}

	return FReply::Handled();
}

FReply SESQLQueryBuilderGraphNode::HandleAddSortClause() const
{
	if (FESQLQueryClauseUiBase* QueryUi = GetQueryClauseUi(GraphNode))
	{
		QueryUi->AddSortClause();
	}

	return FReply::Handled();
}

FReply SESQLQueryBuilderGraphNode::HandleRemoveClause(UEdGraphPin* Pin) const
{
	if (Pin)
	{
		if (FESQLQueryClauseUiBase* QueryUi = GetQueryClauseUi(GraphNode))
		{
			QueryUi->RemoveClauseForPin(Pin);
		}
	}

	return FReply::Handled();
}