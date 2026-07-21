// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "PropertyHandle.h"
#include "PropertyEditorModule.h"

#include "UnrealExtendedQuestEditor/Editor/Nodes/EGQuestGraphNode.h"

#define CREATE_VISIBILITY_CALLBACK(_SelfMethod) \
	TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, _SelfMethod))

#define CREATE_VISIBILITY_CALLBACK_STATIC(_StaticMethod) \
	TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateStatic(_StaticMethod))

#define CREATE_BOOL_CALLBACK(_SelfMethod) \
	TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, _SelfMethod))

#define CREATE_BOOL_CALLBACK_STATIC(_StaticMethod) \
	TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateStatic(_StaticMethod))

// Constants used in this file
static const TCHAR* META_ShowOnlyInnerProperties = TEXT("ShowOnlyInnerProperties");
static const TCHAR* META_UIMin = TEXT("UIMin");
static const TCHAR* META_UIMax = TEXT("UIMax");
static const TCHAR* META_ClampMin = TEXT("ClampMin");
static const TCHAR* META_ClampMax = TEXT("ClampMax");

struct FEGQuestDetailsPanelUtils
{
public:
	/** Gets the appropriate modifier key for an input field depending on the Quest System Settings */
	static EModifierKey::Type GetModifierKeyFromQuestSettings()
	{
		switch (GetDefault<UEGQuestPluginSettings>()->QuestTextInputKeyForNewLine)
		{
		case EEGQuestTextInputKeyForNewLine::ShiftPlusEnter:
			return EModifierKey::Shift;

		case EEGQuestTextInputKeyForNewLine::Enter:
		default:
			return EModifierKey::None;
		}
	}

	/** Resets the numeric property to not have any limits */
	static void ResetNumericPropertyLimits(const TSharedPtr<IPropertyHandle>& PropertyHandle)
	{
		if (!PropertyHandle.IsValid())
		{
			return;
		}

		auto* Property = PropertyHandle->GetProperty();
		Property->RemoveMetaData(META_UIMin);
		Property->RemoveMetaData(META_UIMax);
		Property->RemoveMetaData(META_ClampMin);
		Property->RemoveMetaData(META_ClampMax);
	}

	/** Sets the limits of the numeric property. It can only have values in the range [Min, Max] */
	template <typename NumericType>
	static void SetNumericPropertyLimits(const TSharedPtr<IPropertyHandle>& PropertyHandle, const NumericType Min, const NumericType Max)
	{
		if (!PropertyHandle.IsValid())
		{
			return;
		}

		// Clamp Current value if not in range
		NumericType NumericValue;
		if (PropertyHandle->GetValue(NumericValue) != FPropertyAccess::Success)
		{
			return;
		}
		if (PropertyHandle->SetValue(FMath::Clamp(NumericValue, Min, Max)) != FPropertyAccess::Success)
		{
			return;
		}

		const FString MinString = FString::FromInt(Min);
		const FString MaxString = FString::FromInt(Max);
		auto* Property = PropertyHandle->GetProperty();

		// min
		Property->SetMetaData(META_UIMin, *MinString);
		Property->SetMetaData(META_ClampMin, *MinString);

		// max
		Property->SetMetaData(META_UIMax, *MaxString);
		Property->SetMetaData(META_ClampMax, *MaxString);
	}

	/** Gets the Base GraphNode owner that belongs to this PropertyHandle */
	static UEGQuestGraphNode_Base* GetGraphNodeBaseFromPropertyHandle(const TSharedRef<IPropertyHandle>& PropertyHandle);

	/** Similar to the Base node only this always returns a UEGQuestGraphNode */
	static UEGQuestGraphNode* GetClosestGraphNodeFromPropertyHandle(const TSharedRef<IPropertyHandle>& PropertyHandle);

	/** Gets the Quest that is the top most root owner of this PropertyHandle. used in the details panel. */
	static UEGQuestGraph* GetQuestFromPropertyHandle(const TSharedRef<IPropertyHandle>& PropertyHandle);

};
