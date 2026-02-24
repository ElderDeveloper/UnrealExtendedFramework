// Fill out your copyright notice in the Description page of Project Settings.


#include "EFActorLibrary.h"

#include "GameFramework/PlayerState.h"
#include "Kismet/KismetMathLibrary.h"
#include "UnrealExtendedFramework/Data/EFMacro.h"


// ================================ GET LOCATION ================================

float UEFActorLibrary::GetActorLocationX(AActor* Actor)
{
	if (Actor) return Actor->GetActorLocation().X;
	return 0;
}

float UEFActorLibrary::GetActorLocationY(AActor* Actor)
{
	if (Actor) return Actor->GetActorLocation().Y;
	return 0;
}

float UEFActorLibrary::GetActorLocationZ(AActor* Actor)
{
	if (Actor) return Actor->GetActorLocation().Z;
	return 0;
}


// ================================ GET ROTATION ================================

float UEFActorLibrary::GetActorRotationYaw(AActor* Actor)
{
	if (Actor) return Actor->GetActorRotation().Yaw;
	return 0;
}

float UEFActorLibrary::GetActorRotationPitch(AActor* Actor)
{
	if (Actor) return Actor->GetActorRotation().Pitch;
	return 0;
}

float UEFActorLibrary::GetActorRotationRoll(AActor* Actor)
{
	if (Actor) return Actor->GetActorRotation().Roll;
	return 0;
}


// ================================ DIRECTION VECTORS ================================

FVector UEFActorLibrary::GetActorForwardVector(AActor* Actor)
{
	if (Actor) return Actor->GetActorForwardVector();
	return FVector::ZeroVector;
}

FVector UEFActorLibrary::GetActorRightVector(AActor* Actor)
{
	if (Actor) return Actor->GetActorRightVector();
	return FVector::ZeroVector;
}

FVector UEFActorLibrary::GetActorUpVector(AActor* Actor)
{
	if (Actor) return Actor->GetActorUpVector();
	return FVector::ZeroVector;
}


// ================================ SET ROTATION ================================

void UEFActorLibrary::RotateToObjectYaw(AActor* From, AActor* To)
{
	if (From && To)
	{
		FRotator Rot = From->GetActorRotation();
		Rot.Yaw = UKismetMathLibrary::FindLookAtRotation(From->GetActorLocation(),To->GetActorLocation()).Yaw;
		From->SetActorRotation(Rot);
	}
}

void UEFActorLibrary::RotateToObject(AActor* From, AActor* To)
{
	if (From && To)
	{
		From->SetActorRotation(UKismetMathLibrary::FindLookAtRotation(From->GetActorLocation(),To->GetActorLocation()));
	}
}

void UEFActorLibrary::RotateToObjectInterpYaw(const UObject* WorldContextObject, AActor* From, AActor* To, float InterpSpeed, bool UseFindLookAtRotation)
{
	if (!From || !To || !WorldContextObject || !WorldContextObject->GetWorld())
	{
		return;
	}

	const float DeltaSeconds = WorldContextObject->GetWorld()->GetDeltaSeconds();
	const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(From->GetActorLocation(), To->GetActorLocation());

	if (UseFindLookAtRotation)
	{
		// BUG FIX: Previously used FInterpTo on raw yaw which doesn't handle angular
		// wraparound (e.g. 350->10 would go the 340-degree long way around).
		// Now uses NormalizeAxis to always interpolate via the shortest angular path.
		FRotator Rot = From->GetActorRotation();
		const float ShortestAngle = FRotator::NormalizeAxis(LookAtRotation.Yaw - Rot.Yaw);
		Rot.Yaw = FRotator::NormalizeAxis(Rot.Yaw + ShortestAngle * FMath::Clamp(DeltaSeconds * InterpSpeed, 0.0f, 1.0f));
		From->SetActorRotation(Rot);
	}
	else
	{
		const FRotator CurrentRotation = From->GetActorRotation();
		const float ShortestAngle = FRotator::NormalizeAxis(LookAtRotation.Yaw - CurrentRotation.Yaw);
		const float DeltaMove = ShortestAngle * FMath::Clamp(DeltaSeconds * InterpSpeed, 0.0f, 1.0f);
		From->SetActorRotation(FRotator(0, CurrentRotation.Yaw + DeltaMove, 0));
	}
}

void UEFActorLibrary::RotateToLocationInterpYaw(const UObject* WorldContextObject, AActor* From, const FVector& To, float InterpSpeed, bool UseFindLookAtRotation)
{
	if (!From || !WorldContextObject || !WorldContextObject->GetWorld())
	{
		return;
	}

	const float DeltaSeconds = WorldContextObject->GetWorld()->GetDeltaSeconds();
	const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(From->GetActorLocation(), To);

	if (UseFindLookAtRotation)
	{
		// BUG FIX: Same wraparound fix as RotateToObjectInterpYaw.
		FRotator Rot = From->GetActorRotation();
		const float ShortestAngle = FRotator::NormalizeAxis(LookAtRotation.Yaw - Rot.Yaw);
		Rot.Yaw = FRotator::NormalizeAxis(Rot.Yaw + ShortestAngle * FMath::Clamp(DeltaSeconds * InterpSpeed, 0.0f, 1.0f));
		From->SetActorRotation(Rot);
	}
	else
	{
		const FRotator CurrentRotation = From->GetActorRotation();
		const float ShortestAngle = FRotator::NormalizeAxis(LookAtRotation.Yaw - CurrentRotation.Yaw);
		const float DeltaMove = ShortestAngle * FMath::Clamp(DeltaSeconds * InterpSpeed, 0.0f, 1.0f);
		From->SetActorRotation(FRotator(0, CurrentRotation.Yaw + DeltaMove, 0));
	}
}


void UEFActorLibrary::RotateToObjectInterp(const UObject* WorldContextObject, AActor* From, AActor* To, float InterpSpeed)
{
	if (!From || !To || !WorldContextObject || !WorldContextObject->GetWorld())
	{
		return;
	}

	From->SetActorRotation(UKismetMathLibrary::RInterpTo(
		From->GetActorRotation() ,
		UKismetMathLibrary::FindLookAtRotation(From->GetActorLocation(),To->GetActorLocation())
		, WorldContextObject->GetWorld()->GetDeltaSeconds() , InterpSpeed));
}

void UEFActorLibrary::RotateToLocationInterp(const UObject* WorldContextObject, AActor* From, const FVector& To, float InterpSpeed, float MaxDegreePerSecond)
{
	// BUG FIX: Added null check on From — all other rotation functions had this, but this one didn't.
	if (!From || !WorldContextObject || !WorldContextObject->GetWorld())
	{
		return;
	}

	FRotator current = From->GetActorRotation();
	const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(From->GetActorLocation(), To);
	const FRotator DeltaRotation = UKismetMathLibrary::NormalizedDeltaRotator(LookAtRotation, current);
	const float Angle = FMath::Abs(DeltaRotation.Yaw) + FMath::Abs(DeltaRotation.Pitch) + FMath::Abs(DeltaRotation.Roll);
	const float DeltaTime = WorldContextObject->GetWorld()->GetDeltaSeconds() * InterpSpeed;
	if (Angle > 0.001f)
	{
		const float DeltaAngle = FMath::Clamp(Angle / DeltaTime, -MaxDegreePerSecond, MaxDegreePerSecond) * DeltaTime;
		const FRotator NewRotator = UKismetMathLibrary::RInterpTo(current, LookAtRotation, DeltaTime, DeltaAngle);
		From->SetActorRotation(NewRotator);
	}
}


// ================================ NETWORKING ================================

bool UEFActorLibrary::IsActorLocal(AActor* Actor)
{
	if (!Actor) return false;
	
	if (const APawn* Pawn = Cast<APawn>(Actor)) return Pawn->IsLocallyControlled();
	
	if (const AController* Controller = Cast<AController>(Actor)) return Controller->IsLocalController();
	
	if (const APlayerState* PS = Cast<APlayerState>(Actor))
	{
		if (const AController* Controller = Cast<AController>(PS->GetOwner())) return Controller->IsLocalController();
	}

	return false;
}

void UEFActorLibrary::SwitchIsLocallyControlled(AActor* Actor, TEnumAsByte<EFConditionOutput>& OutPins)
{
	IsActorLocal(Actor) ? OutPins = UEF_True :  OutPins = UEF_False;
}


// ================================ CLASS ================================

bool UEFActorLibrary::IsClassEqualOrChildOfOtherClass(TSubclassOf<AActor> TestClass, TSubclassOf<AActor> ParentClass)
{
	return TestClass == ParentClass || UKismetMathLibrary::ClassIsChildOf(TestClass,ParentClass);
}


// ================================ CONTROL ROTATION ================================

void UEFActorLibrary::GetActorControlRotationDirection(APawn* Pawn, FVector& Forward, FVector& Right, bool YawOnly)
{
	Forward = FVector::ZeroVector;
	Right = FVector::ZeroVector;
	
	if(Pawn)
	{
		const FRotator Rot = YawOnly ?  FRotator(0 ,Pawn->GetControlRotation().Yaw,0) : Pawn->GetControlRotation();
		Forward = Rot.Vector();
		Right = UKismetMathLibrary::GetRightVector(Rot);
	}
}

FRotator UEFActorLibrary::GetActorControlRotationYaw(APawn* Pawn)
{
	FRotator Return = FRotator();
	if (Pawn)
		Return = FRotator(0 ,Pawn->GetControlRotation().Yaw,0);
	return Return;
}


// ================================ COORDINATE SPACE ================================

FVector UEFActorLibrary::WorldToLocal(const AActor* Actor, const FVector& WorldLocation)
{
	if (Actor)
	{
		return UKismetMathLibrary::InverseTransformLocation(Actor->GetActorTransform(), WorldLocation);
	}
	return FVector::ZeroVector;
}

FVector UEFActorLibrary::LocalToWorld(const AActor* Actor, const FVector& LocalLocation)
{
	if (Actor)
	{
		return UKismetMathLibrary::TransformLocation(Actor->GetActorTransform(), LocalLocation);
	}
	return FVector::ZeroVector;
}


// ================================ DISTANCE ================================

float UEFActorLibrary::DistanceBetweenActors(const AActor* ActorA, const AActor* ActorB)
{
	if (!ActorA || !ActorB)
	{
		return -1.0f;
	}
	return FVector::Dist(ActorA->GetActorLocation(), ActorB->GetActorLocation());
}

float UEFActorLibrary::DistanceBetweenActors2D(const AActor* ActorA, const AActor* ActorB)
{
	if (!ActorA || !ActorB)
	{
		return -1.0f;
	}
	return FVector::Dist2D(ActorA->GetActorLocation(), ActorB->GetActorLocation());
}
