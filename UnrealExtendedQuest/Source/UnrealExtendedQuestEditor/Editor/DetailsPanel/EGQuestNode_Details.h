// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

/**
 * How the details customization panel looks for the UEGQuestNode (and derived types)
 * See FEGQuestPluginEditorModule::StartupModule for usage.
 */
class UNREALEXTENDEDQUESTEDITOR_API FEGQuestNode_Details : public IDetailCustomization
{
	typedef FEGQuestNode_Details Self;

public:
	// Makes a new instance of this detail layout class for a specific detail view requesting it
	static TSharedRef<IDetailCustomization> MakeInstance() { return MakeShared<Self>(); }

	// IDetailCustomization interface
	/** Called when details should be customized */
	void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
};
