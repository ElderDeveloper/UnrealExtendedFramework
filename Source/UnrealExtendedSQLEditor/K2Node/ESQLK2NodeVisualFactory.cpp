// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "K2Node/ESQLK2NodeVisualFactory.h"

#include "EdGraph/EdGraphNode.h"
#include "K2Node/ESQLAsyncTypedK2Nodes.h"
#include "K2Node/ESQLQueryBuilderK2Nodes.h"
#include "K2Node/ESQLRowQueryK2Nodes.h"
#include "K2Node/SESQLQueryBuilderGraphNode.h"

namespace
{
	bool SupportsCustomQueryBuilderVisual(const UEdGraphNode* Node)
	{
		return Node
			&& (Node->IsA<UK2Node_ESQLMakeQuerySpec>()
				|| Node->IsA<UK2Node_ESQLQueryRows>()
				|| Node->IsA<UK2Node_ESQLCountRowsQuery>()
				|| Node->IsA<UK2Node_ESQLFindRows>()
				|| Node->IsA<UK2Node_ESQLAsyncFindRows>());
	}
}

TSharedPtr<SGraphNode> FESQLK2NodeVisualFactory::CreateNode(UEdGraphNode* Node) const
{
	if (!SupportsCustomQueryBuilderVisual(Node))
	{
		return nullptr;
	}

	return SNew(SESQLQueryBuilderGraphNode, Node);
}