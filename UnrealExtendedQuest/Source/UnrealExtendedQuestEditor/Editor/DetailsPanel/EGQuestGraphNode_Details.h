// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "IDetailCustomization.h"
#include "IDetailPropertyRow.h"

#include "UnrealExtendedQuest/EGQuestManager.h"
#include "UnrealExtendedQuestEditor/Editor/Nodes/EGQuestGraphNode.h"
#include "EGQuestDetailsPanelUtils.h"

class FEGQuestObject_CustomRowHelper;
class FEGQuestTextPropertyPickList_CustomRowHelper;
class FEGQuestMultiLineEditableTextBox_CustomRowHelper;

/**
 * How the details customization panel looks for UEGQuestGraphNode object
 * See FEGQuestPluginEditorModule::StartupModule for usage.
 */
class UNREALEXTENDEDQUESTEDITOR_API FEGQuestGraphNode_Details : public IDetailCustomization
{
	typedef FEGQuestGraphNode_Details Self;

public:
	// Makes a new instance of this detail layout class for a specific detail view requesting it
	static TSharedRef<IDetailCustomization> MakeInstance() { return MakeShared<Self>(); }

	// IDetailCustomization interface
	/** Called when details should be customized */
	void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	/** Handler for when the text is changed */
	void HandleTextCommitted(const FText& InText, ETextCommit::Type CommitInfo);
	void HandleTextChanged(const FText& InText);

private:
	/** Hold the reference to the Graph Node this represents */
	UEGQuestGraphNode* GraphNode = nullptr;

	/** Cache some properties. */
	// Property Handles
	TSharedPtr<IPropertyHandle> TextPropertyHandle;
	TSharedPtr<IPropertyHandle> DescriptionPropertyHandle;

	// Property rows
	TSharedPtr<FEGQuestMultiLineEditableTextBox_CustomRowHelper> TextPropertyRow;
	TSharedPtr<FEGQuestMultiLineEditableTextBox_CustomRowHelper> DescriptionPropertyRow;

	/** The details panel layout builder reference. */
	IDetailLayoutBuilder* DetailLayoutBuilder = nullptr;

	/** Hold a reference to quest we are displaying. */
	UEGQuestGraph* Quest = nullptr;
};
