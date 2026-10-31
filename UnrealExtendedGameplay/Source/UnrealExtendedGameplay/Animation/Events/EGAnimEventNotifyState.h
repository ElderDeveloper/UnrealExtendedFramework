// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CoreMinimal.h"
#include "EGAnimEventTypes.h"

#include "EGAnimEventNotifyState.generated.h"

/**
 * Base for notify windows that raise Begin / Tick / End anim events (IEGAnimEventReceiver).
 * Every window gets a WindowId so receivers can tell overlapping windows with the same tag apart.
 */
UCLASS(Abstract)
class UNREALEXTENDEDGAMEPLAY_API UEGAnimEventNotifyStateBase : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	/** Also send an event every tick of the window. Off by default: most receivers only need Begin and End. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Anim Event")
	bool bSendTickEvents = false;

	virtual FString GetNotifyName_Implementation() const override;
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	virtual FGameplayTag GetEventTag() const PURE_VIRTUAL(UEGAnimEventNotifyStateBase::GetEventTag, return FGameplayTag(););
	virtual FEGAnimEventPayload GetEventPayload() const { return FEGAnimEventPayload(); }

private:
	void Send(EEGAnimEventPhase Phase, USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float Time, int32 WindowId) const;

	/**
	 * Open windows per mesh, oldest first. The notify object is shared by every mesh playing the
	 * clip, and the same window can overlap itself when a montage blends into itself.
	 */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, TArray<int32>> OpenWindows;
};

/** Raises tagged Begin / End (and optionally Tick) anim events for the length of the window. */
UCLASS(meta = (DisplayName = "Anim Event Window"))
class UNREALEXTENDEDGAMEPLAY_API UEGAnimEventNotifyState : public UEGAnimEventNotifyStateBase
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
