// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UEExpandedFramework/Gameplay/Trace/UEExtendedTraceData.h"
#include "UEExtendedLadderComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UEEXPANDEDFRAMEWORK_API UUEExtendedLadderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUEExtendedLadderComponent();
	

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
	float LadderEnterInterpSpeed = 10;

	
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
	class AUEExtendedLadder* Ladder;
	
	bool IsReadyForEnterLadder = false;
	bool IsInLadder = false;
	bool IsReadyForClimbLeaveLadder = false;
	bool IsReadyForFootLeaveLadder = false;
	bool IsInMontage = false;
	bool IsInLadderMoving = false;
	bool IsEnteringLadder = false;
	bool IsEnterMontageValid = false;
	bool IsWaitingMontageToEnd = false;
	bool MontageEndLadderState;

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
	FORCEINLINE bool GetIsInLadderMoving() const {	return IsInLadderMoving;	}

	FORCEINLINE void SetIsReadyForEnterLadder (const bool Enable , class AUEExtendedLadder* ladder ) { IsReadyForEnterLadder = Enable; Ladder = ladder; }
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

private:

	void CheckLadderGroundDistance();

	void InterpPlayerToLadderAndStartClimb(const float DeltaTime);

	void InterpPlayerToLadderAndWaitMontage(const float DeltaTime);
};
