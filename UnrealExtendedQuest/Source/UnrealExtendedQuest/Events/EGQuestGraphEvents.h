// Copyright Devil of the Plague. All Rights Reserved.
#pragma once

#include "UnrealExtendedQuest/EGQuestEventCustom.h"
#include "GameplayTagContainer.h"

#include "EGQuestGraphEvents.generated.h"

class UEGQuestContext;
class UEGQuestGraph;

/** Broadcasts a name through UEGQuestContext::OnNamedQuestEvent. */
UCLASS(BlueprintType, DisplayName = "Named Quest Event")
class UNREALEXTENDEDQUEST_API UEGQuestEvent_NamedEvent : public UEGQuestEventCustom
{
	GENERATED_BODY()

public:
	void EnterEvent_Implementation(UEGQuestContext* Context) override;
	FString GetEditorDisplayString_Implementation(UEGQuestGraph* OwnerQuest) override;
	bool ValidateForCompile(FString& OutError) const override;
	void GetSearchTerms(TArray<FEGQuestSearchTerm>& OutTerms) const override;

	FName GetEventName() const { return EventName; }
	void SetEventName(const FName InEventName) { EventName = InEventName; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	FName EventName = NAME_None;
};

/**
 * Emits a gameplay-tag notification through UEGQuestComponent::OnQuestGameplayNotify.
 * Authority-only (like every quest event); games relay it into their own event pipeline.
 */
UCLASS(BlueprintType, DisplayName = "Gameplay Notify (Tag)")
class UNREALEXTENDEDQUEST_API UEGQuestEvent_GameplayNotify : public UEGQuestEventCustom
{
	GENERATED_BODY()

public:
	void EnterEvent_Implementation(UEGQuestContext* Context) override;
	FString GetEditorDisplayString_Implementation(UEGQuestGraph* OwnerQuest) override;
	bool ValidateForCompile(FString& OutError) const override;
	void GetSearchTerms(TArray<FEGQuestSearchTerm>& OutTerms) const override;

	FGameplayTag GetNotifyTag() const { return NotifyTag; }
	void SetNotifyTag(const FGameplayTag& InTag) { NotifyTag = InTag; }
	float GetNotifyMagnitude() const { return NotifyMagnitude; }
	void SetNotifyMagnitude(const float InMagnitude) { NotifyMagnitude = InMagnitude; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	FGameplayTag NotifyTag;

	/** Magnitude forwarded with the tag (reward amounts, intensities, ...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event")
	float NotifyMagnitude = 1.0f;
};
