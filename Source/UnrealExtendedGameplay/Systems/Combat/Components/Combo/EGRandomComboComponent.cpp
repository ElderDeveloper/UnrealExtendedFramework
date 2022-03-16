// Fill out your copyright notice in the Description page of Project Settings.


#include "EGRandomComboComponent.h"

#include "UnrealExtendedGameplay/Animation/Notifiers/Combo/EGComboComponentNotify.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetMathLibrary.h"
#include "UnrealExtendedFramework/Data/EFMacro.h"


// Sets default values for this component's properties
UEGRandomComboComponent::UEGRandomComboComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}




void UEGRandomComboComponent::ExecuteCombo()
{
	if (bCanExecuteNextCombo && IsValid(ComboSkeleton) && !bExecuteComboBlocked)
	{
		switch (RandomComboExecuteType)
		{
			case Random: RandomCombo(); break;
			case InOrder: InOrderCombo(); break;
			default:break;;
		}
	}
}




void UEGRandomComboComponent::ExecuteComboWithBoolCheck(bool CanExecuteCombo)
{
	if (CanExecuteCombo)
		ExecuteCombo();
}




void UEGRandomComboComponent::RandomCombo()
{
	int32 i = UKismetMathLibrary::RandomIntegerInRange(0,ComboMontages.Num()-1);
	i == CurrentAttackIndex ?  i == 0 ? i++ : i-- :  i ;
	CurrentAttackIndex = i ;
	PlayMontage(ComboMontages[i]);
}




void UEGRandomComboComponent::InOrderCombo()
{
	if (ComboMontages.IsValidIndex(CurrentAttackIndex))
	{
		PlayMontage(ComboMontages[CurrentAttackIndex]);
		CurrentAttackIndex++;
		if (CurrentAttackIndex > ComboMontages.Num()-1)
			CurrentAttackIndex = 0;
	}
}




void UEGRandomComboComponent::PlayMontage(UAnimMontage* Montage)
{
	bCanExecuteNextCombo = false;
	
	// if (n.NotifyName == FName("EGComboComponentNotify"))

	if(!ComboSkeleton->GetAnimInstance()->OnMontageBlendingOut.IsAlreadyBound(this,&UEGRandomComboComponent::OnMontageBlendOut))
		ComboSkeleton->GetAnimInstance()->OnMontageBlendingOut.AddDynamic(this ,&UEGRandomComboComponent::OnMontageBlendOut);
	ComboSkeleton->GetAnimInstance()->Montage_Play(Montage,1,EMontagePlayReturnType::MontageLength,0,true);
	
}




void UEGRandomComboComponent::OnMontageBlendOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (!bInterrupted)
	{
		bCanExecuteNextCombo = true;
		PRINT_STRING(2,Cyan,"Montage Interrupted");
		ComboSkeleton->GetAnimInstance()->OnMontageBlendingOut.RemoveDynamic(this,&UEGRandomComboComponent::OnMontageBlendOut);
	}
}




// Called when the game starts
void UEGRandomComboComponent::BeginPlay()
{
	Super::BeginPlay();
	if (const auto Character = Cast<ACharacter>(GetOwner()))
		ComboSkeleton = Character->GetMesh();

	if (!ComboMontages.IsValidIndex(0))
	{
		UE_LOG(LogTemp,Error,TEXT("EGComboComponent Has No Montages Assigned at %s") , *GetOwner()->GetName());
		DestroyComponent();
	}
}



void UEGRandomComboComponent::SetupComponent(USkeletalMeshComponent* MeshComponent)
{
	ComboSkeleton = MeshComponent;
}



void UEGRandomComboComponent::SetEnableNextCombo()
{
	bCanExecuteNextCombo = true;
}



void UEGRandomComboComponent::SetExecuteComboBlocked(bool IsBlocked)
{
	bExecuteComboBlocked = IsBlocked;
}



void UEGRandomComboComponent::SetExecuteComboBlockWithDelay(float DelayTime)
{
	bExecuteComboBlocked = true;
	const FTimerDelegate DelayDelegate = FTimerDelegate::CreateUObject(this ,&UEGRandomComboComponent::SetExecuteComboBlocked,false);
	GetWorld()->GetTimerManager().SetTimer(ComboBlockTimer , DelayDelegate , DelayTime ,false );
}