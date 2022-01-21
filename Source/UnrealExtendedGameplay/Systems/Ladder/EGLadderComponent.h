// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UnrealExtendedFramework/Data/EFTraceData.h"
#include "EGLadderComponent.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLadderStateChanged , bool , LadderState);


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGLadderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEGLadderComponent();
	

	UPROPERTY(EditDefaultsOnly)
	UAnimMontage* EnterLadderMontage;

	UPROPERTY(EditDefaultsOnly)
	UAnimMontage* LeaveLadderFootMontage;

	UPROPERTY(EditDefaultsOnly)
	UAnimMontage* LeaveLadderClimbMontage;

	
	UPROPERTY(EditDefaultsOnly , Category= "Ladder Settings")
	FLineTraceStruct GroundTraceStruct;

	UPROPERTY(EditDefaultsOnly , Category= "Ladder Settings")
	float LadderSpeed = 10;

	
	UPROPERTY(EditDefaultsOnly , Category= "Ladder Settings")
	float LadderTraceUpPlus= 40;

	UPROPERTY(EditDefaultsOnly , Category= "Ladder Settings")
	float LadderTraceDownPlus= -40;

	UPROPERTY(EditDefaultsOnly , Category= "Ladder Settings")
	float LadderEnterInterpSpeed = 10;

	UPROPERTY(EditDefaultsOnly , Category= "Ladder Settings")
	float LadderGroundCheckDistance = 10;



	UPROPERTY(BlueprintAssignable)
	FOnLadderStateChanged OnLadderStateChanged;

	
	UFUNCTION(BlueprintCallable)
	void ProcessLadderClimb(const float Axis , const float DeltaTime);

	UFUNCTION(BlueprintCallable)
	void TryEnterLadder();
	
protected:

	UPROPERTY()
	class USkeletalMeshComponent* PlayerMesh;

	UPROPERTY()
	class ACharacter* Player;

	UPROPERTY()
	class AEGLadder* Ladder;
	
	bool IsReadyForEnterLadder = false;
	bool IsInLadder = false;
	bool IsReadyForClimbLeaveLadder = false;
	bool IsReadyForFootLeaveLadder = false;
	
	bool IsInMontage = false;

	bool IsEnteringLadder = false;
	bool IsEnterMontageValid = false;


	bool IsLeavingLadder = false;


	bool IsClimbLeaveLadder = false;
	bool IsClimbLeaveMontageValid = false;
	
	
	bool IsWaitingMontageToEnd = false;
	bool MontageEndLadderState;
	float DistanceToGround;

	

public:

	UFUNCTION(BlueprintPure , Category="Ladder")
	FORCEINLINE bool GetIsReadyForEnterLadder() const {	return IsReadyForEnterLadder;	}
	
	UFUNCTION(BlueprintPure , Category="Ladder")
	FORCEINLINE bool GetIsInLadder() const {	return IsInLadder;	}
	
	UFUNCTION(BlueprintPure , Category="Ladder")
	FORCEINLINE bool GetIsReadyForClimbLeaveLadder() const {	return IsReadyForClimbLeaveLadder;	}
	
	UFUNCTION(BlueprintPure , Category="Ladder")
	FORCEINLINE bool GetIsReadyForFootLeaveLadder() const {	return IsReadyForFootLeaveLadder;	}
	
	UFUNCTION(BlueprintPure , Category="Ladder")
	FORCEINLINE bool GetIsInMontage() const {	return IsInMontage;	}

	UFUNCTION(BlueprintPure , Category="Ladder")
	FORCEINLINE float GetDistanceToGround() const {	return DistanceToGround;	}

	FORCEINLINE void SetIsReadyForEnterLadder (const bool Enable , class AEGLadder* ladder ) { IsReadyForEnterLadder = Enable; Ladder = ladder; }
	FORCEINLINE void SetIsReadyForClimbLeaveLadder (const bool Enable ) { IsReadyForClimbLeaveLadder = Enable; }
	FORCEINLINE void SetIsReadyForFootLeaveLadder (const bool Enable ) { IsReadyForFootLeaveLadder = Enable; }

	
protected:

	virtual void BeginPlay() override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	void PlayMontageIfValid(UAnimMontage* MontageToPlay , bool InLadderState = false , bool SetStateOnBegin = false);
	
	UFUNCTION()
	void OnMontageBegin(UAnimMontage* MontageToPlay);
	
	UFUNCTION()
	void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnEnterLadderMontageBlendOut(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnLeaveLadderMontageBlendOut(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnLeaveLadderClimbMontageBlendOut(UAnimMontage* Montage, bool bInterrupted);

private:

	void CheckLadderGroundDistance();
	void InterpPlayerToLadderAndStartClimb(const float DeltaTime);
	void InterpPlayerToLadderAndWaitMontage(const float DeltaTime);

	void FootLeaveLadder();
	void ClimbLeaveLadder();
};


