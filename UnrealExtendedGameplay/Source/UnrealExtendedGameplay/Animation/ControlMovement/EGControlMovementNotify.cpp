// Fill out your copyright notice in the Description page of Project Settings.


#include "EGControlMovementNotify.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"


#if ENGINE_MAJOR_VERSION != 5
void UEGControlMovementNotify::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,float TotalDuration)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration);

	if (const auto character = Cast<ACharacter>(MeshComp->GetOwner()))
	{
		if (const auto movement = Cast<UCharacterMovementComponent>(character->GetCharacterMovement()))
		{
			DefaultAcceleration = movement->MaxAcceleration;
			movement->MaxAcceleration = 0;
		}
	}
}

void UEGControlMovementNotify::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation)
{
	Super::NotifyEnd(MeshComp, Animation);

	if (const auto character = Cast<ACharacter>(MeshComp->GetOwner()))
	{
		if (const auto movement = Cast<UCharacterMovementComponent>(character->GetCharacterMovement()))
			movement->MaxAcceleration = DefaultAcceleration;
	}
}

#endif




#if ENGINE_MAJOR_VERSION == 5
void UEGControlMovementNotify::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (const auto character = Cast<ACharacter>(MeshComp->GetOwner()))
	{
		if (const auto movement = Cast<UCharacterMovementComponent>(character->GetCharacterMovement()))
		{
			DefaultAcceleration = movement->MaxAcceleration;
			movement->MaxAcceleration = 0;
		}
	}
}

void UEGControlMovementNotify::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	if (const auto character = Cast<ACharacter>(MeshComp->GetOwner()))
	{
		if (const auto movement = Cast<UCharacterMovementComponent>(character->GetCharacterMovement()))
			movement->MaxAcceleration = DefaultAcceleration;
	}
}

#endif