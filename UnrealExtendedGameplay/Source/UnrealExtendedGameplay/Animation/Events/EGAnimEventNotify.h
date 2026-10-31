// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "CoreMinimal.h"
#include "EGAnimEventTypes.h"

#include "EGAnimEventNotify.generated.h"

/**
 * Base for single-frame notifies that raise an anim event (IEGAnimEventReceiver). Subclasses
 * decide the tag and payload and may add behaviour of their own around the dispatch.
 */
UCLASS(Abstract)
class UNREALEXTENDEDGAMEPLAY_API UEGAnimEventNotifyBase : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	/** Subclasses return their tag. The default (no tag) dispatches nothing. */
	virtual FGameplayTag GetEventTag() const { return FGameplayTag(); }
	virtual FEGAnimEventPayload GetEventPayload() const { return FEGAnimEventPayload(); }
};

/** Raises a tagged anim event on the owner, its components and its anim instance. */
UCLASS(meta = (DisplayName = "Anim Event"))
class UNREALEXTENDEDGAMEPLAY_API UEGAnimEventNotify : public UEGAnimEventNotifyBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anim Event")
	FGameplayTag EventTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anim Event")
	FEGAnimEventPayload Payload;

protected:
	virtual FGameplayTag GetEventTag() const override { return EventTag; }
	virtual FEGAnimEventPayload GetEventPayload() const override { return Payload; }
};
