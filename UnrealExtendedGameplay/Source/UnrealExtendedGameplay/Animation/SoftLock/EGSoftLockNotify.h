// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CoreMinimal.h"
#include "UnrealExtendedFramework/Data/EFTraceData.h"

#include "EGSoftLockNotify.generated.h"

/**
 * Turns the owner towards the closest pawn in range for the length of the window. Runs on the
 * server and the owning client; simulated proxies get the rotation through replication.
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
};
