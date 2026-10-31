// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "EGAnimEventReceiver.h"
#include "UnrealExtendedGameplay/Animation/AnimInstance/EGAnimStateProvider.h"

#include "EGAnimEventComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGOnAnimEvent, const FEGAnimEvent&, Event);
DECLARE_MULTICAST_DELEGATE_OneParam(FEGOnAnimEventNative, const FEGAnimEvent&);

/**
 * UEGAnimEventComponent
 *
 * Turns anim events into delegates for Blueprint and for code that should not implement the
 * receiver interface itself, and tracks which event windows are open ("can cancel", "combo
 * input", "invulnerable"), so gameplay can ask instead of keeping its own flags. Open windows are
 * also reported as anim state tags to UEGAnimInstance.
 */
UCLASS(ClassGroup = (Extended), meta = (BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGAnimEventComponent : public UActorComponent, public IEGAnimEventReceiver, public IEGAnimStateProvider
{
	GENERATED_BODY()

public:
	UEGAnimEventComponent();

	/** Only events whose tag matches one of these (or a child of one) are broadcast. Empty broadcasts all. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Anim Events")
	FGameplayTagContainer TagFilter;

	UPROPERTY(BlueprintAssignable, Category = "Anim Events")
	FEGOnAnimEvent OnAnimEvent;

	FEGOnAnimEventNative OnAnimEventNative;

	/** True while a window with this tag, or a child of it, is open. */
	UFUNCTION(BlueprintPure, Category = "Anim Events")
	bool IsWindowActive(FGameplayTag Tag) const;

	/** Tags of every open window. */
	UFUNCTION(BlueprintPure, Category = "Anim Events")
	FGameplayTagContainer GetActiveWindowTags() const;

	virtual void ReceiveAnimEvent_Implementation(const FEGAnimEvent& Event) override;

	/** Open window tags, so UEGAnimInstance::HasStateTag can answer "is the combo window open". */
	virtual FGameplayTagContainer GetAnimStateTags_Implementation() const override { return GetActiveWindowTags(); }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool PassesFilter(const FGameplayTag& Tag) const;

	/** WindowId -> tag of every window that began and has not ended. */
	TMap<int32, FGameplayTag> ActiveWindows;
};
