// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "DetailWidgetRow.h"

#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "EGQuestBase_CustomRowHelper.h"
#include "SEGQuestTextPropertyEditableTextBox.h"

class FEGQuestEditableTextPropertyHandle;
class FDetailWidgetRow;

struct UNREALEXTENDEDQUESTEDITOR_API FEGQuestMultiLineEditableTextBoxOptions
{
	typedef FEGQuestMultiLineEditableTextBoxOptions Self;
public:
	FEGQuestMultiLineEditableTextBoxOptions() {}

	void SetDefaults();

public:
	bool bSelectAllTextWhenFocused = false;
	bool bClearKeyboardFocusOnCommit = false;
	bool bSelectAllTextOnCommit = false;
	bool bAutoWrapText = true;

	float WrapTextAt = 0.f;
	EModifierKey::Type ModiferKeyForNewLine = EModifierKey::None;

	// Can not be set here
	TAttribute<FSlateFontInfo> Font;
	TAttribute<FSlateColor> ForegroundColor;
	TAttribute<FSlateColor> ReadOnlyForegroundColor;
	FEditableTextBoxStyle Style;
};

/**
 * Helper for a custom row when using SMultiLineEditableTextBox.
 */
class UNREALEXTENDEDQUESTEDITOR_API FEGQuestMultiLineEditableTextBox_CustomRowHelper :
	public FEGQuestBase_CustomRowHelper,
	public TSharedFromThis<FEGQuestMultiLineEditableTextBox_CustomRowHelper>
{
	typedef FEGQuestMultiLineEditableTextBox_CustomRowHelper Self;
	typedef FEGQuestBase_CustomRowHelper Super;

public:
	FEGQuestMultiLineEditableTextBox_CustomRowHelper(FDetailWidgetRow* InDetailWidgetRow, const TSharedPtr<IPropertyHandle>& InPropertyHandle)
		: FEGQuestBase_CustomRowHelper(InDetailWidgetRow, InPropertyHandle)
	{
		Options.SetDefaults();
	}

#define CREATE_OPTIONS_SETTER(_NameMethod, _VariableType, _OptionVariableName)  \
	Self& _NameMethod(_VariableType InVariableValue)              \
	{                                                             \
		Options._OptionVariableName = InVariableValue;            \
		return *this;                                             \
	}

	CREATE_OPTIONS_SETTER(Style, const FEditableTextBoxStyle&, Style)
	CREATE_OPTIONS_SETTER(Font, const TAttribute<FSlateFontInfo>&, Font)
	CREATE_OPTIONS_SETTER(ForegroundColor, const TAttribute<FSlateColor>&, ForegroundColor)
	CREATE_OPTIONS_SETTER(ReadOnlyForegroundColor, const TAttribute<FSlateColor>&, ReadOnlyForegroundColor)
	CREATE_OPTIONS_SETTER(SelectAllTextWhenFocused, bool, bSelectAllTextWhenFocused)
	CREATE_OPTIONS_SETTER(ClearKeyboardFocusOnCommit, bool, bClearKeyboardFocusOnCommit)
	CREATE_OPTIONS_SETTER(SelectAllTextOnCommit, bool, bSelectAllTextOnCommit)
	CREATE_OPTIONS_SETTER(AutoWrapText, bool, bAutoWrapText)
	CREATE_OPTIONS_SETTER(WrapTextAt, float, WrapTextAt)
	CREATE_OPTIONS_SETTER(ModiferKeyForNewLine, EModifierKey::Type, ModiferKeyForNewLine)

#undef CREATE_OPTIONS_SETTER

	/** Gets the value of the text property. */
	FText GetTextValue() const;

	// NOTE: only call these after Update()
	FEGQuestTextCommitedDelegate& OnTextCommittedEvent();
	FEGQuestTextChangedDelegate& OnTextChangedEvent();


protected:
	void UpdateInternal() override;

private:
	// Events
	FEGQuestTextCommitedDelegate TextCommittedEvent;

	// The text box Widget.
	TSharedPtr<SEGQuestTextPropertyEditableTextBox> TextBoxWidget;

	// Options for the widget
	FEGQuestMultiLineEditableTextBoxOptions Options;

	// Editable text property
	TSharedPtr<FEGQuestEditableTextPropertyHandle> EditableTextProperty;
};
