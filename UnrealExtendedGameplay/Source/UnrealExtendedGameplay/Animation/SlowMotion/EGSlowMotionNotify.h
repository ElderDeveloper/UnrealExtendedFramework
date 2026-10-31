// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CoreMinimal.h"

#include "EGSlowMotionNotify.generated.h"

/**
 * Sets global time dilation for the length of the window.
 *
 * Only the server (or standalone) changes it; time dilation replicates from the server, and a
 * client changing its own copy would run out of step until the server's value next changed.
 * Overlapping windows (two characters in a finisher, a clip blending into another) are reference
 * counted per world: the world leaves slow motion when the last window ends, not the first.
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

	/** When the last window ends, go back to the time dilation the world had before the first one began. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify")
	bool bRestorePreviousTimeDilation = true;

	/** Time dilation set when the last window ends, when not restoring the previous value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify", meta = (EditCondition = "!bRestorePreviousTimeDilation"))
	float EndSpeed;

private:
	struct FWorldSlowMotion
	{
		int32 Count = 0;
		float PreviousTimeDilation = 1.0f;
	};

	/** Shared by every instance of this notify, so windows from different clips and characters stack. */
	static TMap<TWeakObjectPtr<UWorld>, FWorldSlowMotion> ActiveWorlds;

	static UWorld* GetAuthorityWorld(const USkeletalMeshComponent* MeshComp);
};
