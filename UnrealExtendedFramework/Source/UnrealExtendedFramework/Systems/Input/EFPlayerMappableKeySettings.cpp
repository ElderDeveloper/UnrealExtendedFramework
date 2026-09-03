// Fill out your copyright notice in the Description page of Project Settings.

#include "Systems/Input/EFPlayerMappableKeySettings.h"

#include "Algo/StableSort.h"
#include "EnhancedActionKeyMapping.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UserSettings/EnhancedInputUserSettings.h"

namespace
{
	/** The key the player actually has on this mapping, or the authored one when nothing is remapped. */
	FKey ResolveCurrentKey(const FEnhancedActionKeyMapping& Mapping, const UEnhancedPlayerMappableKeyProfile* KeyProfile)
	{
		if (!KeyProfile)
		{
			return Mapping.Key;
		}

		const FName MappingName = Mapping.GetMappingName();
		const FKeyMappingRow* Row = MappingName.IsNone() ? nullptr : KeyProfile->FindKeyMappingRow(MappingName);
		if (!Row)
		{
			return Mapping.Key;
		}

		// A row holds every slot and device for the name; the authored key is
		// what ties this particular mapping to its player mapping.
		for (const FPlayerKeyMapping& PlayerMapping : Row->Mappings)
		{
			if (PlayerMapping.GetDefaultKey() == Mapping.Key && PlayerMapping.GetCurrentKey().IsValid())
			{
				return PlayerMapping.GetCurrentKey();
			}
		}

		return Mapping.Key;
	}
}

FText UEFPlayerMappableKeySettings::GetPromptLabel() const
{
	return PromptLabel.IsEmpty() ? DisplayName : PromptLabel;
}

FName UEFPlayerMappableKeySettings::MakeMappingName(const FEnhancedActionKeyMapping* OwningActionKeyMapping) const
{
	if (!Name.IsNone() || !OwningActionKeyMapping || !OwningActionKeyMapping->Action)
	{
		return Super::MakeMappingName(OwningActionKeyMapping);
	}

	const UInputAction* Action = OwningActionKeyMapping->Action;
	if (const UPlayerMappableKeySettings* ActionSettings = Action->GetPlayerMappableKeySettings(); ActionSettings && ActionSettings != this)
	{
		const FName ActionMappingName = ActionSettings->GetMappingName();
		if (!ActionMappingName.IsNone())
		{
			return ActionMappingName;
		}
	}

	return Action->GetFName();
}

void UEFPlayerMappableKeySettings::CollectInputPrompts(const UInputMappingContext* MappingContext, const UEnhancedInputUserSettings* UserSettings, TArray<FEFInputPromptEntry>& OutPrompts)
{
	OutPrompts.Reset();

	if (!MappingContext)
	{
		return;
	}

	const UEnhancedPlayerMappableKeyProfile* KeyProfile = UserSettings ? UserSettings->GetActiveKeyProfile() : nullptr;

	for (const FEnhancedActionKeyMapping& Mapping : MappingContext->GetMappings())
	{
		if (!Mapping.Action || !Mapping.Key.IsValid())
		{
			continue;
		}

		UEFPlayerMappableKeySettings* Settings = Mapping.GetPlayerMappableKeySettings<UEFPlayerMappableKeySettings>();
		if (!Settings || !Settings->bShowInPrompts)
		{
			continue;
		}

		FEFInputPromptEntry* Entry = OutPrompts.FindByPredicate(
			[&Mapping](const FEFInputPromptEntry& Existing)
			{
				return Existing.Action == Mapping.Action;
			});

		if (!Entry)
		{
			Entry = &OutPrompts.AddDefaulted_GetRef();
			Entry->Action = Mapping.Action;
			Entry->Label = Settings->GetPromptLabel();
			Entry->Style = Settings->PromptStyle;
			Entry->Order = Settings->PromptOrder;
			Entry->Settings = Settings;
		}

		Entry->Keys.AddUnique(ResolveCurrentKey(Mapping, KeyProfile));
	}

	// Stable, so actions with the same order keep their mapping order.
	Algo::StableSort(OutPrompts, [](const FEFInputPromptEntry& Left, const FEFInputPromptEntry& Right)
	{
		return Left.Order < Right.Order;
	});
}
