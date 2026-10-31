// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CoreMinimal.h"
#include "UnrealExtendedFramework/Data/EFTraceData.h"
#include "UnrealExtendedGameplay/Animation/TurnInPlace/EGTurnInPlaceTypes.h"

#include "EGSoftLockNotify.generated.h"

/**
 * Turns the owner towards its lock-on target (UEGTargetingComponent / UEGDynamicTargetingComponent),
 * else the closest pawn in range, for the length of the window. Runs on the server and the owning
 * client; simulated proxies get the rotation through replication.
 *
 * For attacks driven by root motion, Set Warp Target with Motion Warping reaches the target too,
 * instead of only turning towards it.
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGSoftLockNotify : public UAnimNotifyState
{
	GENERATED_BODY()
	
	
	UPROPERTY(EditAnywhere, Category = "Soft Lock")
	FSphereTraceStruct SoftLockActorTrace = FSphereTraceStruct(300);

	UPROPERTY(EditAnywhere, Category = "Soft Lock")
	float SoftLockInterpSpeed = 5;

	
protected:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
		
	AActor* FindSoftLockActor(USkeletalMeshComponent* MeshComp) const;
	AActor* GetSoftLockActor(USkeletalMeshComponent* MeshComp);
	void ClearSoftLockActor(USkeletalMeshComponent* MeshComp);
	void TickSoftLock(USkeletalMeshComponent* MeshComp, float FrameDeltaTime);
private:
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, TWeakObjectPtr<AActor>> SoftLockActors;

	/** Turn in place is paused while this notify turns the actor, so the two do not fight. Oldest first. */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, TArray<FEGTurnInPlaceRequestHandle>> TurnInPlacePauses;
};
