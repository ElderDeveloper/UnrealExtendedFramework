// Fill out your copyright notice in the Description page of Project Settings.


#include "EGWeaponComponentCore.h"

#include "UnrealExtendedFramework/Data/EFMacro.h"


// Sets default values for this component's properties
UEGWeaponComponentCore::UEGWeaponComponentCore()
{
	PrimaryComponentTick.bCanEverTick = true;

	WeaponSMComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Weapon Static Mesh"));
	WeaponSMComponent->SetupAttachment(this);
	WeaponSMComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	WeaponSMComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponSMComponent->SetStaticMesh(WeaponStaticMesh);

	WeaponSKComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Weapon Skeletal Mesh"));
	WeaponSKComponent->SetupAttachment(this);
	WeaponSKComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	WeaponSKComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponSKComponent->SetSkeletalMesh(WeaponSkeletalMesh);
}


void UEGWeaponComponentCore::WeaponComponentConstruct(USkeletalMeshComponent* AttachTo)
{
	WeaponSMComponent->SetStaticMesh(WeaponStaticMesh);
	WeaponSKComponent->SetSkeletalMesh(WeaponSkeletalMesh);
	AttachToHolsterSocket(AttachTo);
}


void UEGWeaponComponentCore::AttachToHolsterSocket(USkeletalMeshComponent* AttachTo)
{
	if (AttachTo)
		this->AttachToComponent(AttachTo ,AttachmentRules, WeaponHolsterAttachmentSocket);
	else if (OwnerSK)
		this->AttachToComponent(OwnerSK ,AttachmentRules, WeaponHolsterAttachmentSocket);
	
}

void UEGWeaponComponentCore::AttachToEquipSlot(USkeletalMeshComponent* AttachTo)
{
	if (AttachTo)
		this->AttachToComponent(AttachTo , AttachmentRules, WeaponEquipAttachmentSocket);
	else if (OwnerSK)
		this->AttachToComponent(OwnerSK , AttachmentRules, WeaponEquipAttachmentSocket);
}






void UEGWeaponComponentCore::EquipWeapon()
{
	if (EquipMontage && OwnerSK && WeaponEquipState == UnEquipped)
	{
		bEquipUseNotify ?
			OwnerSK->GetAnimInstance()->OnPlayMontageNotifyBegin.AddDynamic(this,&UEGWeaponComponentCore::OnWeaponMontageNotifyBegin) :
			OwnerSK->GetAnimInstance()->OnMontageBlendingOut.AddDynamic(this,&UEGWeaponComponentCore::OnWeaponMontageBlendOut);
		
		OwnerSK->GetAnimInstance()->Montage_Play(EquipMontage);
		WeaponEquipState = Equipping;
	}
}

void UEGWeaponComponentCore::UnEquipWeapon()
{
	if (UnEquipMontage && OwnerSK && WeaponEquipState == Equipped)
	{
		bUnEquipUseNotify ?
			OwnerSK->GetAnimInstance()->OnPlayMontageNotifyBegin.AddDynamic(this,&UEGWeaponComponentCore::OnWeaponMontageNotifyBegin) :
			OwnerSK->GetAnimInstance()->OnMontageBlendingOut.AddDynamic(this,&UEGWeaponComponentCore::OnWeaponMontageBlendOut);
		
		OwnerSK->GetAnimInstance()->Montage_Play(UnEquipMontage);
		WeaponEquipState = UnEquipping;
		
	}
}

void UEGWeaponComponentCore::ChangeEquipState()
{
	if (WeaponEquipState == Equipped) UnEquipWeapon();
	if (WeaponEquipState == UnEquipped) EquipWeapon();
}


void UEGWeaponComponentCore::OnWeaponMontageBlendOut(UAnimMontage* Montage, bool bInterrupted)
{
	
	if (WeaponEquipState == Equipping)
	{
		AttachToEquipSlot();
		WeaponEquipState = Equipped;
	}
	
	if (WeaponEquipState == UnEquipping)
	{
		AttachToHolsterSocket();
		WeaponEquipState = UnEquipped;
	}
	if (OwnerSK)
		OwnerSK->GetAnimInstance()->OnMontageBlendingOut.RemoveDynamic(this,&UEGWeaponComponentCore::OnWeaponMontageBlendOut);
	
}

void UEGWeaponComponentCore::OnWeaponMontageNotifyBegin(FName NotifyName,const FBranchingPointNotifyPayload& BranchingPointPayload)
{
	if (WeaponEquipState == Equipping)
	{
		if (EquipMontageNotify == FName())
		{
			AttachToEquipSlot();
			WeaponEquipState = Equipped;
		}
		else if (EquipMontageNotify == NotifyName)
		{
			AttachToEquipSlot();
			WeaponEquipState = Equipped;
		}
	}
	
	if (WeaponEquipState == UnEquipping)
	{
		if (UnEquipMontageNotify == FName())
		{
			AttachToHolsterSocket();
			WeaponEquipState = UnEquipped;
		}
		else if (UnEquipMontageNotify == NotifyName)
		{
			AttachToHolsterSocket();
			WeaponEquipState = UnEquipped;
		}
	}
	
	if (OwnerSK)
		OwnerSK->GetAnimInstance()->OnPlayMontageNotifyBegin.RemoveDynamic(this,&UEGWeaponComponentCore::OnWeaponMontageNotifyBegin);
}













void UEGWeaponComponentCore::BeginPlay()
{
	Super::BeginPlay();
	if (const auto skeletal = Cast<USkeletalMeshComponent>(GetAttachParent()))
		OwnerSK = skeletal;
	else
		UE_LOG(LogBlueprint , Error , TEXT("Weapon Component Is Not Attached To A Skeletal Mesh"))
}