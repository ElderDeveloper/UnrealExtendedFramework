// Fill out your copyright notice in the Description page of Project Settings.


#include "EFMathLibrary.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"


// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< ROTATION >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
FRotator UEFMathLibrary::GetRotationBetweenActors(const AActor* From, const AActor* To, const FRotator PlusRotator)
{
	if (IsValid(From) && IsValid(To))
	{
		return UKismetMathLibrary::FindLookAtRotation(From->GetActorLocation(),To->GetActorLocation())+PlusRotator;
	}	return FRotator::ZeroRotator;
}



void UEFMathLibrary::GetAngleBetweenActors(AActor* From, AActor* To, float& Yaw, float& Pitch)
{
	if (IsValid(From) && IsValid(To))
	{
		const FRotator Angle = FRotator(UKismetMathLibrary::NormalizedDeltaRotator(From->GetActorRotation(),GetRotationBetweenActors(From,To)));
		Yaw = Angle.Yaw; Pitch = Angle.Pitch;
	}
}






// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< DISTANCE >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
float UEFMathLibrary::GetDistanceBetweenActors(const AActor* From, const AActor* To)
{
	if (IsValid(From) && IsValid(To))
	{
		return FVector(From->GetActorLocation()-To->GetActorLocation()).Size();
	}
	return 0;
}



float UEFMathLibrary::GetDistanceBetweenComponents(const USceneComponent* From, const USceneComponent* To)
{
	if (From && To)
	{
		return FVector(From->GetComponentLocation()-To->GetComponentLocation()).Size();
	}
	return 0;
}



float UEFMathLibrary::GetDistanceBetweenComponentAndActor(const USceneComponent* From, const AActor* To)
{
	if (From && To)
	{
		return FVector(From->GetComponentLocation()-To->GetActorLocation()).Size();
	}
	return 0;
}



float UEFMathLibrary::GetDistanceBetweenVectors(const FVector From, const FVector To)
{
	return FVector(From-To).Size();
}



float UEFMathLibrary::GetDistanceBetweenVectorsNoSquareRoot(const FVector From, const FVector To)
{
	const auto i = FVector(From-To);
	return  (i.X*i.X + i.Y*i.Y + i.Z*i.Z);
}

AActor* UEFMathLibrary::GetClosestActorFromActorArray(const AActor* OwnerActor ,UPARAM(ref) const TArray<AActor*>& TargetArray)
{
	float ClosestDistance = 999999999;
	AActor* ClosestActor = nullptr;
	if (TargetArray.IsValidIndex(0))
		ClosestActor = TargetArray[0];
	
	for (const auto i : TargetArray)
	{
		const float dist = GetDistanceBetweenActors(OwnerActor,i);
		if (dist < ClosestDistance)
		{
			ClosestDistance = dist;		ClosestActor = i;
		}
	}
	return ClosestActor;
}

void UEFMathLibrary::GetClosestComponentFromComponentArray(const AActor* OwnerActor,const TArray<USceneComponent*>& TargetArray, USceneComponent*& Item)
{
	float ClosestDistance = 999999999;
	if (TargetArray.IsValidIndex(0))
		Item = TargetArray[0];
	
	for (const auto i : TargetArray)
	{
		const float dist = GetDistanceBetweenComponentAndActor(i,OwnerActor);
		if (dist < ClosestDistance)
		{
			ClosestDistance = dist;		Item = i;
		}
	}
}

void UEFMathLibrary::ComponentGetClosestActorFromActorArray(const USceneComponent* OwnerComponent,const TArray<AActor*>& TargetArray, AActor*& Item)
{
	float ClosestDistance = 999999999;
	if (TargetArray.IsValidIndex(0))
		Item = TargetArray[0];
	
	for (const auto i : TargetArray)
	{
		const float dist = GetDistanceBetweenComponentAndActor(OwnerComponent,i);
		if (dist < ClosestDistance)
		{
			ClosestDistance = dist;		Item = i;
		}
	}
}

void UEFMathLibrary::ComponentGetClosestComponentFromComponentArray(const USceneComponent* OwnerComponent,const TArray<USceneComponent*>& TargetArray, USceneComponent*& Item)
{
	float ClosestDistance = 999999999;
	if (TargetArray.IsValidIndex(0))
		Item = TargetArray[0];
	
	for (const auto i : TargetArray)
	{
		const float dist = GetDistanceBetweenComponents(OwnerComponent,i);
		if (dist < ClosestDistance)
		{
			ClosestDistance = dist;		Item = i;
		}
	}
}


// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< DIRECTION >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
FVector UEFMathLibrary::GetDirectionBetweenActors(const AActor* From, const AActor* To, float scaleVector)
{
	if (From && To)
	{
		return (FVector(To->GetActorLocation() - From->GetActorLocation()).GetSafeNormal())*scaleVector;
	}
	return FVector::ZeroVector;
}

/*

TEnumAsByte<EHitDirection> UEFMathLibrary::CalculateHitDirectionYaw(float Yaw, float& Angle)
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



TEnumAsByte<EHitDirection> UEFMathLibrary::CalculateHitDirectionActors(AActor* From, AActor* To,float& Angle)
{
	if (IsValid(From) && IsValid(To))
	{
		float pitch;
		GetAngleBetweenActors(From,To,Angle,pitch);
		return CalculateHitDirectionYaw(Angle,pitch);
	}
	return No;
}
*/



FVector UEFMathLibrary::GetComponentForwardVectorPlus(USceneComponent* Component, float Distance, FVector& CurrentLocation)
{
	if (Component)
	{
		CurrentLocation = Component->GetComponentLocation();
		return  CurrentLocation + Component->GetForwardVector()*Distance;
	}
	return FVector::ZeroVector;
}



void UEFMathLibrary::GetActorForwardVectorPlus(AActor* Actor, float Distance, FVector& CurrentLocation,FVector& ForwardLocation)
{
	if (Actor)
	{
		CurrentLocation = Actor->GetActorLocation();
		ForwardLocation= CurrentLocation + Actor->GetActorForwardVector()*Distance;
	}
}


FVector UEFMathLibrary::FCalculateDirectionalLocation(const FVector targetLocation,const FVector startPosition, float distance, bool forward)
{
	const FVector minus = targetLocation - startPosition;
	return forward ? (minus.GetSafeNormal()*distance) + startPosition : (minus.GetSafeNormal()*distance) - startPosition ;
}


bool UEFMathLibrary::FCalculateIsLookingAt(const FVector actorForward, const FVector target, const FVector start, float& returnAngle,float limit)
{
	const FVector minus = target-start;
	const FVector plus ( actorForward.X*minus.X ,actorForward.Y*minus.Y ,actorForward.Z*minus.Z );
	returnAngle = UKismetMathLibrary::Acos((plus.X + plus.Y + plus.Z)	/ (actorForward.Size() * minus.Size()));
	return returnAngle < limit;
}

float UEFMathLibrary::FindLookAtRotationYaw(const FVector& Start, const FVector& Target)
{
	return FRotationMatrix::MakeFromX(Target - Start).Rotator().Yaw;
}

bool UEFMathLibrary::FCalculateIsTheSameDirection(const FVector firstForwardDirection,const FVector secondForwardDirection, const float tolerance=0.1)
{
	return UKismetMathLibrary::EqualEqual_VectorVector(firstForwardDirection.GetUnsafeNormal(),secondForwardDirection.GetUnsafeNormal(),tolerance);
}

uint8 UEFMathLibrary::CalculateDirectionBetweenActors(const AActor* Target, const AActor* From , const float ForwardTolerance)
{
	if (Target && From)
	{
		const float FrontDirection = Target->GetDotProductTo(From);
		
		if (FrontDirection > ForwardTolerance* -1 && FrontDirection < ForwardTolerance)
		{
			// 2 = LEFT , 3 = RIGHT
			return 	FVector::DotProduct(Target->GetActorRightVector(), From->GetActorForwardVector()) > 0 ?  2 : 3;
		}
		else
		{
			// 0 = FRONT , 1 = BACK
			return FrontDirection > 0 ? 0 : 1 ;
		}
	}
	return -1;
}


uint8 UEFMathLibrary::GetControllerLookAtDirection(const APawn* Pawn)
{
	if (Pawn)
	{
		const FVector ControllerForward = Pawn->Controller->GetControlRotation().Vector();
		const float ForwardDot = FVector::DotProduct(ControllerForward ,Pawn->GetActorForwardVector());

		if(FMath::IsNearlyEqual(ForwardDot, 1.f, 0.1f))	return 0;
		if(FMath::IsNearlyEqual(ForwardDot,-1.f,0.1f)) return 1;
		
		const float RightDot = FVector::DotProduct(ControllerForward , Pawn->GetActorRightVector());

		if(FMath::IsNearlyEqual(RightDot, 1.f, 0.1f))	return 2;
		if(FMath::IsNearlyEqual(RightDot,-1.f,0.1f)) return 3;

		return 4;
	}
	return 4;
}


// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< SCREEN >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
AActor* UEFMathLibrary::GetActorInTheCenterOfTheScreen(TMap<AActor*, float> ActorScreenMap , const FVector2D ClampMinMax)
{

	TArray<float> ValueArray;
	TArray<AActor*> ActorArray;
	ActorScreenMap.GetKeys(ActorArray);
	ActorScreenMap.GenerateValueArray(ValueArray);

	for (int32 i = 0 ; i<ActorArray.Num() ; i++)
	{
		if (ValueArray.IsValidIndex(i))
		{
			const bool Validation = ValueArray[i] > ClampMinMax.X && ValueArray[i] < ClampMinMax.Y;
			if (!Validation)
			{
				ValueArray.RemoveAt(i);
				ActorArray.RemoveAt(i);
			}
		}
	}
	
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
	
			return ActorArray[ClosestIndex];
		}
	}
	return nullptr;
	
}



FVector2D UEFMathLibrary::GetObjectScreenPositionClamped(UObject* WorldContextObject, FVector Position)
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




// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< LOCATION >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

FVector UEFMathLibrary::FindRandomCircleLocation(float innerRadius, float outerRadius, FVector centerPont,	FVector forwardVector)
{
	const float rndAngle = UKismetMathLibrary::RandomFloatInRange(0,360);

	const float rndDistance = UKismetMathLibrary::RandomFloatInRange(innerRadius,outerRadius);

	const FVector Direction = UKismetMathLibrary::GreaterGreater_VectorRotator(forwardVector,FRotator(0,rndAngle,0))*rndDistance;

	return  centerPont+Direction;

}

FVector UEFMathLibrary::FindRandomCircleLocationWithDirection(float innerRadius, float outerRadius,FVector centerPont, FVector targetPoint, float angle)
{
	const float rndAngle = UKismetMathLibrary::RandomFloatInRange(angle*-1,angle);
	const float rndDistance = UKismetMathLibrary::RandomFloatInRange(innerRadius,outerRadius);
	const FVector Direction = UKismetMathLibrary::GreaterGreater_VectorRotator(FVector(targetPoint-centerPont).GetSafeNormal(),FRotator(0,rndAngle,0))*rndDistance;
	return  centerPont+Direction;
}






// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< PHYSICS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

FVector UEFMathLibrary::FCalculateLaunchVelocity(const FVector targetLocation, const FVector startPosition, const float duration)
{
	FVector returnVector;

	returnVector.X = (targetLocation.X - startPosition.X) / duration;
	
	returnVector.Y = (targetLocation.Y - startPosition.Y) / duration;
	
	const float zVelocity = (duration*duration) * -0.5 * 982;
	
	returnVector.Z = (targetLocation.Z - (startPosition.Z + zVelocity)) / duration;
	
	return returnVector;
}


FQuat UEFMathLibrary::RotatorToQuad(const FRotator Rotator)
{
	
	const float SinRoll =	FMath::Sin(FMath::DegreesToRadians(Rotator.Roll) / 2);
	const float CosRoll =	FMath::Cos(FMath::DegreesToRadians(Rotator.Roll) / 2);
	
	const float SinPitch =  FMath::Sin(FMath::DegreesToRadians(Rotator.Pitch) / 2);
	const float CosPitch =  FMath::Cos(FMath::DegreesToRadians(Rotator.Pitch) / 2);

	const float SinYaw =	FMath::Sin(FMath::DegreesToRadians(Rotator.Yaw) / 2);
	const float CosYaw =	FMath::Cos(FMath::DegreesToRadians(Rotator.Yaw) / 2);

	const float X = (CosRoll * SinPitch * SinYaw) - (SinRoll * CosPitch * CosYaw);
	
	const float Y = (CosRoll * SinPitch * CosYaw * -1) - (SinRoll * CosPitch * SinYaw);
	
	const float Z = (CosRoll * CosPitch * SinYaw) - (SinRoll * SinPitch * CosYaw);
	
	const float W = (CosRoll * CosPitch * CosYaw) + (SinRoll * SinPitch * SinYaw);
	

	return FQuat(X,Y,Z,W);
}

//<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< MATH >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
FVector2D UEFMathLibrary::ClampVector2D(const FVector2D Vector , const float Min, const float Max)
{
	return FVector2D(FMath::Clamp(Vector.X , Min , Max) , FMath::Clamp(Vector.Y,Min,Max));
}

FVector2D UEFMathLibrary::MapRangeClampVector2D(const FVector2D Value, const FVector2D InRangeA,const FVector2D InRangeB, const FVector2D OutRangeA, const FVector2D OutRangeB)
{
	return FVector2D
	(
		UKismetMathLibrary::MapRangeClamped(Value.X , InRangeA.X,InRangeB.X , OutRangeA.X,OutRangeB.X),
		UKismetMathLibrary::MapRangeClamped(Value.Y , InRangeA.Y,InRangeB.Y , OutRangeA.Y,OutRangeB.Y)
	);
}
