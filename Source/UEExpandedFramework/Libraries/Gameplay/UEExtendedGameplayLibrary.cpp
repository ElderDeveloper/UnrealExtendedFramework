// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedGameplayLibrary.h"

#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"


 float UUEExtendedGameplayLibrary::GetActorLocationX(AActor* Actor)
{
	 if (Actor) return Actor->GetActorLocation().X;
	return 0;
}

 float UUEExtendedGameplayLibrary::GetActorLocationY(AActor* Actor)
{
	if (Actor) return Actor->GetActorLocation().Y;
	return 0;
}

 float UUEExtendedGameplayLibrary::GetActorLocationZ(AActor* Actor)
{
	if (Actor) return Actor->GetActorLocation().Z;
	return 0;
}

 float UUEExtendedGameplayLibrary::GetActorRotationYaw(AActor* Actor)
{
	if (Actor) return Actor->GetActorRotation().Yaw;
	return 0;
}

 float UUEExtendedGameplayLibrary::GetActorRotationPitch(AActor* Actor)
{
	if (Actor) return Actor->GetActorRotation().Pitch;
	return 0;
}

 float UUEExtendedGameplayLibrary::GetActorRotationRoll(AActor* Actor)
{
	if (Actor) return Actor->GetActorRotation().Roll;
	return 0;
}

void UUEExtendedGameplayLibrary::ExecuteFunction(UObject* RequestOwner , UObject* TargetObject, const FString FunctionToExecute)
{
	if(TargetObject && RequestOwner)
	{
		UE_LOG(LogTemp,Warning,TEXT("Request Owner: %s") , *RequestOwner->GetName());


		//FindFunction will return a pointer to a UFunction based on a
		//given FName. We use an asterisk before our FString in order to
		//convert the FString variable to FName
		if(const auto Function = TargetObject->FindFunction(*FunctionToExecute))
		{
			//The following pointer is a void pointer,
			//this means that it can point to anything - from raw memory to all the known types -
			void* locals = nullptr;
 
			//In order to execute our function we need to reserve a chunk of memory in 
			//the execution stack for it.
			if(const auto frame = new FFrame(RequestOwner, Function, locals))
			{
				//Unfortunately the source code of the engine doesn't explain what the locals
				//pointer is used for.
				//After some trial and error I ended up on this code which actually works without any problem.
		
				//Actual call of our UFunction
				TargetObject->CallFunction(*frame, locals, Function);
 
				//delete our pointer to avoid memory leaks!
			}
		}
	}
}

void UUEExtendedGameplayLibrary::RotateToObjectYaw(AActor* From, AActor* To)
{
	if (From && To)
	{
		FRotator Rot = From->GetActorRotation();
		Rot.Yaw = UKismetMathLibrary::FindLookAtRotation(From->GetActorLocation(),To->GetActorLocation()).Yaw;
		From->SetActorRotation(Rot);
	}
}




void UUEExtendedGameplayLibrary::RotateToObject(AActor* From, AActor* To)
{
	if (From && To)
	{
		From->SetActorRotation(UKismetMathLibrary::FindLookAtRotation(From->GetActorLocation(),To->GetActorLocation()));
	}
}




void UUEExtendedGameplayLibrary::RotateToObjectInterpYaw(const UObject* WorldContextObject, AActor* From, AActor* To,float InterpSpeed)
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




void UUEExtendedGameplayLibrary::RotateToObjectInterp(const UObject* WorldContextObject, AActor* From, AActor* To,float InterpSpeed)
{
	if (From && To)
	{
		From->SetActorRotation(UKismetMathLibrary::RInterpTo(
			From->GetActorRotation() ,
			UKismetMathLibrary::FindLookAtRotation(From->GetActorLocation(),To->GetActorLocation())
			, WorldContextObject->GetWorld()->GetDeltaSeconds() , InterpSpeed));
	}
}

float UUEExtendedGameplayLibrary::GetGameplayFramePerSecond(const UObject* WorldContextObject)
{
	if (const auto World = Cast<UWorld>(WorldContextObject))
		return (1/World->GetDeltaSeconds())* UGameplayStatics::GetGlobalTimeDilation(World);
 	return 0;
}
