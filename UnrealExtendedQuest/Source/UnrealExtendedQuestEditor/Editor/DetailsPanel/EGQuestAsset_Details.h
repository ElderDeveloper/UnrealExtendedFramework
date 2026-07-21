// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "IDetailCustomization.h"

/**
 * How the details customization panel looks for the QuestGraph object.
 * See FEGQuestPluginEditorModule::StartupModule for usage.
 */
class UNREALEXTENDEDQUESTEDITOR_API FEGQuestAsset_Details : public IDetailCustomization
{
	typedef FEGQuestAsset_Details Self;
public:
	// Makes a new instance of this detail layout class for a specific detail view requesting it
	static TSharedRef<IDetailCustomization> MakeInstance() { return MakeShared<Self>(); }

	// IDetailCustomization interface
	/** Called when details should be customized */
	void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
