// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "EdGraphUtilities.h"

class FESQLK2NodeVisualFactory : public FGraphPanelNodeFactory
{
public:
	virtual TSharedPtr<SGraphNode> CreateNode(UEdGraphNode* Node) const override;
};