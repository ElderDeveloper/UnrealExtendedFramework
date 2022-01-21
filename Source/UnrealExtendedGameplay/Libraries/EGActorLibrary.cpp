// Fill out your copyright notice in the Description page of Project Settings.


#include "EGActorLibrary.h"

#include "Kismet/KismetMathLibrary.h"


float UEGActorLibrary::GetActorLocationX(AActor* Actor)
{
	if (Actor) return Actor->GetActorLocation().X;
	return 0;
}

float UEGActorLibrary::GetActorLocationY(AActor* Actor)
{
	if (Actor) return Actor->GetActorLocation().Y;
	return 0;
}

float UEGActorLibrary::GetActorLocationZ(AActor* Actor)
{
	if (Actor) return Actor->GetActorLocation().Z;
	return 0;
}

float UEGActorLibrary::GetActorRotationYaw(AActor* Actor)
{
	if (Actor) return Actor->GetActorRotation().Yaw;
	return 0;
}

float UEGActorLibrary::GetActorRotationPitch(AActor* Actor)
{
	if (Actor) return Actor->GetActorRotation().Pitch;
	return 0;
}

float UEGActorLibrary::GetActorRotationRoll(AActor* Actor)
{
	if (Actor) return Actor->GetActorRotation().Roll;
	return 0;
}



void UEGActorLibrary::RotateToObjectYaw(AActor* From, AActor* To)
{
	if (From && To)
	{
		FRotator Rot = From->GetActorRotation();
		Rot.Yaw = UKismetMathLibrary::FindLookAtRotation(From->GetActorLocation(),To->GetActorLocation()).Yaw;
		From->SetActorRotation(Rot);
	}
}




void UEGActorLibrary::RotateToObject(AActor* From, AActor* To)
{
	if (From && To)
	{
		From->SetActorRotation(UKismetMathLibrary::FindLookAtRotation(From->GetActorLocation(),To->GetActorLocation()));
	}
}




void UEGActorLibrary::RotateToObjectInterpYaw(const UObject* WorldContextObject, AActor* From, AActor* To,float InterpSpeed)
{
	if (From && To)
	{
		FRotator Rot = From->GetActorRotation();
		Rot.Yaw = UKismetMathLibrary::FInterpTo(
			Rot.Yaw,
			UKismetMathLibrary::FindLookAtRotation(From->GetActorLocation(),To->GetActorLocation()).Yaw,
			WorldContextObject->GetWorld()->GetDeltaSeconds(),
			InterpSpeed);
		 
		From->SetActorRotation(Rot);
	}
}




void UEGActorLibrary::RotateToObjectInterp(const UObject* WorldContextObject, AActor* From, AActor* To,float InterpSpeed)
{
	if (From && To)
	{
		From->SetActorRotation(UKismetMathLibrary::RInterpTo(
			From->GetActorRotation() ,
			UKismetMathLibrary::FindLookAtRotation(From->GetActorLocation(),To->GetActorLocation())
			, WorldContextObject->GetWorld()->GetDeltaSeconds() , InterpSpeed));
	}
}
