// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedMathLibrary.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"


// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< ROTATION >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
FRotator UUEExtendedMathLibrary::GetRotationBetweenActors(const AActor* From, const AActor* To, const FRotator PlusRotator)
{
	if (IsValid(From) && IsValid(To))
	{
		return UKismetMathLibrary::FindLookAtRotation(From->GetActorLocation(),To->GetActorLocation())+PlusRotator;
	}	return FRotator::ZeroRotator;
}



void UUEExtendedMathLibrary::GetAngleBetweenActors(AActor* From, AActor* To, float& Yaw, float& Pitch)
{
	if (IsValid(From) && IsValid(To))
	{
		const FRotator Angle = FRotator(UKismetMathLibrary::NormalizedDeltaRotator(From->GetActorRotation(),GetRotationBetweenActors(From,To)));
		Yaw = Angle.Yaw; Pitch = Angle.Pitch;
	}
}






// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< DISTANCE >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
float UUEExtendedMathLibrary::GetDistanceBetweenActors(const AActor* From, const AActor* To)
{
	if (IsValid(From) && IsValid(To))
	{
		return FVector(From->GetActorLocation()-To->GetActorLocation()).Size();
	}
	return 0;
}



float UUEExtendedMathLibrary::GetDistanceBetweenComponents(const USceneComponent* From, const USceneComponent* To)
{
	if (From && To)
	{
		return FVector(From->GetComponentLocation()-To->GetComponentLocation()).Size();
	}
	return 0;
}



float UUEExtendedMathLibrary::GetDistanceBetweenComponentAndActor(const USceneComponent* From, const AActor* To)
{
	if (From && To)
	{
		return FVector(From->GetComponentLocation()-To->GetActorLocation()).Size();
	}
	return 0;
}



float UUEExtendedMathLibrary::GetDistanceBetweenVectors(const FVector From, const FVector To)
{
	return FVector(From-To).Size();
}



float UUEExtendedMathLibrary::GetDistanceBetweenVectorsNoSquareRoot(const FVector From, const FVector To)
{
	const auto i = FVector(From-To);
	return  (i.X*i.X + i.Y*i.Y + i.Z*i.Z);
}







// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< DIRECTION >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
FVector UUEExtendedMathLibrary::GetDirectionBetweenActors(const AActor* From, const AActor* To, float scaleVector)
{
	if (From && To)
	{
		return (FVector(To->GetActorLocation() - From->GetActorLocation()).GetSafeNormal())*scaleVector;
	}
	return FVector::ZeroVector;
}



TEnumAsByte<EHitDirection> UUEExtendedMathLibrary::CalculateHitDirectionYaw(float Yaw, float& Angle)
{
	Angle=Yaw;
	if (UKismetMathLibrary::InRange_FloatFloat(Yaw,-45,45))
		return Front;

	if (UKismetMathLibrary::InRange_FloatFloat(Yaw,-180,-135) || UKismetMathLibrary::InRange_FloatFloat(Yaw,135,180))
		return Back;

	if (UKismetMathLibrary::InRange_FloatFloat(Yaw,-135,-45))
		return Right;
	
	return Left;
}



TEnumAsByte<EHitDirection> UUEExtendedMathLibrary::CalculateHitDirectionActors(AActor* From, AActor* To,float& Angle)
{
	if (IsValid(From) && IsValid(To))
	{
		float pitch;
		GetAngleBetweenActors(From,To,Angle,pitch);
		return CalculateHitDirectionYaw(Angle,pitch);
	}
	return No;
}



void UUEExtendedMathLibrary::GetComponentForwardVectorPlus(USceneComponent* Component, float Distance, FVector& CurrentLocation,FVector& ForwardLocation)
{
	if (Component)
	{
		CurrentLocation = Component->GetComponentLocation();
		ForwardLocation= CurrentLocation + Component->GetForwardVector()*Distance;
	}
}



void UUEExtendedMathLibrary::GetActorForwardVectorPlus(AActor* Actor, float Distance, FVector& CurrentLocation,FVector& ForwardLocation)
{
	if (Actor)
	{
		CurrentLocation = Actor->GetActorLocation();
		ForwardLocation= CurrentLocation + Actor->GetActorForwardVector()*Distance;
	}
}






// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< SCREEN >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
AActor* UUEExtendedMathLibrary::GetActorInTheCenterOfTheScreen(TMap<AActor*, float> ActorScreenMap, bool& Found)
{

	TArray<float> ValueArray;
	TArray<AActor*> ActorArray;
	ActorScreenMap.GetKeys(ActorArray);
	ActorScreenMap.GenerateValueArray(ValueArray);

	
	if (ValueArray.IsValidIndex(0))
	{
		
		const float SearchValue = 0.5;
		
		float ClosestIndex = 0;
		
		float subtract_result = FMath::Abs(ValueArray[0] - SearchValue) ;

		for (int32 i = 0 ; i < ValueArray.Num(); i++)
		{
			if (subtract_result > FMath::Abs(ValueArray[i] - SearchValue))
			{
				subtract_result = FMath::Abs(ValueArray[i] - SearchValue);
				
				ClosestIndex = i;
			}
		}

		if (ActorArray.IsValidIndex(ClosestIndex))
		{
			Found = true;
			return ActorArray[ClosestIndex];
		}
	}
	Found = false;
	return nullptr;
	
}



FVector2D UUEExtendedMathLibrary::GetObjectScreenPositionClamped(UObject* WorldContextObject, FVector Position)
{
	if (auto const PlayerController = UGameplayStatics::GetPlayerController(WorldContextObject,0))
	{
		FVector2D ScreenPosition;
		PlayerController->ProjectWorldLocationToScreen(Position,ScreenPosition);
		return	FVector2D
		(
			FMath::Clamp<float>(ScreenPosition.X/UWidgetLayoutLibrary::GetViewportSize(WorldContextObject).X, 0, 1) ,

			FMath::Clamp<float>(ScreenPosition.Y/UWidgetLayoutLibrary::GetViewportSize(WorldContextObject).Y, 0, 1) 
		);
	}
	return FVector2D();
}
