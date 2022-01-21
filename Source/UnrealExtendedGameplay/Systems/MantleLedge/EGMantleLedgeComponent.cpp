// Fill out your copyright notice in the Description page of Project Settings.


#include "EGMantleLedgeComponent.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "UnrealExtendedFramework/Data/EFTraceData.h"
#include "UnrealExtendedFramework/Libraries/Trace/EFTraceLibrary.h"


// Sets default values for this component's properties
UEGMantleLedgeComponent::UEGMantleLedgeComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
}




// Called when the game starts
void UEGMantleLedgeComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const auto character = Cast<ACharacter>(GetOwner()))
	{
		OwnerCharacter = character;
		OwnerMovement = character->GetCharacterMovement();

		if (!OwnerMovement)
		{
			DestroyComponent();
		}
	}
	else
	{
		DestroyComponent();
	}
	
}


// Called every frame
void UEGMantleLedgeComponent::TickComponent(float DeltaTime, ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (OwnerMovement)
	{
		if (OwnerMovement->IsFalling() && !Mantling && CanMantle)
		{
			CheckMantleState();
		}
	}

}

FVector UEGMantleLedgeComponent::GetCapsuleBaseLocation(float ZOffset) const
{
	if (const auto capsule = OwnerCharacter->GetCapsuleComponent() )
	{
		return capsule->GetComponentLocation()	 - 	(capsule->GetUpVector()	*	(capsule->GetScaledCapsuleHalfHeight()+ZOffset)); 
	}
	return FVector::ZeroVector;
}

FVector UEGMantleLedgeComponent::GetCapsuleLocationFromBase(FVector BaseLocation, float ZOffset) const
{
	return BaseLocation + FVector(0 , 0 , OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+ZOffset);
}


FVector UEGMantleLedgeComponent::GetPlayerMovementForward() const
{return UKismetMathLibrary::GetForwardVector(FRotator(0,OwnerCharacter->GetControlRotation().Yaw,0));}
FVector UEGMantleLedgeComponent::GetPlayerMovementRight() const
{return UKismetMathLibrary::GetRightVector(FRotator(0,OwnerCharacter->GetControlRotation().Yaw,0));}
FVector UEGMantleLedgeComponent::GetPlayerMovementInput() const
{return UKismetMathLibrary::Normal(GetPlayerMovementForward()+GetPlayerMovementRight());}

UAnimMontage* UEGMantleLedgeComponent::GetMantleMontage(EMantleType MantleType) const
{
	switch (MantleType)
	{
	case HighMantle:
		return HighMantleMontage;

	case LowMantle:
		return LowMantleMontage;

	case FallingCatch:
		return FallingCatchMontage;
		default: return nullptr;
	}
}


bool UEGMantleLedgeComponent::CheckMantleState()
{
	FVector InitialTraceImpactPoint;
	FVector InitialTraceNormal;

	FCapsuleTraceStruct TraceForward;
	TraceForward.Start = (GetCapsuleBaseLocation(2)+(GetPlayerMovementInput()*-30)) + FVector(0,0,(MaxLedgeHeight+MinLedgeHeight)/2);
	TraceForward.End = TraceForward.Start + GetPlayerMovementInput()*ReachDistance;
	TraceForward.Radius = ForwardTraceRadius;
	TraceForward.HalfHeight = (MaxLedgeHeight-MinLedgeHeight)/2+1;
	TraceForward.TraceType=ETraceTypes::TraceType;
	TraceForward.TraceChannel = TraceChannel;
	TraceForward.DrawDebugType = DrawDebugType;
	//OwnerMovement->IsWalkable(TraceForward.HitResult) &&
	UEFTraceLibrary::ExtendedCapsuleTraceSingle(GetWorld(),TraceForward);
	if ( TraceForward.HitResult.bBlockingHit)
	{
		InitialTraceImpactPoint = TraceForward.HitResult.ImpactPoint;
		InitialTraceNormal = TraceForward.HitResult.ImpactNormal;
		GEngine->AddOnScreenDebugMessage(-1 , 2.f , FColor::Green , "First Pass");
	}
	else return false;
	


	FVector DownTraceLocation;
	UPrimitiveComponent* HitComponent;
	
	FSphereTraceStruct TraceDownward;
	TraceDownward.End =FVector(InitialTraceImpactPoint.X , InitialTraceImpactPoint.Y , GetCapsuleBaseLocation(2).Z) + InitialTraceNormal* -15;
	TraceDownward.Start = TraceDownward.End + FVector(0 , 0 , MaxLedgeHeight + DownwardTraceRadius + 1) ;
	TraceDownward.Radius=DownwardTraceRadius;

	UEFTraceLibrary::ExtendedSphereTraceSingle(GetWorld(),TraceDownward);
	
	if (OwnerMovement->IsWalkable(TraceDownward.HitResult) && TraceDownward.HitResult.bBlockingHit)
	{
		DownTraceLocation = FVector(TraceDownward.HitResult.Location.X ,TraceDownward.HitResult.Location.Y , TraceDownward.HitResult.ImpactPoint.Z );
		HitComponent = TraceDownward.HitResult.Component.Get();
		GEngine->AddOnScreenDebugMessage(-1 , 2.f , FColor::Green , "Second Pass");
	}
	else return false;


	
	FSphereTraceStruct TraceCapsule;
	TraceCapsule.Start = GetCapsuleLocationFromBase(DownTraceLocation, 2) + OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight_WithoutHemisphere();
	TraceCapsule.End = GetCapsuleLocationFromBase(DownTraceLocation, 2) - OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight_WithoutHemisphere();
	TraceCapsule.Radius = OwnerCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius();
	TraceCapsule.TraceType=ETraceTypes::ProfileType;
	TraceCapsule.TraceProfileName = TraceProfileName;
	TraceCapsule.DrawDebugType = DrawDebugType;
	

	UEFTraceLibrary::ExtendedSphereTraceSingle(GetWorld(),TraceCapsule);
	
	
	
	float MantleHeight = FVector(GetCapsuleLocationFromBase(DownTraceLocation,2) -  OwnerCharacter->GetActorLocation()).Z;

	
	FTransform TargetTransform;
	/*
	if (UKismetMathLibrary::BooleanNOR(TraceCapsule.HitResult.bBlockingHit , TraceCapsule.HitResult.bStartPenetrating))
	{
		TargetTransform = FTransform(
			UKismetMathLibrary::Conv_VectorToRotator(InitialTraceNormal*FVector(-1,-1,0)) ,
			GetCapsuleLocationFromBase(DownTraceLocation,2) ,
			FVector(0,0,0));
		
	}	else return false;

	*/

	
	EMantleType MantleType = MantleHeight > 125 ? HighMantle : LowMantle;


	
	//StartMantle(MantleHeight , TargetTransform , HitComponent , MantleType)
	StartMantle(MantleHeight,MantleType);
	GEngine->AddOnScreenDebugMessage(-1 , 2.f , FColor::Green , "Mantle");

	
	return true;
}

void UEGMantleLedgeComponent::StartMantle(float MantleHeight, EMantleType MantleType)
{
	OwnerMovement->SetMovementMode(EMovementMode::MOVE_Flying);
	
	if(const auto Montage = GetMantleMontage(MantleType))
	{
		Mantling = true;
		OwnerCharacter->GetMesh()->GetAnimInstance()->OnMontageBlendingOut.AddDynamic(this,&UEGMantleLedgeComponent::OnMontageBlendOut);
		OwnerCharacter->GetMesh()->GetAnimInstance()->Montage_Play(Montage);
	}

	
}

void UEGMantleLedgeComponent::OnMontageBlendOut(UAnimMontage* Montage, bool bInterrupted)
{
	OwnerMovement->SetMovementMode(MOVE_Walking);
	OwnerCharacter->GetMesh()->GetAnimInstance()->OnMontageBlendingOut.RemoveDynamic(this,&UEGMantleLedgeComponent::OnMontageBlendOut);
	Mantling = false;
}