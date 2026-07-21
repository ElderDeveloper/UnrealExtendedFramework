// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR

#include "UILab/Scripting/EFUIInteractionScript.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class FEEUIScriptRunner;
class ITableRow;
class SEEUILabHostPanel;
class STableViewBase;
template <typename ItemType> class SListView;

/**
 * UL-6 interaction timeline / script panel.
 *
 * Pick a script asset, run it against the host panel (step list shows pass/fail with
 * messages, failures are selectable), record semantic steps live (key presses and focus
 * changes become PressKey/Navigate steps; a button snapshots the current focus as an
 * ExpectFocus assertion), and save recorded steps back into the asset.
 */
class SEEUILabScriptPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SEEUILabScriptPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<SEEUILabHostPanel>& InHostPanel);
	virtual ~SEEUILabScriptPanel() override;

	/** Selects a script (UX-2 Content Browser action). */
	void SetActiveScript(UEFUIInteractionScript* Script);

private:
	struct FStepRow
	{
		int32 StepIndex = INDEX_NONE;
		FString Description;
		/** -1 pending, 0 failed, 1 passed */
		int32 Status = -1;
		FString Message;
	};

	TSharedRef<SWidget> MakeScriptPickerButton();
	TSharedRef<SWidget> MakeScriptPickerMenu();
	TSharedRef<ITableRow> MakeStepRow(TSharedPtr<FStepRow> Row, const TSharedRef<STableViewBase>& OwnerTable);

	void RebuildStepRows();
	void HandleRunFinished();
	void HandleInputLogChanged();

	FReply OnRunClicked();
	FReply OnStopClicked();
	FReply OnExpectFocusClicked();
	void OnRecordToggled(bool bEnabled);
	FReply OnSaveClicked();

	TWeakPtr<SEEUILabHostPanel> HostPanel;
	TStrongObjectPtr<UEFUIInteractionScript> ActiveScript;
	TSharedPtr<FEEUIScriptRunner> Runner;

	TArray<TSharedPtr<FStepRow>> StepRows;
	TSharedPtr<SListView<TSharedPtr<FStepRow>>> StepList;

	/** Steps captured while recording; appended to the asset on Save. */
	bool bRecording = false;
	int32 LastSeenLogCount = 0;
	FDelegateHandle LogChangedHandle;
};

#endif // WITH_EDITOR
