// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UEExpandedFramework/Gameplay/Trace/UEExtendedTraceData.h"
#include "UEExtendedInverseKinematicsInstance.generated.h"

/**
 * 
 */
UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedInverseKinematicsInstance : public UAnimInstance
{
	GENERATED_BODY()

	UUEExtendedInverseKinematicsInstance(const FObjectInitializer& ObjectInitializer);
	
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

public:

	UPROPERTY(EditDefaultsOnly, Category = "Extended IK")
	float IKTraceDistance = 55.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Extended IK")
	float IKAdjustOffset = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Extended IK")
	FName LeftFootSocket = "LeftFootSocket";

	UPROPERTY(EditDefaultsOnly, Category = "Extended IK")
	FName RightFootSocket = "RightFootSocket";

	UPROPERTY(EditDefaultsOnly, Category = "Extended IK")
	float IKHipsInterpSpeed = 7.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Extended IK")
	float IKFeetInterpSpeed = 13.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Extended IK")
	float IKTimeout = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Extended IK")
	FLineTraceStruct LineTraceStruct;
	
	UPROPERTY(BlueprintReadOnly, Category = "Extended IK")
	FVector  LeftJointLoc = FVector(-15.97f, 1000.0f, -15.97f);
	
	UPROPERTY(BlueprintReadOnly, Category = "Extended IK")
	FVector  RightJointLoc = FVector(-16.8f, 26.9, 15.97f);
	
	UPROPERTY(BlueprintReadOnly, Category = "Extended IK")
	float LeftEffectorLoc = 0.0f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Extended IK")
	float RightEffectorLoc = 0.0f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Extended IK")
	float HipsOffset = 0.0f;
	
	UPROPERTY(BlueprintReadOnly, Category = "Extended IK")
	FRotator LeftFootRotation = FRotator(0.f,0.f,0.f);
	
	UPROPERTY(BlueprintReadOnly, Category = "Extended IK")
	FRotator RightFootRotation = FRotator(0.f, 0.f, 0.f);

protected:

	UPROPERTY()
	class USkeletalMeshComponent* CharacterMesh = nullptr;
	UPROPERTY()
	class ACharacter* Character;
	UPROPERTY()
	class UCapsuleComponent* CharacterCapsule;

	
	float IKCapsuleHalfHeight = 0.f;
	FVector Impact = FVector(0.f, 0.f, 0.f);
	float Scale = 0.0f;
	float IKFootTrace(float TraceDistance, FName SocketName);
	void UpdateCapsuleHalfHeight(float HipShifts, bool ResetDefault);

	FRotator NormalToRotator(FVector Normal);

	void IKUpdateFootOffset(float TargetValue, float &EffectorVal, float InterpSpeed);

	void IKUpdateFootRotation(FRotator TargetValue, FRotator &RotationVar, float InterpSpeed);

	void IKResetVars();

	bool IsMoving();

	UFUNCTION(BlueprintCallable, category = "IK")
	void IKUpdate(bool bEnable);
	
};
