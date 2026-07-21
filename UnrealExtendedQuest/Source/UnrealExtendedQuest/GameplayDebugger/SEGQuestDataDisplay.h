// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "Widgets/SCompoundWidget.h"

class SVerticalBox;
DECLARE_LOG_CATEGORY_EXTERN(LogEGQuestPluginDataDisplay, Verbose, All);

/** Runtime inspector for replicated quest snapshots. */
class UNREALEXTENDEDQUEST_API SEGQuestDataDisplay : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEGQuestDataDisplay) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TWeakObjectPtr<const UObject>& InWorldContextObjectPtr);
	void SetWorldContextObject(const TWeakObjectPtr<const UObject>& InWorldContextObjectPtr) { WorldContextObjectPtr = InWorldContextObjectPtr; }
	void RefreshTree(bool bPreserveExpansion);

private:
	FReply HandleRefresh();
	void AddComponentSnapshots(class UEGQuestComponent* Component, const FText& ScopeLabel);
	void AddSnapshot(const struct FEGQuestViewSnapshot& Snapshot, const FText& ScopeLabel);

	TWeakObjectPtr<const UObject> WorldContextObjectPtr;
	TSharedPtr<SVerticalBox> SnapshotList;
};
