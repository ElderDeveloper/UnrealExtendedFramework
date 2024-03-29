﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "EFActorLibrary.h"

#include "GameFramework/PlayerState.h"
#include "Kismet/KismetMathLibrary.h"
#include "UnrealExtendedFramework/Data/EFMacro.h"


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




void UEFActorLibrary::RotateToObjectInterpYaw(const UObject* WorldContextObject, AActor* From, AActor* To,float InterpSpeed , bool UseFindLookAtRotation)
{
	if (From && To)
	{
		if (UseFindLookAtRotation)
		{
			FRotator Rot = From->GetActorRotation();
			Rot.Yaw = UKismetMathLibrary::FInterpTo(
				Rot.Yaw,
				UKismetMathLibrary::FindLookAtRotation(From->GetActorLocation(),To->GetActorLocation()).Yaw,
				WorldContextObject->GetWorld()->GetDeltaSeconds(),
				InterpSpeed);
		 
			From->SetActorRotation(Rot);
		}
		else
		{
			float current = From->GetActorRotation().Yaw;
			const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(From->GetActorLocation(),To->GetActorLocation());
			const int Difference = LookAtRotation.Yaw - current;
			const float shortest_angle=(((Difference % 360) + 540) % 360) - 180;
			const float deltaMove = shortest_angle * FMath::Clamp(WorldContextObject->GetWorld()->GetDeltaSeconds() * InterpSpeed, 0.0f, 1.0f);
			From->SetActorRotation(FRotator(0,current += deltaMove,0));
		}
	}
}

void UEFActorLibrary::RotateToLocationInterpYaw(const UObject* WorldContextObject, AActor* From, const FVector& To,float InterpSpeed, bool UseFindLookAtRotation)
{
	if (From)
	{
		if (UseFindLookAtRotation)
		{
			FRotator Rot = From->GetActorRotation();
			Rot.Yaw = UKismetMathLibrary::FInterpTo(
				Rot.Yaw,
				UKismetMathLibrary::FindLookAtRotation(From->GetActorLocation(),To).Yaw,
				WorldContextObject->GetWorld()->GetDeltaSeconds(),
				InterpSpeed);
		 
			From->SetActorRotation(Rot);
		}
		else
		{
			float current = From->GetActorRotation().Yaw;
			const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(From->GetActorLocation(), To);
			const int Difference = LookAtRotation.Yaw - current;
			const float shortest_angle = (((Difference % 360) + 540) % 360) - 180;
			const float deltaMove = shortest_angle * FMath::Clamp(WorldContextObject->GetWorld()->GetDeltaSeconds() * InterpSpeed, 0.0f, 1.0f);
			From->SetActorRotation(FRotator(0, current += deltaMove, 0));
		}
	}
}


void UEFActorLibrary::RotateToObjectInterp(const UObject* WorldContextObject, AActor* From, AActor* To,float InterpSpeed)
{
	if (From && To)
	{
		From->SetActorRotation(UKismetMathLibrary::RInterpTo(
			From->GetActorRotation() ,
			UKismetMathLibrary::FindLookAtRotation(From->GetActorLocation(),To->GetActorLocation())
			, WorldContextObject->GetWorld()->GetDeltaSeconds() , InterpSpeed));
	}
}

void UEFActorLibrary::RotateToLocationInterp(const UObject* WorldContextObject, AActor* From, const FVector& To,float InterpSpeed , float MaxDegreePerSecond)
{
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


bool UEFActorLibrary::IsActorLocal(AActor* Actor)
{
	if (!Actor) return false;
	
	if(const auto& Pawn = Cast<APawn>(Actor)) return Pawn->IsLocallyControlled();
	
	if (const auto& Controller = Cast<AController>(Actor)) return Controller->IsLocalController();
	
	if (const auto& PS = Cast<APlayerState>(Actor))
		if (const auto& Controller = Cast<AController>(PS->GetOwner())) return Controller->IsLocalController();

	return false;
}

void UEFActorLibrary::SwitchIsLocallyControlled(AActor* Actor, TEnumAsByte<EFConditionOutput>& OutPins)
{
	IsActorLocal(Actor) ? OutPins = UEF_True :  OutPins = UEF_False;
}

bool UEFActorLibrary::IsClassEqualOrChildOfOtherClass(TSubclassOf<AActor> TestClass, TSubclassOf<AActor> ParentClass)
{
	return TestClass == ParentClass || UKismetMathLibrary::ClassIsChildOf(TestClass,ParentClass);
}

void UEFActorLibrary::GetActorControlRotationDirection(APawn* Pawn, FVector& Forward, FVector& Right , bool YawOnly)
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
	if ( Actor)
	{
		return UKismetMathLibrary::TransformLocation(Actor->GetActorTransform(), LocalLocation);
	}
	return FVector::ZeroVector;
}
