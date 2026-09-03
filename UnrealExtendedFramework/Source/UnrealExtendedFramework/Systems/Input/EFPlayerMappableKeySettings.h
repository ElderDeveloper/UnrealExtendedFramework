// Fill out your copyright notice in the Description page of Project Settings.
// ─────────────────────────────────────────────────────────────────────────────
// EFPlayerMappableKeySettings.h
//
// Player Mappable Key Settings that also carry on-screen prompt data.
//
// Enhanced Input lets a mapping carry a UPlayerMappableKeySettings, inherited
// from the action or overridden per mapping inside the context. The engine uses
// it for exactly one thing: key rebinding. This subclass adds what a HUD needs
// to decide whether a mapping is shown and how, as separate fields — so having
// settings never means "is on screen", and a mapping can be remappable without
// being a prompt, or a prompt without being remappable.
//
// Author it per mapping (Setting Behavior = Override Settings) when the same
// action should read differently in different contexts — Close is a prompt on a
// screen but not in gameplay — and on the action when one answer fits every
// context it appears in.
//
// Which prompts are live is a question for UEFInputMappingComponent, which
// applies exactly one context at a time: see GetActiveInputPrompts.
// ─────────────────────────────────────────────────────────────────────────────

#pragma once

#include "CoreMinimal.h"
#include "PlayerMappableKeySettings.h"
#include "Systems/Input/EFInputMappingTypes.h"
#include "EFPlayerMappableKeySettings.generated.h"

class UEnhancedInputUserSettings;
class UInputMappingContext;

UCLASS(DisplayName = "Extended Player Mappable Key Settings")
class UNREALEXTENDEDFRAMEWORK_API UEFPlayerMappableKeySettings : public UPlayerMappableKeySettings
{
	GENERATED_BODY()

public:
	/** Show this mapping in on-screen prompts while its context is the active one. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prompt")
	bool bShowInPrompts = false;

	/** Sort key among the prompts of one context. Lower comes first. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prompt", meta = (EditCondition = "bShowInPrompts"))
	int32 PromptOrder = 0;

	/** Text shown beside the key. Empty falls back to DisplayName. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prompt", meta = (EditCondition = "bShowInPrompts"))
	FText PromptLabel;

	/**
	 * Free-form tag for the presenting widget — chip variant, emphasis, region.
	 * Nothing here interprets it; the game's prompt templates switch on it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prompt", meta = (EditCondition = "bShowInPrompts"))
	FName PromptStyle;

	UFUNCTION(BlueprintPure, Category = "Prompt")
	FText GetPromptLabel() const;

	/**
	 * Remap name. A per-mapping override authored only for its prompt fields
	 * usually leaves Name empty; falling back to the owning action's mappable
	 * name (then its asset name) keeps that mapping in the action's remap row
	 * instead of splitting off a nameless one.
	 */
	virtual FName MakeMappingName(const FEnhancedActionKeyMapping* OwningActionKeyMapping) const override;

	/**
	 * Every mapping in the context flagged for prompts, folded to one entry per
	 * action in mapping order and sorted by PromptOrder. Label, style and order
	 * come from the action's first flagged mapping; every flagged key joins its
	 * Keys. With user settings, each key is the player's current key for that
	 * mapping; without, the authored key.
	 */
	static void CollectInputPrompts(const UInputMappingContext* MappingContext, const UEnhancedInputUserSettings* UserSettings, TArray<FEFInputPromptEntry>& OutPrompts);
};
