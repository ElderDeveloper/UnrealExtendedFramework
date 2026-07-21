// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "DetailWidgetRow.h"

#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "EGQuestBase_CustomRowHelper.h"
#include "SEGQuestTextPropertyEditableTextBox.h"

class FDetailWidgetRow;
class UEGQuestGraph;

// Custom row for integers
class UNREALEXTENDEDQUESTEDITOR_API FEGQuestIntTextBox_CustomRowHelper :
	public FEGQuestBase_CustomRowHelper,
	public TSharedFromThis<FEGQuestIntTextBox_CustomRowHelper>
{
	typedef FEGQuestIntTextBox_CustomRowHelper Self;
	typedef FEGQuestBase_CustomRowHelper Super;
public:
	FEGQuestIntTextBox_CustomRowHelper(
		FDetailWidgetRow* InDetailWidgetRow,
		const TSharedPtr<IPropertyHandle>& InPropertyHandle,
		const UEGQuestGraph* InQuest
	) : FEGQuestBase_CustomRowHelper(InDetailWidgetRow, InPropertyHandle), Quest(InQuest) {}

	Self& SetJumpToNodeVisibility(const TAttribute<EVisibility>& Visibility)
	{
		JumpToNodeVisibility = Visibility;
		return *this;
	}

protected:
	void UpdateInternal() override;

	// Reset to default
	FText GetResetToolTip() const;
	EVisibility GetDiffersFromDefaultAsVisibility() const;
	FReply OnResetClicked();
	FReply OnJumpToNodeClicked();

private:
	bool bAddResetToDefaultWidget = true;

	TWeakObjectPtr<const UEGQuestGraph> Quest = nullptr;
	TAttribute<EVisibility> JumpToNodeVisibility;
};
