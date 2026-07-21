// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "IPropertyTypeCustomization.h"

/**
 * Replacement for the engine's FGameplayTagQueryCustomization, identical in UI (the engine's own
 * public SGameplayTagQueryEntryBox renders the row).
 *
 * Why it exists: with the engine's registration in place, building a details tree that contains any
 * FGameplayTagQuery property at category level recurses infinitely inside the detail-node
 * generation until the editor dies with EXCEPTION_STACK_OVERFLOW (reproduced deterministically by
 * the DOPInspectQuest commandlet's -Probe=plainquery walk; the same stack signature crashed the
 * quest editor when editing role definitions). Re-registering an equivalent customization from this
 * module does not trigger the loop, so on PostEngineInit the plugin swaps the engine's registration
 * for this one. Remove when the engine bug is fixed.
 */
class FEGQuestTagQuery_Details : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	void CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	void CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override {}
};
