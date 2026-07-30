// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ExtendedAtlassianDocBlock.h"
#include "Widgets/SCompoundWidget.h"

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
 * Variable-height rich-text rows are materialized in bounded chunks. This preserves text
 * selection and correct wrapping while keeping very large Confluence pages responsive.
 */
class SExtendedAtlassianDocumentView : public SCompoundWidget
{
public:
	DECLARE_DELEGATE_OneParam(FOnTaskToggled, int32 /*BlockIndex*/);
	DECLARE_DELEGATE_OneParam(FOnIssueClicked, const FString& /*IssueKey*/);
	DECLARE_DELEGATE_TwoParams(
		FOnAssetClicked,
		const FString& /*Name*/,
		const FString& /*PathOrMeta*/);

	SLATE_BEGIN_ARGS(SExtendedAtlassianDocumentView)
		: _MaxReadingWidth(920.0f)
	{}
		/** Content is capped to this width for legibility. */
		SLATE_ARGUMENT(float, MaxReadingWidth)
		SLATE_EVENT(FOnTaskToggled, OnTaskToggled)
		SLATE_EVENT(FOnIssueClicked, OnIssueClicked)
		SLATE_EVENT(FOnAssetClicked, OnAssetClicked)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Replaces the displayed document. */
	void SetBlocks(const TArray<FExtendedAtlassianDocBlock>& InBlocks);
	void SetIssueStatuses(const TMap<FString, FString>& InStatuses)
	{
		IssueStatuses = InStatuses;
	}
	int32 GetMaterializedBlockCountForAutomation() const
	{
		return VisibleBlockLimit;
	}
	int32 GetTotalBlockCountForAutomation() const { return Blocks.Num(); }

	void Clear();

private:
	TSharedRef<SWidget> BuildBlockWidget(
		const FExtendedAtlassianDocBlock& Block,
		int32 BlockIndex) const;
	TSharedRef<SWidget> BuildTableRow(const FExtendedAtlassianDocBlock& Block) const;
	TSharedRef<SWidget> BuildOrderedRules(
		const TArray<FExtendedAtlassianDocBlock>& InBlocks,
		int32 FirstIndex,
		int32 LastIndex) const;
	TSharedRef<SWidget> BuildTable(
		const TArray<FExtendedAtlassianDocBlock>& InBlocks,
		int32 FirstIndex,
		int32 LastIndex) const;
	TSharedRef<SWidget> BuildTasks(
		const TArray<FExtendedAtlassianDocBlock>& InBlocks,
		int32 FirstIndex,
		int32 LastIndex) const;
	TSharedRef<SWidget> BuildAssets(
		const TArray<FExtendedAtlassianDocBlock>& InBlocks,
		int32 FirstIndex,
		int32 LastIndex) const;
	void RebuildVisibleBlocks();
	FReply LoadMoreBlocks();
	EActiveTimerReturnType HandleCopyTimer(double CurrentTime, float DeltaTime);

	/** Spacing above a block, so sections separate without manual blank lines. */
	static float GetTopPadding(const FExtendedAtlassianDocBlock& Block, bool bIsFirst);

	TSharedPtr<SVerticalBox> ContentBox;
	TArray<FExtendedAtlassianDocBlock> Blocks;
	int32 VisibleBlockLimit = 0;
	float MaxReadingWidth = 920.0f;
	TMap<FString, FString> IssueStatuses;
	FOnTaskToggled OnTaskToggled;
	FOnIssueClicked OnIssueClicked;
	FOnAssetClicked OnAssetClicked;
	mutable bool bCodeCopied = false;
};
