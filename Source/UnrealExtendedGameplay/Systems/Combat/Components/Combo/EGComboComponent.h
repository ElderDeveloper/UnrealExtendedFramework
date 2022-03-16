// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EGComboComponent.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFastComboAvailableDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStrongComboAvailableDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFastComboDisableDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStrongComboDisableDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnComboMontagesEnded);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGComboComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UEGComboComponent();

	UPROPERTY(BlueprintAssignable)
	FOnFastComboAvailableDelegate OnFastComboAvailableDelegate;

	UPROPERTY(BlueprintAssignable)
	FOnStrongComboAvailableDelegate OnHoldComboAvailableDelegate;

	UPROPERTY(BlueprintAssignable)
	FOnFastComboDisableDelegate OnFastComboDisableDelegate;

	UPROPERTY(BlueprintAssignable)
	FOnStrongComboDisableDelegate OnHoldComboDisableDelegate;

	UPROPERTY(BlueprintAssignable)
	FOnComboMontagesEnded OnComboMontagesEnded;
	
protected:

	UPROPERTY()
	USkeletalMeshComponent* ComboSkeleton;

	virtual void BeginPlay() override;

public:


protected:

	UFUNCTION()
	void OnCombatMontagesEnd();

	UFUNCTION()
	void OnMontageBlendOut(UAnimMontage* Montage, bool bInterrupted);

};
