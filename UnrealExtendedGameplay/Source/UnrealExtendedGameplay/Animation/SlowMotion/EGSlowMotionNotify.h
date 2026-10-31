// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CoreMinimal.h"
#include "UnrealExtendedGameplay/Time/EGTimeDilationSubsystem.h"

#include "EGSlowMotionNotify.generated.h"

/**
 * Global slow motion for the length of the window, through UEGTimeDilationSubsystem.
 *
 * Only the server (or standalone) changes it: time dilation replicates from the server. Overlapping
 * windows (two characters in a finisher, a clip blending into another) stack in the subsystem, and
 * the world leaves slow motion when the last one ends.
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGSlowMotionNotify : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UEGSlowMotionNotify();

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify")
	float BeginSpeed;

	/** Priority, blend in and blend out (real seconds). Duration is ignored: the window decides. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify")
	FEGTimeDilationParams Blend;

	/** When the last window ends, go back to the time dilation the world had before the first one began. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify")
	bool bRestorePreviousTimeDilation = true;

	/** Time dilation set when the last window ends, when not restoring the previous value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify", meta = (EditCondition = "!bRestorePreviousTimeDilation"))
	float EndSpeed;

private:
	/** Handles per mesh, oldest first; the notify object is shared by every mesh playing the clip. */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, TArray<FEGTimeDilationHandle>> ActiveHandles;
};
