// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "EGSpawnScaledParticle.generated.h"


class UParticleSystemComponent;
class UCurveVector;

UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGSpawnScaledParticle : public UAnimNotifyState
{
	GENERATED_BODY()

	UEGSpawnScaledParticle();

public:
	// Particle System to Spawn
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AnimNotify", meta=(DisplayName="Particle System"))
	UParticleSystem* PSTemplate;

	// Location offset from the socket
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AnimNotify")
	FVector LocationOffset;

	// Rotation offset from socket
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AnimNotify")
	FRotator RotationOffset;
	
	// Should attach to the bone/socket
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AnimNotify")
	uint32 Attached:1; 	//~ Does not follow coding standard due to redirection from BP
	
	// SocketName to attach to
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify")
	FName SocketName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify")
	FVector StartingScale = FVector(1,1,1);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify")
	FVector EndScale = FVector(1,1,1);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify")
	float ScaleInterpSpeed = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify")
	UCurveVector* ScaleCurve;

protected:

	UPROPERTY()
	UParticleSystemComponent* ParticleSystem;

	UPROPERTY()
	float CurrentTime = 0;
	
	// Cached version of the Rotation Offset already in Quat form
	FQuat RotationOffsetQuat;

#if ENGINE_MAJOR_VERSION == 5
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
#endif


	// Begin UAnimNotify interface
	virtual FString GetNotifyName_Implementation() const override;
};

