// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EGWeaponComponentCore.generated.h"


UENUM()
enum EEquipState
{
	Equipped,
	Equipping,
	UnEquipped,
	UnEquipping
};




UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGWeaponComponentCore : public USceneComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UEGWeaponComponentCore();

	
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite , Category="Mesh")
	UStaticMesh* WeaponStaticMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite , Category="Mesh")
	USkeletalMesh* WeaponSkeletalMesh;


	
	UPROPERTY(EditDefaultsOnly , BlueprintReadOnly , Category= "Sockets")
	FName WeaponHolsterAttachmentSocket;

	UPROPERTY(EditDefaultsOnly , BlueprintReadOnly , Category= "Sockets")
	FName WeaponEquipAttachmentSocket;


	
	UPROPERTY(EditDefaultsOnly , Category="Montages|Equip")
	UAnimMontage* EquipMontage;

	UPROPERTY(EditDefaultsOnly , Category="Montages|Equip")
	bool bEquipUseNotify = false;

	UPROPERTY(EditDefaultsOnly , Category="Montages|Equip")
	FName EquipMontageNotify;


	
	UPROPERTY(EditDefaultsOnly , Category="Montages|UnEquip")
	UAnimMontage* UnEquipMontage;

	UPROPERTY(EditDefaultsOnly , Category="Montages|UnEquip")
	bool bUnEquipUseNotify = false;

	UPROPERTY(EditDefaultsOnly , Category="Montages|UnEquip")
	FName UnEquipMontageNotify;


private:

	UPROPERTY()
	UStaticMeshComponent* WeaponSMComponent;
	UPROPERTY()
	USkeletalMeshComponent* WeaponSKComponent;
	UPROPERTY()
	USkeletalMeshComponent* OwnerSK;
	
	FAttachmentTransformRules AttachmentRules={EAttachmentRule::KeepRelative,EAttachmentRule::KeepRelative,EAttachmentRule::KeepRelative,true };

	EEquipState WeaponEquipState = EEquipState::UnEquipped;

protected:

	UFUNCTION()
	void OnWeaponMontageBlendOut(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION()
	void OnWeaponMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload);
	
	virtual void BeginPlay() override;

public:

	UFUNCTION(BlueprintCallable , Category="Weapon|Constuct")
	void WeaponComponentConstruct(USkeletalMeshComponent* AttachTo);

	UFUNCTION(BlueprintCallable , Category="Weapon|Attachment")
	void AttachToHolsterSocket(USkeletalMeshComponent* AttachTo = nullptr);

	UFUNCTION(BlueprintCallable , Category="Weapon|Attachment")
	void AttachToEquipSlot(USkeletalMeshComponent* AttachTo = nullptr);

	UFUNCTION(BlueprintCallable , Category="Weapon|Attachment")
	void EquipWeapon();

	UFUNCTION(BlueprintCallable , Category="Weapon|Attachment")
	void UnEquipWeapon();

	UFUNCTION(BlueprintCallable , Category="Weapon|Attachment")
	void ChangeEquipState();
	

	UFUNCTION(BlueprintPure , Category="Weapon|Components")
	FORCEINLINE UStaticMeshComponent* GetWeaponStaticMesh() const { return WeaponSMComponent; }
	
	UFUNCTION(BlueprintPure , Category="Weapon|Components")
	FORCEINLINE USkeletalMeshComponent* GetWeaponSkeletalMesh() const { return WeaponSKComponent; }

	UFUNCTION(BlueprintPure , Category="Weapon|Accessors")
	FORCEINLINE bool GetIsEquippedWeapon() const {	return WeaponEquipState == Equipped; }
};

