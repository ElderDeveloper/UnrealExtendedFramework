// Fill out your copyright notice in the Description page of Project Settings.


#include "EGTargetingComponent.h"

#include "EGTargetingTargetComponent.h"
#include "EngineUtils.h"
#include "Camera/CameraComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EGTargetingWidget.h"
#include "Kismet/KismetMathLibrary.h"
#include "UnrealExtendedFramework/Data/EFMacro.h"
#include "UnrealExtendedFramework/Data/EFTraceData.h"
#include "UnrealExtendedFramework/Libraries/Math/EFMathLibrary.h"
#include "UnrealExtendedFramework/Libraries/Trace/EFTraceLibrary.h"



UEGTargetingComponent::UEGTargetingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	TargetActorSearchSphere.Radius = MaximumDistanceToEnable;
}

void UEGTargetingComponent::BeginPlay()
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


void UEGTargetingComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsTargetingEnabled || !TargetedActor) return;
	
	SetControlRotationOnTarget(TargetedActor);

	// Target Locked Off based on Distance
	if (UEFMathLibrary::GetDistanceBetweenActors(OwnerActor , TargetedActor) > MaximumDistanceToEnable) TargetLockOff();
	

	if (ShouldBreakLineOfSight() && !bIsBreakingLineOfSight)
	{
		if (BreakLineOfSightDelay <= 0) TargetLockOff();
		
		else
		{
			bIsBreakingLineOfSight = true;
			GetWorld()->GetTimerManager().SetTimer(
				LineOfSightBreakTimerHandle,
				this,
				&UEGTargetingComponent::BreakLineOfSight,
				BreakLineOfSightDelay
			);
		}
	}
	
	if (!IsValid(TargetedActor)) TargetLockOff();
}







void UEGTargetingComponent::EnableDisableTargeting()
{
	if (GetIsTargetingEnabled())
		TargetLockOff();
	
	else
	{
		ClosestTargetDistance = MaximumDistanceToEnable;
		TargetActorSearchSphere.Start = OwnerActor->GetActorLocation();
		TargetActorSearchSphere.End = OwnerActor->GetActorLocation();
		
		if (UEFTraceLibrary::ExtendedSphereTraceMulti(GetWorld(),TargetActorSearchSphere))
		{
			TArray<AActor*> HitActorArray;
			for (const auto hit : TargetActorSearchSphere.HitResults)
			{
				GEngine->AddOnScreenDebugMessage(-1,2.f,FColor::Green,hit.GetActor()->GetName());
				HitActorArray.Add(hit.GetActor());
			}
			
			if (const auto Actor = FindNearestTarget(HitActorArray))
			{
				GEngine->AddOnScreenDebugMessage(-1,2.f,FColor::Yellow,Actor->GetName());
				TargetLockOn(Actor);
			}
		}
		
	}
}



AActor* UEGTargetingComponent::FindNearestTarget(const TArray<AActor*>& Actors)
{

	TArray<AActor*> ValidActors;

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
					ValidActors.Add(Actor);
			}
		}
	}

	// If None Actor Hit Return
	UE_LOG(LogBlueprint,Log,TEXT("EG Targeting Component Cant Find Any Actor With EGTargeting Component or has no Actor In The Viewport"))
	if (ValidActors.Num() == 0) return nullptr;
	

	float ClosestDistance = ClosestTargetDistance;
	AActor* Target = nullptr;
	
	for (AActor* Actor : ValidActors)
	{
		const float Distance = UEFMathLibrary::GetDistanceBetweenActors(OwnerActor , Actor);
		if (Distance < ClosestDistance)
		{
			ClosestDistance = Distance;
			Target = Actor;
		}
	}

	return Target;
}





void UEGTargetingComponent::TargetLockOn(AActor* TargetToLockOn)
{
	if (!IsValid(TargetToLockOn)) return;
	
	// Recast PlayerController in case it wasn't already setup on Begin Play (local split screen)
	SetupLocalPlayerController();

	TargetedActor = TargetToLockOn;

	TargetedActorComponent = TargetedActor->FindComponentByClass<UEGTargetingTargetComponent>();

	bIsTargetingEnabled = true;
	
	if (bShouldDrawLockedOnWidget)
		CreateAndAttachTargetLockedOnWidgetComponent(TargetToLockOn);

	if (bShouldControlRotation)
		ControlRotation(true);
	
	if (bAdjustPitchBasedOnDistanceToTarget || bIgnoreLookInput && IsValid(OwnerPlayerController))
		OwnerPlayerController->SetIgnoreLookInput(true);
	
	if (OnTargetLockedOn.IsBound()) OnTargetLockedOn.Broadcast(TargetToLockOn);
	
}




void UEGTargetingComponent::TargetLockOff()
{
	// Recast PlayerController in case it wasn't already setup on Begin Play (local split screen)
	SetupLocalPlayerController();

	bIsTargetingEnabled = false;
	if (TargetLockedOnWidgetComponent)
		TargetLockedOnWidgetComponent->DestroyComponent();

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
}




void UEGTargetingComponent::TargetActorWithAxisInput(const float AxisValue)
{

	/*
	 * If we're not locked on, do nothing
	 * If we're not allowed to switch target, do nothing
	 * If we're switching target, do nothing for a set amount of time
	 */
	if (!bIsTargetingEnabled || !TargetedActor || bIsSwitchingTarget || !ShouldSwitchTargetActor(AxisValue))
		return;
	
/*
	// Lock off target
	AActor* CurrentTarget = TargetedActor;

	// Depending on Axis Value negative / positive, set Direction to Look for (negative: left, positive: right)
	const float RangeMin = AxisValue < 0 ? 0 : 180;
	const float RangeMax = AxisValue < 0 ? 180 : 360;

	// Reset Closest Target Distance to Minimum Distance to Enable
	ClosestTargetDistance = MaximumDistanceToEnable;

	// Get All Actors of Class
	TArray<AActor*> Actors = GetAllActorsOfClass(TargetableActors);

	// For each of these actors, check line trace and ignore Current Target and build the list of actors to look from
	TArray<AActor*> ActorsToLook;

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(CurrentTarget);
	for (AActor* Actor : Actors)
	{
		const bool bHit = LineTraceForActor(Actor, ActorsToIgnore);
		if (bHit && IsInViewport(Actor))
			ActorsToLook.Add(Actor);
	}

	// Find Targets in Range (left or right, based on Character and CurrentTarget)
	TArray<AActor*> TargetsInRange = FindTargetsInRange(ActorsToLook, RangeMin, RangeMax);

	// For each of these targets in range, get the closest one to current target
	AActor* ActorToTarget = nullptr;
	for (AActor* Actor : TargetsInRange)
	{
		// and filter out any character too distant from minimum distance to enable
		const float Distance = UEFMathLibrary::GetDistanceBetweenActors(OwnerActor , Actor);
		if (Distance < MaximumDistanceToEnable)
		{
			const float RelativeActorsDistance = CurrentTarget->GetDistanceTo(Actor);
			if (RelativeActorsDistance < ClosestTargetDistance)
			{
				ClosestTargetDistance = RelativeActorsDistance;
				ActorToTarget = Actor;
			}
		}
	}

	if (ActorToTarget)
	{
		if (SwitchingTargetTimerHandle.IsValid())
			SwitchingTargetTimerHandle.Invalidate();
		

		TargetLockOff();
		TargetedActor = ActorToTarget;
		TargetLockOn(ActorToTarget);

		GetWorld()->GetTimerManager().SetTimer(
			SwitchingTargetTimerHandle,
			this,
			&UEGTargetingComponent::ResetIsSwitchingTarget,
			// Less sticky if still switching
			bIsSwitchingTarget ? 0.25f : 0.5f
		);

		bIsSwitchingTarget = true;
	}

	*/
}











TArray<AActor*> UEGTargetingComponent::FindTargetsInRange(TArray<AActor*> ActorsToLook, const float RangeMin, const float RangeMax) const
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




float UEGTargetingComponent::GetAngleUsingCameraRotation(const AActor* ActorToLook) const
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




float UEGTargetingComponent::GetAngleUsingCharacterRotation(const AActor* ActorToLook) const
{
	const FRotator CharacterRotation = OwnerActor->GetActorRotation();
	const FRotator LookAtRotation = UEFMathLibrary::GetRotationBetweenVectors(OwnerActor->GetActorLocation(), ActorToLook->GetActorLocation());

	float YawAngle = CharacterRotation.Yaw - LookAtRotation.Yaw;
	
	if (YawAngle < 0) YawAngle = YawAngle + 360;

	return YawAngle;
}






void UEGTargetingComponent::ResetIsSwitchingTarget()
{
	bIsSwitchingTarget = false;
	bDesireToSwitch = false;
}




bool UEGTargetingComponent::ShouldSwitchTargetActor(const float AxisValue)
{
	// Sticky feeling computation
	if (bEnableStickyTarget)
	{
		StartRotatingStack += (AxisValue != 0) ? AxisValue * AxisMultiplier : (StartRotatingStack > 0 ? -AxisMultiplier : AxisMultiplier);

		if (AxisValue == 0 && FMath::Abs(StartRotatingStack) <= AxisMultiplier) StartRotatingStack = 0.0f;
		
		// If Axis value does not exceeds configured threshold, do nothing
		if (FMath::Abs(StartRotatingStack) < StickyRotationThreshold)
		{
			bDesireToSwitch = false;
			return false;
		}

		//Sticky when switching target.
		if (StartRotatingStack * AxisValue > 0) StartRotatingStack = StartRotatingStack > 0 ? StickyRotationThreshold : -StickyRotationThreshold;
		
		else if (StartRotatingStack * AxisValue < 0) StartRotatingStack = StartRotatingStack * -1.0f;

		bDesireToSwitch = true;
		return true;
	}

	// Non Sticky feeling, check Axis value exceeds threshold
	return FMath::Abs(AxisValue) > StartRotatingThreshold;
}






void UEGTargetingComponent::CreateAndAttachTargetLockedOnWidgetComponent(AActor* TargetActor)
{
	
	TargetLockedOnWidgetComponent = NewObject<UWidgetComponent>(TargetActor, MakeUniqueObjectName(TargetActor, UWidgetComponent::StaticClass(), FName("TargetLockOn")));
	TargetLockedOnWidgetComponent->SetWidgetClass(LockedOnWidget->StaticClass());

	const auto EGTargetComponent = TargetActor->FindComponentByClass<UEGTargetingTargetComponent>();
	
	if (IsValid(OwnerPlayerController))
		TargetLockedOnWidgetComponent->SetOwnerPlayer(OwnerPlayerController->GetLocalPlayer());
	
	TargetLockedOnWidgetComponent->ComponentTags.Add(FName("TargetSystem.LockOnWidget"));
	TargetLockedOnWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	TargetLockedOnWidgetComponent->SetupAttachment(EGTargetComponent);
	TargetLockedOnWidgetComponent->SetRelativeLocation(LockedOnWidgetRelativeLocation);
	TargetLockedOnWidgetComponent->SetDrawSize(FVector2D(LockedOnWidgetDrawSize, LockedOnWidgetDrawSize));
	TargetLockedOnWidgetComponent->SetVisibility(true);
	TargetLockedOnWidgetComponent->RegisterComponent();

	if (const auto widget = Cast<UEGTargetingWidget>(TargetLockedOnWidgetComponent->GetWidget()))
	{
		UE_LOG(LogBlueprint,Log,TEXT("Cast To UEExtended Targeting Widget Success"));
		widget->InitializeTargetWidget(LockedOnWidgetImage,FVector2D(LockedOnWidgetDrawSize , LockedOnWidgetDrawSize));
	}
	
	ELSE_LOG(LogBlueprint ,Error , TEXT("Cast To UEExtended Targeting Widget Failed"));
	
}













FRotator UEGTargetingComponent::GetControlRotationOnTarget(const AActor* OtherActor) const
{
	if (!IsValid(OwnerPlayerController))
	{
		UE_LOG(LogBlueprint,Warning, TEXT("UEGTargetingComponent::GetControlRotationOnTarget - OwnerPlayerController is not valid ..."))
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





void UEGTargetingComponent::SetControlRotationOnTarget(AActor* TargetActor) const
{
	if (!IsValid(OwnerPlayerController))
		return;

	const FRotator ControlRotation = GetControlRotationOnTarget(TargetActor);
	if (OnTargetSetRotation.IsBound())
		OnTargetSetRotation.Broadcast(TargetActor, ControlRotation);
	else
		OwnerPlayerController->SetControlRotation(UKismetMathLibrary::RInterpTo(OwnerPlayerController->GetControlRotation(),ControlRotation,GetWorld()->GetDeltaSeconds(),ControlRotationInterpSpeed));
}





bool UEGTargetingComponent::ShouldBreakLineOfSight()
{
	
	if (!TargetedActor || !TargetedActorComponent || !OwnerActor)
		return true;

	TargetObstacleLineTrace.Start = OwnerActor->GetActorLocation();
	TargetObstacleLineTrace.End = TargetedActorComponent->GetComponentLocation();

	if (UEFTraceLibrary::ExtendedLineTraceSingle(GetWorld(),TargetObstacleLineTrace))
	{
		if (TargetObstacleLineTrace.GetHitActor() != TargetedActor )
		{
			return true;
		}
	}
	
	return false;
	
}





void UEGTargetingComponent::BreakLineOfSight()
{
	bIsBreakingLineOfSight = false;
	if (ShouldBreakLineOfSight())
		TargetLockOff();
}





void UEGTargetingComponent::ControlRotation(const bool ShouldControlRotation) const
{
	if (!IsValid(OwnerPawn)) return;

	OwnerPawn->bUseControllerRotationYaw = ShouldControlRotation;

	UCharacterMovementComponent* CharacterMovementComponent = OwnerPawn->FindComponentByClass<UCharacterMovementComponent>();
	if (CharacterMovementComponent)
		CharacterMovementComponent->bOrientRotationToMovement = !ShouldControlRotation;
}





void UEGTargetingComponent::SetupLocalPlayerController()
{
	if (OwnerPlayerController) return;

	if (!IsValid(OwnerPawn))
	{
		UE_LOG(LogBlueprint,Error, TEXT("[%s] TargetSystemComponent: Component is meant to be added to Pawn only ..."), *GetName());
		return;
	}
	OwnerPlayerController = Cast<APlayerController>(OwnerPawn->GetController());
}





bool UEGTargetingComponent::IsInViewport(const AActor* TargetActor) const
{
	if (!IsValid(OwnerPlayerController)) return true;

	FVector2D ScreenLocation;
	OwnerPlayerController->ProjectWorldLocationToScreen(TargetActor->GetActorLocation(), ScreenLocation);

	FVector2D ViewportSize;
	GetWorld()->GetGameViewport()->GetViewportSize(ViewportSize);

	return ScreenLocation.X > 0 && ScreenLocation.Y > 0 && ScreenLocation.X < ViewportSize.X && ScreenLocation.Y < ViewportSize.Y;
}

