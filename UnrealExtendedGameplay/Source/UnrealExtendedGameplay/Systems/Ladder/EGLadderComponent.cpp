// Fill out your copyright notice in the Description page of Project Settings.


#include "EGLadderComponent.h"

#include "EGLadder.h"
#include "Components/ArrowComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "UnrealExtendedFramework/Libraries/Trace/EFTraceLibrary.h"


// Sets default values for this component's properties
UEGLadderComponent::UEGLadderComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}





void UEGLadderComponent::ProcessLadderClimb(const float Axis, const float DeltaTime)
{
	if (Player && IsInLadder && !IsInMontage)
	{
		CheckLadderGroundDistance();

	
		if(IsReadyForFootLeaveLadder && Axis < 0)
		{
			FootLeaveLadder();
			return;
		}
		
		
		const float Plus = Axis > 0 ? LadderTraceUpPlus : LadderTraceDownPlus;
		
		const FVector TargetLocation = Player->GetActorLocation() + FVector(0 , 0 , Plus);
		
		FSphereTraceStruct MoveLocationCheck;
		MoveLocationCheck.Start = TargetLocation ;
		MoveLocationCheck.End = TargetLocation + Player->GetActorForwardVector() * 200;
		MoveLocationCheck.Radius = 18;
		MoveLocationCheck.TraceChannel = GroundTraceStruct.TraceChannel;
		MoveLocationCheck.DrawDebugType = GroundTraceStruct.DrawDebugType;

		const FVector MoveDirection = Player->GetActorLocation() + FVector(0 , 0 , LadderSpeed * Axis * DeltaTime);
		
		if (UEFTraceLibrary::ExtendedSphereTraceSingle(GetWorld(),MoveLocationCheck))
		{
			Player->SetActorLocation(MoveDirection);
		}
		else
		{
			ClimbLeaveLadder();
		}
	}
}





void UEGLadderComponent::TryEnterLadder()
{
	if (!IsInLadder && ! IsEnteringLadder && IsReadyForEnterLadder)
	{
		IsEnteringLadder = true;
		
		if (EnterLadderMontage)
		{
			IsEnterMontageValid = true;
			IsInMontage = true;
			Player->GetMesh()->GetAnimInstance()->OnMontageBlendingOut.AddDynamic(this, &UEGLadderComponent::OnEnterLadderMontageBlendOut);
			Player->GetMesh()->GetAnimInstance()->Montage_Play(EnterLadderMontage);
		}
		else
			IsEnterMontageValid = false;
		
		if (Player)
		{
			Player->GetCharacterMovement()->MovementMode = MOVE_Flying;
		}
	}
}




void UEGLadderComponent::OnEnterLadderMontageBlendOut(UAnimMontage* Montage, bool bInterrupted)
{
	IsEnteringLadder = false;
	IsInLadder = true;
	IsInMontage = false;
	
	Player->GetCharacterMovement()->SetComponentTickEnabled(false);
	
	Player->GetMesh()->GetAnimInstance()->OnMontageBlendingOut.RemoveDynamic(this, &UEGLadderComponent::OnEnterLadderMontageBlendOut);
	
	const FVector TargetLocation = Ladder->LadderEntryArrow->GetComponentLocation();
	const FRotator TargetRotation =  Ladder->LadderEntryArrow->GetComponentRotation();
	
	Player->SetActorLocation(FVector(TargetLocation.X,TargetLocation.Y,Player->GetActorLocation().Z));
	Player->SetActorRotation(FRotator(0,TargetRotation.Yaw,0));
	
	OnLadderStateChanged.Broadcast(true);
}




void UEGLadderComponent::PlayMontageIfValid(UAnimMontage* MontageToPlay  , bool InLadderState , bool SetStateOnBegin)
{
	if (MontageToPlay && PlayerMesh)
	{
		PlayerMesh->GetAnimInstance()->OnMontageStarted.AddDynamic(this,&UEGLadderComponent::OnMontageBegin);
		PlayerMesh->GetAnimInstance()->OnMontageBlendingOut.AddDynamic(this,&UEGLadderComponent::OnMontageEnded);
		PlayerMesh->GetAnimInstance()->Montage_Play(MontageToPlay);
	}
	if (SetStateOnBegin)
	{
		IsInLadder = InLadderState;
		OnLadderStateChanged.Broadcast(true);
	}
		
	else
	{
		IsWaitingMontageToEnd = true;
		MontageEndLadderState = InLadderState;
	}
}





void UEGLadderComponent::OnMontageBegin(UAnimMontage* MontageToPlay)
{
	IsInMontage = true;
}





void UEGLadderComponent::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	IsInMontage = false;
	if (PlayerMesh)
	{
		PlayerMesh->GetAnimInstance()->OnMontageStarted.RemoveDynamic(this,&UEGLadderComponent::OnMontageBegin);
		PlayerMesh->GetAnimInstance()->OnMontageBlendingOut.RemoveDynamic(this,&UEGLadderComponent::OnMontageEnded);

		if (IsWaitingMontageToEnd)
		{
			IsInLadder = MontageEndLadderState;
			IsEnteringLadder = false;
			Player->GetCharacterMovement()->SetComponentTickEnabled(true);
			Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			OnLadderStateChanged.Broadcast(MontageEndLadderState);
		}
			
	}
}





void UEGLadderComponent::CheckLadderGroundDistance()
{
	GroundTraceStruct.Start = Player->GetActorLocation();
	GroundTraceStruct.End = Player->GetActorLocation() + FVector(0,0,-50000);
	
	if (UEFTraceLibrary::ExtendedLineTraceSingle(GetWorld(),GroundTraceStruct))
		DistanceToGround = GroundTraceStruct.GetHitDistance();
		DistanceToGround < Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 10 ? SetIsReadyForFootLeaveLadder(true) : SetIsReadyForFootLeaveLadder(false);
}






void UEGLadderComponent::BeginPlay()
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





void UEGLadderComponent::TickComponent(float DeltaTime, ELevelTick Tick,FActorComponentTickFunction* ThisTickFunction)
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






void UEGLadderComponent::InterpPlayerToLadderAndStartClimb(const float DeltaTime)
{
	const FVector TargetLocation = UKismetMathLibrary::VInterpTo(Player->GetActorLocation() , Ladder->LadderEntryArrow->GetComponentLocation() , DeltaTime , LadderEnterInterpSpeed);
	const FRotator TargetRotation = UKismetMathLibrary::RInterpTo(Player->GetActorRotation() , Ladder->LadderEntryArrow->GetComponentRotation() , DeltaTime , LadderEnterInterpSpeed);
	Player->SetActorLocation(FVector(TargetLocation.X,TargetLocation.Y,Player->GetActorLocation().Z));
	Player->SetActorRotation(FRotator(0,TargetRotation.Yaw,0));

	if (Player->GetActorLocation().X == TargetLocation.X && Player->GetActorLocation().Y == TargetLocation.Y && Player->GetActorRotation().Yaw == TargetRotation.Yaw)
	{
		IsEnteringLadder = false;
		IsInLadder = true;
		OnLadderStateChanged.Broadcast(true);
	}
}




void UEGLadderComponent::InterpPlayerToLadderAndWaitMontage(const float DeltaTime)
{
	const FVector TargetLocation = UKismetMathLibrary::VInterpTo(Player->GetActorLocation() , Ladder->LadderEntryArrow->GetComponentLocation() , DeltaTime , LadderEnterInterpSpeed);
	const FRotator TargetRotation = UKismetMathLibrary::RInterpTo(Player->GetActorRotation() , Ladder->LadderEntryArrow->GetComponentRotation() , DeltaTime , LadderEnterInterpSpeed);
	Player->SetActorLocation(FVector(TargetLocation.X,TargetLocation.Y,Player->GetActorLocation().Z));
	Player->SetActorRotation(FRotator(0,TargetRotation.Yaw,0));
}



void UEGLadderComponent::FootLeaveLadder()
{
	if (LeaveLadderFootMontage)
	{
		IsLeavingLadder = true;
		IsInMontage = true;
		Player->GetMesh()->GetAnimInstance()->OnMontageBlendingOut.AddDynamic(this,&UEGLadderComponent::OnLeaveLadderMontageBlendOut);
		Player->GetMesh()->GetAnimInstance()->Montage_Play(LeaveLadderFootMontage);
	}
	else
	{
		IsLeavingLadder = false;
		IsInLadder = false;
		IsEnteringLadder = false;
		IsInMontage = false;

		Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Player->GetCharacterMovement()->SetComponentTickEnabled(true);

		OnLadderStateChanged.Broadcast(false);
	}
}

void UEGLadderComponent::OnLeaveLadderMontageBlendOut(UAnimMontage* Montage, bool bInterrupted)
{
	Player->GetMesh()->GetAnimInstance()->OnMontageBlendingOut.RemoveDynamic(this,&UEGLadderComponent::OnLeaveLadderMontageBlendOut);
	IsLeavingLadder = false;
	IsInLadder = false;
	IsEnteringLadder = false;
	IsInMontage = false;

	Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	Player->GetCharacterMovement()->SetComponentTickEnabled(true);

	OnLadderStateChanged.Broadcast(false);
}


void UEGLadderComponent::ClimbLeaveLadder()
{
	if (LeaveLadderClimbMontage)
	{
		IsClimbLeaveLadder = true;
		IsInMontage = true;
		Player->GetCharacterMovement()->SetComponentTickEnabled(true);
		Player->GetMesh()->GetAnimInstance()->OnMontageBlendingOut.AddDynamic(this,&UEGLadderComponent::OnLeaveLadderClimbMontageBlendOut);
		Player->GetMesh()->GetAnimInstance()->Montage_Play(LeaveLadderClimbMontage);
	}
	else
	{
		IsClimbLeaveLadder = false;
		IsInLadder = false;
		IsEnteringLadder = false;
		IsInMontage = false;

		Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Player->GetCharacterMovement()->SetComponentTickEnabled(true);

		OnLadderStateChanged.Broadcast(false);
	}
}


void UEGLadderComponent::OnLeaveLadderClimbMontageBlendOut(UAnimMontage* Montage, bool bInterrupted)
{
	Player->GetMesh()->GetAnimInstance()->OnMontageBlendingOut.RemoveDynamic(this,&UEGLadderComponent::OnLeaveLadderClimbMontageBlendOut);
	IsClimbLeaveLadder = false;
	IsInLadder = false;
	IsEnteringLadder = false;
	IsInMontage = false;

	Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);

	OnLadderStateChanged.Broadcast(false);

}
