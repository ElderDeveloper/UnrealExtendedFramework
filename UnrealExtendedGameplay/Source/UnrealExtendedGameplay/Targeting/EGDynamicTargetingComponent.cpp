// Fill out your copyright notice in the Description page of Project Settings.


#include "EGDynamicTargetingComponent.h"
#include "EGTargetingTargetComponent.h"
#include "EngineUtils.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EGTargetingWidget.h"
#include "Kismet/KismetMathLibrary.h"
#include "UnrealExtendedFramework/Data/EFTraceData.h"
#include "UnrealExtendedFramework/Libraries/Math/EFMathLibrary.h"
#include "UnrealExtendedFramework/Libraries/Trace/EFTraceLibrary.h"
#include "Kismet/GameplayStatics.h"


DEFINE_LOG_CATEGORY(LogTargeting);


UEGDynamicTargetingComponent::UEGDynamicTargetingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	TargetActorSearchSphere.Radius = MaximumDistanceToEnable;
}





void UEGDynamicTargetingComponent::BeginPlay()
{
	Super::BeginPlay();
	OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		UE_LOG(LogBlueprint,Error, TEXT("[%s] TargetSystemComponent: Cannot get Owner reference ..."), *GetName());	return;
	}

	OwnerPawn = Cast<APawn>(OwnerActor);
	if (!ensure(OwnerPawn))
	{
		UE_LOG(LogBlueprint,Error, TEXT("[%s] TargetSystemComponent: Component is meant to be added to Pawn only ..."), *GetName());
		return;
	}

	SetupLocalPlayerController();

	TargetActorSearchSphere.Radius = MaximumDistanceToEnable;
}





void UEGDynamicTargetingComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	
	if (TargetedActor)
	{
		SetControlRotationOnTarget(TargetedActor);
		
		// Target Locked Off based on Distance
		if (UEFMathLibrary::GetDistanceBetweenActors(OwnerActor , TargetedActor) > MaximumDistanceToEnable)
		{
			TargetingLog(TEXT("Target Break Distance"));
			TargetLockOff();
		}

		if (CheckShouldBreakLineOfSight(DeltaTime))
		{
			TargetLockOff();
			TargetingLog(TEXT("Target Break Line Of Sight"));
		}
	}
	else if(OwnerPlayerController)
	{
		CheckActorInFocus();
	}
	if (!IsValid(TargetedActor) && IsTargetEnabled)
	{
		TargetingLog(TEXT("Target Break Target Actor Not Valid"));
		TargetLockOff();
		IsTargetEnabled = false;
	}
}
	


void UEGDynamicTargetingComponent::CheckActorInFocus()
{
	if (OwnerPlayerController->PlayerCameraManager)
	{
		TargetActorSearchSphere.Start = OwnerPlayerController->PlayerCameraManager->GetCameraLocation();
		TargetActorSearchSphere.End = TargetActorSearchSphere.Start + OwnerPlayerController->PlayerCameraManager->GetCameraRotation().Vector() * 9000;
			
		if(UEFTraceLibrary::ExtendedSphereTraceMulti(GetWorld(),TargetActorSearchSphere))
		{
			TArray<AActor*> Actors;
			TargetActorSearchSphere.GetAllActorsFromHitArray(Actors);
			
			if(const auto actor = FindNearestTarget(Actors))
			{
				if (LocalTargetedActor == actor)
				{
					CurrentActorTargetingDelay -= GetWorld()->GetDeltaSeconds();
					
					if (CurrentActorTargetingDelay <= 0 )
					{
						TargetLockOn(actor);
						TargetingLog(TEXT("Target Locked On"));
						LocalTargetedActor = nullptr;
					}
				}
				else
				{
					LocalTargetedActor = actor;
					CurrentActorTargetingDelay = ActorTargetingDelay;
				}
			}
		}
	}
}





bool UEGDynamicTargetingComponent::CheckShouldBreakLineOfSight(float DeltaTime)
{
	if (!TargetedActor || !TargetedActorComponent || !OwnerActor) return false;
	
	TargetObstacleLineTrace.Start = OwnerActor->GetActorLocation();
	TargetObstacleLineTrace.End = TargetedActorComponent->GetComponentLocation();
	if (UEFTraceLibrary::ExtendedLineTraceSingle(GetWorld(),TargetObstacleLineTrace))
	{
		if (TargetObstacleLineTrace.GetHitActor() != TargetedActor )
			CurrentBreakLineOfSight -= DeltaTime;
		else
			CurrentBreakLineOfSight = BreakLineOfSightDelay;

		return CurrentBreakLineOfSight <= 0;
	}
	return false;
}










AActor* UEGDynamicTargetingComponent::FindNearestTarget(const TArray<AActor*>& Actors)
{
	if (Actors.Num() == 0) 
		return nullptr;

	if (Actors.Num() == 1 && Actors[0])
	{
		if (const auto TargetingComp = Actors[0]->FindComponentByClass<UEGTargetingTargetComponent>())
		{
			TargetObstacleLineTrace.Start = OwnerActor->GetActorLocation() + FVector(0,0,30);
			TargetObstacleLineTrace.End = TargetingComp->GetComponentLocation();
			if (UEFTraceLibrary::ExtendedLineTraceSingle(GetWorld(),TargetObstacleLineTrace))
			{
				// Make sure Actor Is In The Viewport
				if (Actors[0] == TargetObstacleLineTrace.GetHitActor() && IsInViewport(Actors[0]) && UEFMathLibrary::GetDistanceBetweenActors(GetOwner() ,  Actors[0]) < MaximumDistanceToEnable)
					return Actors[0];
					
				return nullptr;
			}
		}
	}
	
	TMap<AActor*,float>ValidActors;
	for (const auto Actor : Actors)
	{
		if (const auto TargetingComp = Actor->FindComponentByClass<UEGTargetingTargetComponent>())
		{
			TargetObstacleLineTrace.Start = OwnerActor->GetActorLocation() + FVector(0,0,30);
			TargetObstacleLineTrace.End = TargetingComp->GetComponentLocation();

			// Check This Actor Has A Obstacle in Front Of Him
			if (UEFTraceLibrary::ExtendedLineTraceSingle(GetWorld(),TargetObstacleLineTrace))
			{
				// Make sure Actor Is In The Viewport
				if (Actor == TargetObstacleLineTrace.GetHitActor() && IsInViewport(Actor))
				{
					ValidActors.Add(Actor , UEFMathLibrary::GetDistanceBetweenActors(GetOwner() , Actor));
				}
			}
		}
	}

	// If None Actor Hit Return
	if (ValidActors.Num() == 0)
		return nullptr;
	
	if (const auto target = UEFMathLibrary::GetActorInTheCenterOfTheScreen(ValidActors))
		return target;
	
	return nullptr;
	
}





void UEGDynamicTargetingComponent::TargetLockOn(AActor* TargetToLockOn)
{
	if (!IsValid(TargetToLockOn)) return;
	
	// Recast PlayerController in case it wasn't already setup on Begin Play (local split screen)
	SetupLocalPlayerController();

	TargetedActor = TargetToLockOn;
	IsTargetEnabled = true;
	TargetedActorComponent = TargetedActor->FindComponentByClass<UEGTargetingTargetComponent>();
	
	if (bShouldControlRotation)
		ControlRotation(true);
	
	if (bAdjustPitchBasedOnDistanceToTarget || bIgnoreLookInput && IsValid(OwnerPlayerController))
		OwnerPlayerController->SetIgnoreLookInput(true);
	
	if (OnTargetLockedOn.IsBound()) OnTargetLockedOn.Broadcast(TargetToLockOn);
	
}





void UEGDynamicTargetingComponent::TargetLockOff()
{
	// Recast PlayerController in case it wasn't already setup on Begin Play (local split screen)
	SetupLocalPlayerController();
	if (TargetedActor)
	{
		if (bShouldControlRotation)
			ControlRotation(false);
		if (IsValid(OwnerPlayerController))
			OwnerPlayerController->ResetIgnoreLookInput();

		if (OnTargetLockedOff.IsBound())
			OnTargetLockedOff.Broadcast(TargetedActor);
	}
	TargetedActor = nullptr;
	IsTargetEnabled = false;
}






TArray<AActor*> UEGDynamicTargetingComponent::FindTargetsInRange(TArray<AActor*> ActorsToLook, const float RangeMin, const float RangeMax) const
{
	TArray<AActor*> ActorsInRange;
	for (AActor* Actor : ActorsToLook)
	{
		const float Angle = GetAngleUsingCameraRotation(Actor);
		if (Angle > RangeMin && Angle < RangeMax)
			ActorsInRange.Add(Actor);
	}
	return ActorsInRange;
}





float UEGDynamicTargetingComponent::GetAngleUsingCameraRotation(const AActor* ActorToLook) const
{
	UCameraComponent* CameraComponent = OwnerActor->FindComponentByClass<UCameraComponent>();
	if (!CameraComponent)
	{
		// Fallback to CharacterRotation if no CameraComponent can be found
		return GetAngleUsingCharacterRotation(ActorToLook);
	}

	const FRotator CameraWorldRotation = CameraComponent->GetComponentRotation();
	const FRotator LookAtRotation = UEFMathLibrary::GetRotationBetweenVectors(CameraComponent->GetComponentLocation(), ActorToLook->GetActorLocation());

	float YawAngle = CameraWorldRotation.Yaw - LookAtRotation.Yaw;
	
	if (YawAngle < 0) YawAngle = YawAngle + 360;

	return YawAngle;
}





float UEGDynamicTargetingComponent::GetAngleUsingCharacterRotation(const AActor* ActorToLook) const
{
	const FRotator CharacterRotation = OwnerActor->GetActorRotation();
	const FRotator LookAtRotation = UEFMathLibrary::GetRotationBetweenVectors(OwnerActor->GetActorLocation(), ActorToLook->GetActorLocation());

	float YawAngle = CharacterRotation.Yaw - LookAtRotation.Yaw;
	
	if (YawAngle < 0) YawAngle = YawAngle + 360;

	return YawAngle;
}





FRotator UEGDynamicTargetingComponent::GetControlRotationOnTarget(const AActor* OtherActor) const
{
	if (!IsValid(OwnerPlayerController))
	{
		UE_LOG(LogBlueprint,Warning, TEXT("UEGDynamicTargetingComponent::GetControlRotationOnTarget - OwnerPlayerController is not valid ..."))
		return FRotator::ZeroRotator;
	}

	const FRotator ControlRotation = OwnerPlayerController->GetControlRotation();

	const FVector CharacterLocation = OwnerActor->GetActorLocation();
	const FVector OtherActorLocation = OtherActor->GetActorLocation();

	// Find look at rotation
	const FRotator LookRotation = FRotationMatrix::MakeFromX(OtherActorLocation - CharacterLocation).Rotator();
	float Pitch = LookRotation.Pitch;
	FRotator TargetRotation;
	if (bAdjustPitchBasedOnDistanceToTarget)
	{
		const float DistanceToTarget = UEFMathLibrary::GetDistanceBetweenActors(OwnerActor , OtherActor);
		const float PitchInRange = (DistanceToTarget * PitchDistanceCoefficient + PitchDistanceOffset) * -1.0f;
		const float PitchOffset = FMath::Clamp(PitchInRange, PitchMin, PitchMax);

		Pitch = Pitch + PitchOffset;
		TargetRotation = FRotator(Pitch, LookRotation.Yaw, ControlRotation.Roll);
	}
	else
	{
		if (bIgnoreLookInput)
			TargetRotation = FRotator(Pitch, LookRotation.Yaw, ControlRotation.Roll);
		else
			TargetRotation = FRotator(ControlRotation.Pitch, LookRotation.Yaw, ControlRotation.Roll);
	}

	return FMath::RInterpTo(ControlRotation, TargetRotation, GetWorld()->GetDeltaSeconds(), 9.0f);
}





void UEGDynamicTargetingComponent::SetControlRotationOnTarget(AActor* TargetActor) const
{
	if (!IsValid(OwnerPlayerController) || !IsValid(TargetActor))
		return;

	GetControlRotationOnTarget(TargetActor);

	const auto PlayerCamera = OwnerPlayerController->PlayerCameraManager;
	const FRotator ControlRotation = UKismetMathLibrary::FindLookAtRotation(PlayerCamera->GetCameraLocation()  , TargetActor->GetActorLocation()) ;
	PlayerCamera->SetActorRotation(UKismetMathLibrary::RInterpTo(PlayerCamera->GetCameraRotation() , ControlRotation , GetWorld()->GetDeltaSeconds(),ControlRotationInterpSpeed));
	
	/*
	OwnerPlayerController->PlayerCameraManager.Get()->SetActorRotation(UKismetMathLibrary::RInterpTo())
	OwnerPlayerController->SetControlRotation(UKismetMathLibrary::RInterpTo(OwnerPlayerController->GetControlRotation(),ControlRotation,GetWorld()->GetDeltaSeconds(),ControlRotationInterpSpeed));
	*/
}





void UEGDynamicTargetingComponent::ControlRotation(const bool ShouldControlRotation) const
{
	if (!IsValid(OwnerPawn)) return;

	OwnerPawn->bUseControllerRotationYaw = ShouldControlRotation;

	UCharacterMovementComponent* CharacterMovementComponent = OwnerPawn->FindComponentByClass<UCharacterMovementComponent>();
	if (CharacterMovementComponent)
		CharacterMovementComponent->bOrientRotationToMovement = !ShouldControlRotation;
}





void UEGDynamicTargetingComponent::SetupLocalPlayerController()
{
	if (OwnerPlayerController) return;

	if (!IsValid(OwnerPawn))
	{
		UE_LOG(LogBlueprint,Error, TEXT("[%s] TargetSystemComponent: Component is meant to be added to Pawn only ..."), *GetName());
		return;
	}
	OwnerPlayerController = Cast<APlayerController>(OwnerPawn->GetController());
}




bool UEGDynamicTargetingComponent::IsInViewport(const AActor* TargetActor) const
{
	if (!IsValid(OwnerPlayerController)) return true;
	FVector2D ScreenLocation;
	OwnerPlayerController->ProjectWorldLocationToScreen(TargetActor->GetActorLocation(), ScreenLocation);
	FVector2D ViewportSize;
	GetWorld()->GetGameViewport()->GetViewportSize(ViewportSize);
	return ScreenLocation.X > 0 && ScreenLocation.Y > 0 && ScreenLocation.X < ViewportSize.X && ScreenLocation.Y < ViewportSize.Y;
}

