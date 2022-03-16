// Fill out your copyright notice in the Description page of Project Settings.


#include "EFBTServiceInterpCharacterSpeed.h"

#include "AIController.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"

void UEFBTServiceInterpCharacterSpeed::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory,float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	if (OwnerComp.GetOwner())
	{
		if (const auto AIController = Cast<AAIController>(OwnerComp.GetOwner()))
		{
			if (const auto Character = Cast<ACharacter>(AIController->GetPawn()))
			{
				const float Speed =  UKismetMathLibrary::FInterpTo(Character->GetCharacterMovement()->MaxWalkSpeed , TargetSpeed , DeltaSeconds , InterpSpeed );
				Character->GetCharacterMovement()->MaxWalkSpeed = Speed;
			}
		}
	}
}

FString UEFBTServiceInterpCharacterSpeed::GetStaticServiceDescription() const
{
	//"\n"
	const FString Return =  FString::Printf(TEXT(" \nInterp Character Speed To: %s , Interp Speed: %s \n Tick:%s , Deviation: %s ") , *FString::SanitizeFloat(TargetSpeed) , *FString::SanitizeFloat(InterpSpeed) ,*FString::SanitizeFloat(Interval) , *FString::SanitizeFloat(RandomDeviation));
	//"Interp Character Speed To: " + FString::SanitizeFloat(TargetSpeed) + " ,Interp Speed: " + FString::SanitizeFloat(InterpSpeed) + " Tick: " + FString::SanitizeFloat(Interval) + " Deviation: " + FString::SanitizeFloat(RandomDeviation);
	return Return;
}
