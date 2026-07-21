// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "Widgets/SCompoundWidget.h"

class SVerticalBox;
class UEGQuestGraph;

/** Overview of authored quest assets. */
class SEGQuestBrowser : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEGQuestBrowser) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void RefreshTree(bool bPreserveExpansion = false);

private:
	void RebuildQuestList();
	FReply OpenQuest(UEGQuestGraph* Quest);
	FReply HandleRefresh();

	TSharedPtr<SVerticalBox> QuestList;
};
