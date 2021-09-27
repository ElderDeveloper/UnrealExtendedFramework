// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedAILibrary.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"


bool UUEExtendedAILibrary::ExtendedGetBlackboardBool(AActor* OwningActor , FName KeyName)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
		return Blackboard->GetValueAsBool(KeyName);
	return false;
}

TSubclassOf<UObject> UUEExtendedAILibrary::ExtendedGetBlackboardClass(AActor* OwningActor, FName KeyName)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
		return Blackboard->GetValueAsClass(KeyName);
	return nullptr;
}

uint8 UUEExtendedAILibrary::ExtendedGetBlackboardEnum(AActor* OwningActor, FName KeyName)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
		return Blackboard->GetValueAsEnum(KeyName);
	return -1;
}

float UUEExtendedAILibrary::ExtendedGetBlackboardFloat(AActor* OwningActor, FName KeyName)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
		return Blackboard->GetValueAsFloat(KeyName);
	return 0;
}

int32 UUEExtendedAILibrary::ExtendedGetBlackboardInt(AActor* OwningActor, FName KeyName)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
		return Blackboard->GetValueAsInt(KeyName);
	return 0;
}

FName UUEExtendedAILibrary::ExtendedGetBlackboardName(AActor* OwningActor, FName KeyName)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
		return Blackboard->GetValueAsName(KeyName);
	return FName();
}

UObject* UUEExtendedAILibrary::ExtendedGetBlackboardObject(AActor* OwningActor, FName KeyName)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
		return Blackboard->GetValueAsObject(KeyName);
	return nullptr;
}

FRotator UUEExtendedAILibrary::ExtendedGetBlackboardRotator(AActor* OwningActor, FName KeyName)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
		return Blackboard->GetValueAsRotator(KeyName);
	return FRotator();
}

FString UUEExtendedAILibrary::ExtendedGetBlackboardString(AActor* OwningActor, FName KeyName)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
		return Blackboard->GetValueAsString(KeyName);
	return FString();
}

FVector UUEExtendedAILibrary::ExtendedGetBlackboardVector(AActor* OwningActor, FName KeyName)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
		return Blackboard->GetValueAsVector(KeyName);
	return FVector();
}






// <<<<<<<<<<<<<<<<<<<<<<<<<<<< BLACKBOARD SETTERS >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

bool UUEExtendedAILibrary::ExtendedSetBlackboardBool(AActor* OwningActor, FName KeyName, bool Value)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
	{
		Blackboard->SetValueAsBool(KeyName,Value);
		return true;
	}
	return false;
}

bool UUEExtendedAILibrary::ExtendedSetBlackboardClass(AActor* OwningActor, FName KeyName, TSubclassOf<UObject> Value)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
	{
		Blackboard->SetValueAsClass(KeyName,Value);
		return true;
	}
	return false;
}

bool UUEExtendedAILibrary::ExtendedSetBlackboardEnum(AActor* OwningActor, FName KeyName, uint8 Value)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
	{
		Blackboard->SetValueAsEnum(KeyName,Value);
		return true;
	}
	return false;
}

bool UUEExtendedAILibrary::ExtendedSetBlackboardFloat(AActor* OwningActor, FName KeyName, float Value)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
	{
		Blackboard->SetValueAsFloat(KeyName,Value);
		return true;
	}
	return false;
}

bool UUEExtendedAILibrary::ExtendedSetBlackboardInt(AActor* OwningActor, FName KeyName, int32 Value)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
	{
		Blackboard->SetValueAsInt(KeyName,Value);
		return true;
	}
	return false;
}

bool UUEExtendedAILibrary::ExtendedSetBlackboardName(AActor* OwningActor, FName KeyName, FName Value)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
	{
		Blackboard->SetValueAsName(KeyName,Value);
		return true;
	}
	return false;
}

bool UUEExtendedAILibrary::ExtendedSetBlackboardObject(AActor* OwningActor, FName KeyName, UObject* Value)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
	{
		Blackboard->SetValueAsObject(KeyName,Value);
		return true;
	}
	return false;
}

bool UUEExtendedAILibrary::ExtendedSetBlackboardRotator(AActor* OwningActor, FName KeyName, FRotator Value)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
	{
		Blackboard->SetValueAsRotator(KeyName,Value);
		return true;
	}
	return false;
}

bool UUEExtendedAILibrary::ExtendedSetBlackboardString(AActor* OwningActor, FName KeyName, FString Value)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
	{
		Blackboard->SetValueAsString(KeyName,Value);
		return true;
	}
	return false;
}

bool UUEExtendedAILibrary::ExtendedSetBlackboardVector(AActor* OwningActor, FName KeyName, FVector Value)
{
	if (const auto Blackboard = UAIBlueprintHelperLibrary::GetBlackboard(OwningActor))
	{
		Blackboard->SetValueAsVector(KeyName,Value);
		return true;
	}
	return false;
}
