// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "DetailWidgetRow.h"

#include "EGQuestBase_CustomRowHelper.h"

class SEGQuestTextPropertyPickList;
class FDetailWidgetRow;

// Helper for details panel, when we want to use SEGQuestTextPropertyPickList in a custom row in the details panel
class UNREALEXTENDEDQUESTEDITOR_API FEGQuestTextPropertyPickList_CustomRowHelper : public FEGQuestBase_CustomRowHelper
{
	typedef FEGQuestTextPropertyPickList_CustomRowHelper Self;
	typedef FEGQuestBase_CustomRowHelper Super;
public:
	FEGQuestTextPropertyPickList_CustomRowHelper(FDetailWidgetRow* InDetailWidgetRow, const TSharedPtr<IPropertyHandle>& InPropertyHandle)
		: FEGQuestBase_CustomRowHelper(InDetailWidgetRow, InPropertyHandle) {}

	// Set the SPropertyPickList
	Self& SetTextPropertyPickListWidget(const TSharedRef<SEGQuestTextPropertyPickList>& InWidget)
	{
		TextPropertyPickListWidget = InWidget;
		return *this;
	}

	// Call this before Update is called to have the default buttons (like array add/remove/duplicate) added next to the row
	// Derived rows can provide a project-specific value list.
	void SetParentStructPropertyHandle(const TSharedRef<IPropertyHandle>& InParentStructPropertyHandle) { ParentStructPropertyHandle = InParentStructPropertyHandle; }

private:
	void UpdateInternal() override;

private:
	// The TextPropertyPickList Widget.
	TSharedPtr<SEGQuestTextPropertyPickList> TextPropertyPickListWidget;


	// Optional struct widget for additional buttons for one liners, only used if set
	TSharedPtr<IPropertyHandle> ParentStructPropertyHandle;
};
