// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "CoreMinimal.h"

#include "EGSpawnScaledParticle.generated.h"

class UCurveVector;
class UFXSystemComponent;
class UNiagaraSystem;
class UParticleSystem;

/**
 * Spawns an effect for the length of the window and scales it from StartingScale towards
 * EndScale (or along ScaleCurve). The effect is destroyed when the window ends.
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGSpawnScaledParticle : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UEGSpawnScaledParticle();

	/** Niagara system to spawn. Used instead of the Cascade Particle System when both are set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify")
	TObjectPtr<UNiagaraSystem> NiagaraSystem;

	/** Cascade particle system to spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify", meta = (DisplayName = "Particle System"))
	TObjectPtr<UParticleSystem> PSTemplate;

	// Location offset from the socket
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify")
	FVector LocationOffset;

	// Rotation offset from socket
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify")
	FRotator RotationOffset;

	// Should attach to the bone/socket
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify")
	uint32 Attached : 1; //~ Does not follow coding standard due to redirection from BP

	// SocketName to attach to
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify")
	FName SocketName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify")
	FVector StartingScale = FVector(1, 1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify")
	FVector EndScale = FVector(1, 1, 1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify")
	float ScaleInterpSpeed = 4;

	/** Scale over time, in seconds since the window began. Overrides EndScale and ScaleInterpSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify")
	TObjectPtr<UCurveVector> ScaleCurve;

	virtual FString GetNotifyName_Implementation() const override;
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

private:
	struct FActiveEffect
	{
		TWeakObjectPtr<UFXSystemComponent> Component;
		float ElapsedTime = 0.0f;
	};

	/** One entry per mesh currently inside this window. */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FActiveEffect> ActiveEffects;

	UFXSystemComponent* SpawnEffect(USkeletalMeshComponent* MeshComp) const;
	void DestroyEffect(const TWeakObjectPtr<USkeletalMeshComponent>& MeshKey);
};
