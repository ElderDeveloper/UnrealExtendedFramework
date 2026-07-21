// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestNode_Details.h"

#include "DetailLayoutBuilder.h"

// The graph editor customizes the owning UEGQuestGraphNode instead (FEGQuestGraphNode_Details);
// a quest node rooting a details view on its own has nothing to customize.
void FEGQuestNode_Details::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
}
