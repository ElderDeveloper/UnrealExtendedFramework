// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "UnrealExtendedQuestEditor/EGQuestEditorUtilities.h"
#include "IDetailPropertyRow.h"

class FDetailWidgetRow;
class UEGQuestGraph;
class UBlueprint;


// Custom row for Objects that most likely are Blueprints?
// This only works with the IDetailPropertyRow::CustomWidget
// If we don't use this the children of EditInlineNew won't be displayed
// Custom row for things that can Handle Objects that are most likely Blueprints or Native Classes
// And this helps us to open them in the Blueprint Editor or open the Native Class inside the IDE
class UNREALEXTENDEDQUESTEDITOR_API FEGQuestObject_CustomRowHelper : public TSharedFromThis<FEGQuestObject_CustomRowHelper>
{
	typedef FEGQuestObject_CustomRowHelper Self;

public:
	FEGQuestObject_CustomRowHelper(IDetailPropertyRow* InPropertyRow);
	virtual ~FEGQuestObject_CustomRowHelper() {}

	// Update the full property row.
	void Update();

	// Mutually exclusive with
	Self& SetFunctionNameToOpen(EEGQuestBlueprintOpenType InOpenType, FName Name)
	{
		OpenType = InOpenType;
		FunctionNameToOpen = Name;
		return *this;
	}

protected:
	// Reset to default
	virtual FReply OnBrowseClicked();
	virtual FReply OnOpenClicked();

	virtual UObject* GetObject() const;
	UBlueprint* GetBlueprint() const;
	bool IsObjectABlueprint() const;

	virtual FText GetBrowseObjectText() const;
	virtual FText GetJumpToObjectText() const;
	virtual float GetRowMinimumDesiredWidth() const { return 300.f; }

	EVisibility GetOpenButtonVisibility() const;
	EVisibility GetBrowseButtonVisibility() const;
	virtual bool CanBeVisible() const { return true; }

protected:
	// The Property handle of what this row represents
	IDetailPropertyRow* PropertyRow = nullptr;

	// Blueprint Editor
	bool bForceFullEditor = true;
	FName FunctionNameToOpen = NAME_None;
	bool bAddBlueprintFunctionIfItDoesNotExist = true;
	EEGQuestBlueprintOpenType OpenType = EEGQuestBlueprintOpenType::None;
};
