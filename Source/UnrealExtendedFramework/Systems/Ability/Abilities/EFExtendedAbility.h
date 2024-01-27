// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "EFExtendedAbility.generated.h"

class UEFExtendedAbilityComponent;
class UCharacterMovementComponent;
class ULevel;

/**
 *	Defines how an ability is meant to activate.
 */
UENUM(BlueprintType)
enum class EFAbilityActivationPolicy : uint8
{
	// Try to activate the ability when the input is triggered.
	OnInputTriggered,
	// Continually try to activate the ability while the input is active.
	WhileInputActive,
	// Try to activate the ability when an avatar is assigned.
	OnSpawn
};



UCLASS(Abstract, Blueprintable, DefaultToInstanced, EditInlineNew)
class UNREALEXTENDEDFRAMEWORK_API UEFExtendedAbility : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	
	friend class UEFExtendedAbility;
	
	UPROPERTY(EditDefaultsOnly , Category="Tick")
	bool bShouldUseExtendedTick = false;

	// Should Ability Creates itself for activation? this is use full for activating the same ability multiple times in a single frame
	UPROPERTY(EditDefaultsOnly , Category="Instance")
	bool bShouldStartAsInstance = false;

	UPROPERTY(EditDefaultsOnly , Category="Instance")
	int32 InstanceCount = 0;

	UPROPERTY(EditDefaultsOnly , Category="Tick")
	FGameplayTag ExtendedAbilityTag;
	
	/**
	 * Ability Name Can Be Used As A Finding Ability In A Array Or Can Be Used In A UI
	 */
	UPROPERTY(EditDefaultsOnly , Category="ExtendedAbility")
	FName ExtendedAbilityName;

	UPROPERTY(EditDefaultsOnly , Category="ExtendedAbility")
	EFAbilityActivationPolicy AbilityActivationPolicy;




	UFUNCTION(BlueprintNativeEvent , Category="ExtendedAbility")
	void OnExtendedAbilityCreated(AActor* Instigator);
	
	UFUNCTION(BlueprintNativeEvent , Category="ExtendedAbility")
	void StartExtendedAbility(AActor* Instigator);
	
	UFUNCTION(BlueprintNativeEvent , Category="ExtendedAbility")
	void StopExtendedAbility(AActor* Instigator);

	UFUNCTION(BlueprintNativeEvent , Category="ExtendedAbility")
	void OnStartInstanceAbility(AActor* Instigator , UEFExtendedAbility* InstanceOwnerAbility, int32 InstanceIndex);

	UFUNCTION(BlueprintNativeEvent , BlueprintCallable , Category="ExtendedAbility")
	void StopInstanceAbility(UEFExtendedAbility* InstanceAbility);

	UFUNCTION(BlueprintNativeEvent , Category="ExtendedAbility")
	void TickExtendedAbility(AActor* Instigator , float DeltaTime);

	UFUNCTION(BlueprintNativeEvent , Category="ExtendedAbility")
	void OnAbilityLevelChanged(int32 NewLevel , int32 OldLevel);
	
	/**
	 * Returns true if this ability can be activated right now. Has no side effects
	 *
	 * This optionally loose Cost check if the ability is marked as ignore cost,
	 * meaning cost attributes are only checked to be < 0 and prevented if 0 or below.
	 *
	 * If Blueprints implements the CanActivateAbility function, they are responsible for ability activation or not
	 */
	UFUNCTION(BlueprintNativeEvent , BlueprintCallable , Category="ExtendedAbility")
	bool CanStartExtendedAbility(AActor* Instigator);

	
	bool StartExtendedAbilityPure(AActor* Instigator);
	bool StopExtendedAbilityPure(AActor* Instigator);
	

	virtual UWorld* GetWorld() const override;
	ULevel* GetLevel() const;

protected:

	UPROPERTY(EditDefaultsOnly , Category="ExtendedAbility")
	FGameplayTagContainer GrantsTags;

	UPROPERTY(EditDefaultsOnly , Category="ExtendedAbility")
	FGameplayTagContainer BlockedTags;
	
	UPROPERTY(BlueprintReadOnly)
	TArray<UEFExtendedAbility*> Instances;

	UPROPERTY(EditDefaultsOnly , BlueprintReadWrite ,  Category="ExtendedAbility")
	int32 AbilityLevel;

	UPROPERTY(EditDefaultsOnly , BlueprintReadWrite , Category="ExtendedAbility")
	int32 AbilityMaxLevel;

	UPROPERTY(BlueprintReadOnly)
	bool bIsAbilityActive = false;
	
	UFUNCTION(BlueprintCallable)
	void StopExtendedAbilityProtected(AActor* Instigator);

public:
	UFUNCTION(BlueprintCallable , Category="ExtendedAbility")
	void SetAbilityLevel(int32 NewLevel);

	UFUNCTION(BlueprintCallable , Category="ExtendedAbility")
	void IncreaseAbilityLevel(int32 Amount = 1);

	UFUNCTION(BlueprintCallable , Category="ExtendedAbility")
	void DecreaseAbilityLevel(int32 Amount = 1);
	
	UFUNCTION(BlueprintPure , Category="ExtendedAbility")
	bool GetIsExtendedAbilityRunning() const;
	
	UFUNCTION(BlueprintPure , Category="ExtendedAbility")
	UEFExtendedAbilityComponent* GetOwnerExtendedComponent() const;

	UFUNCTION(BlueprintPure , Category="ExtendedAbility")
	AActor* GetOwnerAsActor() const;
	
	UFUNCTION(BlueprintPure , Category="ExtendedAbility")
	ACharacter* GetOwnerAsCharacter() const;

	UFUNCTION(BlueprintPure , Category="ExtendedAbility")
	UCharacterMovementComponent* GetOwnerAsCharacterMovement() const;
	
	UFUNCTION(BlueprintPure , Category="ExtendedAbility")
	USkeletalMeshComponent* GetOwnerAsSkeletalMesh() const;

	UFUNCTION(BlueprintPure , Category="ExtendedAbility")
	FRotator GetOwnerActorRotation() const { return GetOwnerAsActor()->GetActorRotation(); }
	
	UFUNCTION(BlueprintPure , Category="ExtendedAbility")
	FVector GetOwnerActorLocation() const { return GetOwnerAsActor()->GetActorLocation(); }
	
	UFUNCTION(BlueprintPure , Category="ExtendedAbility")
	FVector GetOwnerActorForwardVector() const { return GetOwnerAsActor()->GetActorForwardVector(); }
	
	UFUNCTION(BlueprintPure , Category="ExtendedAbility")
	FVector GetOwnerActorUpVector() const { return GetOwnerAsActor()->GetActorUpVector(); }
};


