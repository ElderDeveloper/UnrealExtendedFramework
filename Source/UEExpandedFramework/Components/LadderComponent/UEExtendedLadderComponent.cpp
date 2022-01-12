// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedLadderComponent.h"

#include "UEExtendedLadder.h"
#include "Components/ArrowComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetMathLibrary.h"
#include "UEExpandedFramework/Gameplay/Trace/UEExtendedTraceData.h"
#include "UEExpandedFramework/Gameplay/Trace/UEExtendedTraceLibrary.h"


// Sets default values for this component's properties
UUEExtendedLadderComponent::UUEExtendedLadderComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}





void UUEExtendedLadderComponent::ProcessLadderClimb(const float Axis, const float DeltaTime)
{
	if (Player && IsInLadder && !IsInMontage)
	{
		CheckLadderGroundDistance();

		if(IsReadyForFootLeaveLadder && Axis < 0)
		{
			PlayMontageIfValid(LeaveLadderFootMontage,false,true);
			return;
		}

		if (IsReadyForClimbLeaveLadder && Axis > 0)
		{
			PlayMontageIfValid(LeaveLadderClimbMontage , false , true);
			return;
		}
		
		const FVector TargetLocation = Player->GetActorLocation() + FVector(0 , 0 , LadderSpeed * Axis * DeltaTime);
		
		FLineTraceStruct MoveLocationCheck;
		MoveLocationCheck.Start = TargetLocation;
		MoveLocationCheck.End = TargetLocation + Player->GetActorForwardVector() * 200;
		MoveLocationCheck.TraceChannel = GroundTraceStruct.TraceChannel;
		MoveLocationCheck.DrawDebugType = GroundTraceStruct.DrawDebugType;
		
		if (UUEExtendedTraceLibrary::ExtendedLineTraceSingle(GetWorld(),MoveLocationCheck))
		{
			Player->SetActorLocation(TargetLocation);
		}
	}
}





void UUEExtendedLadderComponent::TryEnterLadder()
{
	IsEnteringLadder = true;
	EnterLadderMontage ? IsEnterMontageValid = true : IsEnterMontageValid = false;
	PlayMontageIfValid(EnterLadderMontage);
}





void UUEExtendedLadderComponent::PlayMontageIfValid(UAnimMontage* MontageToPlay  , bool InLadderState , bool SetStateOnBegin)
{
	if (MontageToPlay && PlayerMesh)
	{
		PlayerMesh->GetAnimInstance()->OnMontageStarted.AddDynamic(this,&UUEExtendedLadderComponent::OnMontageBegin);
		PlayerMesh->GetAnimInstance()->OnMontageBlendingOut.AddDynamic(this,&UUEExtendedLadderComponent::OnMontageEnded);
		PlayerMesh->GetAnimInstance()->Montage_Play(MontageToPlay);
		
		if (SetStateOnBegin)
			IsInLadder = InLadderState;
		else
		{
			IsWaitingMontageToEnd = true;
			MontageEndLadderState = InLadderState;
		}
			
	}
}





void UUEExtendedLadderComponent::OnMontageBegin(UAnimMontage* MontageToPlay)
{
	IsInMontage = true;
}





void UUEExtendedLadderComponent::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	IsInMontage = false;
	if (PlayerMesh)
	{
		PlayerMesh->GetAnimInstance()->OnMontageStarted.RemoveDynamic(this,&UUEExtendedLadderComponent::OnMontageBegin);
		PlayerMesh->GetAnimInstance()->OnMontageBlendingOut.RemoveDynamic(this,&UUEExtendedLadderComponent::OnMontageEnded);

		if (IsWaitingMontageToEnd)
			IsInLadder = MontageEndLadderState;
	}
}





void UUEExtendedLadderComponent::CheckLadderGroundDistance()
{
	GroundTraceStruct.Start = Player->GetActorLocation();
	GroundTraceStruct.End = Player->GetActorLocation() + FVector(0,0,50000);
	
	if (UUEExtendedTraceLibrary::ExtendedLineTraceSingle(GetWorld(),GroundTraceStruct))
		GroundTraceStruct.GetHitDistance() < Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 10 ? SetIsReadyForFootLeaveLadder(true) : SetIsReadyForFootLeaveLadder(false);
}









void UUEExtendedLadderComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const auto Character = Cast<ACharacter>(GetOwner()))
	{
		Player = Character;
		PlayerMesh = Character->GetMesh();
	}
	else
	{
		DestroyComponent();
		UE_LOG(LogBlueprint,Error,TEXT("Ladder Component Must Be Putted In To A ACharacter , Component Destroyed"));
	}
}





void UUEExtendedLadderComponent::TickComponent(float DeltaTime, ELevelTick Tick,FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, Tick, ThisTickFunction);
	
	if (IsEnteringLadder)
	{
		if (IsEnterMontageValid)
			InterpPlayerToLadderAndWaitMontage(DeltaTime);
		else
			InterpPlayerToLadderAndStartClimb(DeltaTime);
	}
}








void UUEExtendedLadderComponent::InterpPlayerToLadderAndStartClimb(const float DeltaTime)
{
	const FVector TargetLocation = UKismetMathLibrary::VInterpTo(Player->GetActorLocation() , Ladder->LadderEntryArrow->GetComponentLocation() , DeltaTime , LadderEnterInterpSpeed);
	const FRotator TargetRotation = UKismetMathLibrary::RInterpTo(Player->GetActorRotation() , Ladder->LadderEntryArrow->GetComponentRotation() , DeltaTime , LadderEnterInterpSpeed);
	Player->SetActorLocation(FVector(TargetLocation.X,TargetLocation.Y,Player->GetActorLocation().Z));
	Player->SetActorRotation(FRotator(0,TargetRotation.Yaw,0));

	if (Player->GetActorLocation().X == TargetLocation.X && Player->GetActorLocation().Y == TargetLocation.Y && Player->GetActorRotation().Yaw == TargetRotation.Yaw)
	{
		IsEnteringLadder = false;
		IsInLadder = true;
	}
}




void UUEExtendedLadderComponent::InterpPlayerToLadderAndWaitMontage(const float DeltaTime)
{
	const FVector TargetLocation = UKismetMathLibrary::VInterpTo(Player->GetActorLocation() , Ladder->LadderEntryArrow->GetComponentLocation() , DeltaTime , LadderEnterInterpSpeed);
	const FRotator TargetRotation = UKismetMathLibrary::RInterpTo(Player->GetActorRotation() , Ladder->LadderEntryArrow->GetComponentRotation() , DeltaTime , LadderEnterInterpSpeed);
	Player->SetActorLocation(FVector(TargetLocation.X,TargetLocation.Y,Player->GetActorLocation().Z));
	Player->SetActorRotation(FRotator(0,TargetRotation.Yaw,0));
}