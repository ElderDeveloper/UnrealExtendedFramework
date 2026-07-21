// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#include "EGQuestBase_CustomRowHelper.h"

#include "PropertyHandle.h"
#include "DetailWidgetRow.h"

#define LOCTEXT_NAMESPACE "TextPropertyPickListCustomRowHelper"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FEGQuestBase_CustomRowHelper
FEGQuestBase_CustomRowHelper::FEGQuestBase_CustomRowHelper(FDetailWidgetRow* InDetailWidgetRow, const TSharedPtr<IPropertyHandle>& InPropertyHandle) :
	DetailWidgetRow(InDetailWidgetRow),
	PropertyHandle(InPropertyHandle),
	DisplayName(DetailWidgetRow->FilterTextString),
	ToolTip(InPropertyHandle->GetToolTipText())
{
	check(DetailWidgetRow);
	check(PropertyHandle.IsValid());
}

/** Build the full property row. */
void FEGQuestBase_CustomRowHelper::Update()
{
	// Set display name and tooltips
	// Sets a string which should be used to filter the content when a user searches
	DetailWidgetRow->FilterString(DisplayName);
	PropertyHandle->SetToolTipText(ToolTip);

	NameContentWidget = PropertyHandle->CreatePropertyNameWidget(DisplayName, ToolTip);

	// NOTE set the tooltip of your value content widget inside the implementation class.
	UpdateInternal();
}

#undef LOCTEXT_NAMESPACE
