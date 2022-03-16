// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EGRandomComboComponent.generated.h"


UENUM()
enum ERandomComboExecuteType
{
	Random,
	InOrder
};



UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGRandomComboComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UEGRandomComboComponent();
	

	UPROPERTY(EditDefaultsOnly)
	TEnumAsByte<ERandomComboExecuteType> RandomComboExecuteType;
	
	UPROPERTY(EditDefaultsOnly)
	TArray<UAnimMontage*> ComboMontages;


	UFUNCTION(BlueprintCallable)
	void ExecuteCombo();

	UFUNCTION(BlueprintCallable)
	void ExecuteComboWithBoolCheck(bool CanExecuteCombo = true);


	UFUNCTION(BlueprintCallable)
	void SetExecuteComboBlocked(bool IsBlocked = false);

	UFUNCTION(BlueprintCallable)
	void SetExecuteComboBlockWithDelay(float DelayTime = 1);

	void SetupComponent(USkeletalMeshComponent* MeshComponent);
	
	void SetEnableNextCombo();


protected:
	
	bool bCanExecuteNextCombo = true;
	bool bExecuteComboBlocked = false;
	int32 CurrentAttackIndex;
	FTimerHandle ComboBlockTimer;
	
	
	UPROPERTY()
	USkeletalMeshComponent* ComboSkeleton;

	void RandomCombo();
	void InOrderCombo();
	void PlayMontage(UAnimMontage* Montage);
	
	UFUNCTION()
	void OnMontageBlendOut( UAnimMontage* Montage, bool bInterrupted);
	virtual void BeginPlay() override;

public:

	UFUNCTION(BlueprintPure , Category="EG Combo Component")
	FORCEINLINE bool GetCanExecuteCombo() const { return bCanExecuteNextCombo; }

	UFUNCTION(BlueprintPure , Category="EG Combo Component")
	FORCEINLINE bool GetComboBlocked() const { return bExecuteComboBlocked; }

	UFUNCTION(BlueprintPure , Category="EG Combo Component")
	FORCEINLINE bool GetIsInCombo() const { return !bCanExecuteNextCombo; }
};


