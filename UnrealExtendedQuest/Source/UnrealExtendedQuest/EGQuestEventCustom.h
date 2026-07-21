// Copyright Csaba Molnar, Daniel Butum. All Rights Reserved.
#pragma once
#include "EGQuestObject.h"
#include "EGQuestSearchTerm.h"

#include "EGQuestEventCustom.generated.h"

class UEGQuestContext;

// Base class for every quest event. Every non-abstract subclass - native or Blueprint - is offered
// automatically by the event picker, so adding an event type means adding a class and nothing else.
//
// CollapseCategories keeps an event's properties flat under the picker instead of nesting them
// inside their category groups, so picking an event shows its fields immediately.
//
// 1. Override EnterEvent
UCLASS(Blueprintable, BlueprintType, Abstract, EditInlineNew, CollapseCategories)
class UNREALEXTENDEDQUEST_API UEGQuestEventCustom : public UEGQuestObject
{
	GENERATED_BODY()
public:
	// Called when the event is triggered.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Quest", DisplayName = "Enter")
	void EnterEvent(UEGQuestContext* Context);
	virtual void EnterEvent_Implementation(UEGQuestContext* Context) {}

	// Display text for editor graph node
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Quest")
	FString GetEditorDisplayString(UEGQuestGraph* OwnerQuest);
	virtual FString GetEditorDisplayString_Implementation(UEGQuestGraph* OwnerQuest)
	{
#if WITH_EDITOR
		return GetClass()->GetDisplayNameText().ToString();
#else
		return GetName();
#endif
	}

	/** Editor compile check. Return false and fill OutError to raise a warning on the owning node. */
	virtual bool ValidateForCompile(FString& OutError) const { return true; }

	/** Reports this event's searchable fields to Find-in-Quests. The class name is added for you. */
	virtual void GetSearchTerms(TArray<FEGQuestSearchTerm>& OutTerms) const {}
};
