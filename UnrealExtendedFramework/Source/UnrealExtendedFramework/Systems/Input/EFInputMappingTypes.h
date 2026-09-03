// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "EFInputMappingTypes.generated.h"

class UEFPlayerMappableKeySettings;
class UInputAction;
class UInputMappingContext;

/**
 * Ticket for an input mapping set pushed through UEFInputMappingComponent.
 * Ids start at 1 so a default-constructed handle is always invalid.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDFRAMEWORK_API FEFInputMappingHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="ExtendedInput")
	int32 Id = 0;

	FORCEINLINE bool IsValid() const { return Id != 0; }

	FORCEINLINE void Reset() { Id = 0; }

	friend bool operator==(const FEFInputMappingHandle& Left, const FEFInputMappingHandle& Right)
	{
		return Left.Id == Right.Id;
	}

	friend bool operator!=(const FEFInputMappingHandle& Left, const FEFInputMappingHandle& Right)
	{
		return !(Left == Right);
	}

	friend uint32 GetTypeHash(const FEFInputMappingHandle& Handle)
	{
		return ::GetTypeHash(Handle.Id);
	}
};

/**
 * One mapping context plus the priority it competes at.
 *
 * Priority is a raw int32 so the framework stays project-agnostic. Games are
 * expected to name their tiers in their own UENUM and convert on the way in —
 * see ETOTInputMappingPriority / EDOPInputMappingPriority.
 *
 * Higher priority wins. Only the single winning spec is ever applied; there is
 * no stacking, so a spec that wins fully replaces the one below it.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDFRAMEWORK_API FEFInputMappingSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ExtendedInput")
	TObjectPtr<UInputMappingContext> MappingContext = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ExtendedInput")
	int32 Priority = 0;

	FORCEINLINE bool IsValid() const { return MappingContext != nullptr; }

	static FEFInputMappingSpec Make(UInputMappingContext* InMappingContext, const int32 InPriority)
	{
		FEFInputMappingSpec Spec;
		Spec.MappingContext = InMappingContext;
		Spec.Priority = InPriority;
		return Spec;
	}

	friend bool operator==(const FEFInputMappingSpec& Left, const FEFInputMappingSpec& Right)
	{
		return Left.MappingContext == Right.MappingContext && Left.Priority == Right.Priority;
	}

	friend bool operator!=(const FEFInputMappingSpec& Left, const FEFInputMappingSpec& Right)
	{
		return !(Left == Right);
	}
};

/** One live row in the component's mapping list. */
USTRUCT()
struct UNREALEXTENDEDFRAMEWORK_API FEFInputMappingEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FEFInputMappingSpec Spec;

	UPROPERTY()
	FEFInputMappingHandle Handle;

	/** Owner of an override entry. Managed entries leave this null. */
	UPROPERTY()
	TWeakObjectPtr<UObject> Source = nullptr;

	/** Managed entries live until removed by context; override entries until released by handle. */
	UPROPERTY()
	bool bManaged = false;
};

/**
 * One on-screen prompt resolved from a mapping context: an action and the keys
 * that reach it, as authored by UEFPlayerMappableKeySettings. Built by
 * UEFPlayerMappableKeySettings::CollectInputPrompts; the live set comes from
 * UEFInputMappingComponent::GetActiveInputPrompts.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDFRAMEWORK_API FEFInputPromptEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="ExtendedInput|Prompts")
	TObjectPtr<const UInputAction> Action = nullptr;

	/**
	 * Every flagged key for the action in the context, in mapping order. Reflects
	 * the player's remaps when Enhanced Input user settings are enabled. Filtering
	 * by input device is the presenter's job.
	 */
	UPROPERTY(BlueprintReadOnly, Category="ExtendedInput|Prompts")
	TArray<FKey> Keys;

	UPROPERTY(BlueprintReadOnly, Category="ExtendedInput|Prompts")
	FText Label;

	UPROPERTY(BlueprintReadOnly, Category="ExtendedInput|Prompts")
	FName Style;

	/** Lower first. */
	UPROPERTY(BlueprintReadOnly, Category="ExtendedInput|Prompts")
	int32 Order = 0;

	/** The settings Label, Style and Order were read from: the action's first flagged mapping. */
	UPROPERTY(BlueprintReadOnly, Category="ExtendedInput|Prompts")
	TObjectPtr<UEFPlayerMappableKeySettings> Settings = nullptr;
};
