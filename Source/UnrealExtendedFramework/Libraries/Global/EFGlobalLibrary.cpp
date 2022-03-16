// Fill out your copyright notice in the Description page of Project Settings.


#include "EFGlobalLibrary.h"

#include "EFGlobalSubsystem.h"


AActor* UEFGlobalLibrary::GetGlobalActor(const UObject* WorldContextObject, FGameplayTag Tag , bool& Valid)
{
	if (const auto world = GEngine->GetEngineSubsystem<UEFGlobalSubsystem>()) 
	{
		return world->GetGlobalActor(Tag , Valid);
	}
	Valid = false;
	return nullptr;
}

void UEFGlobalLibrary::SetGlobalActor(const UObject* WorldContextObject, FGameplayTag Tag, AActor* Value)
{
	if (const auto world = GEngine->GetEngineSubsystem<UEFGlobalSubsystem>()) 
	{
		return world->SetGlobalActor(Tag , Value);
	}
}





UPrimitiveComponent* UEFGlobalLibrary::GetGlobalComponent(const UObject* WorldContextObject, FGameplayTag Tag , bool& Valid)
{
	
	if (const auto world  = GEngine->GetEngineSubsystem<UEFGlobalSubsystem>()) 
	{
		return world->GetGlobalComponent(Tag , Valid);
	}
	Valid = false;
	return nullptr;
}

void UEFGlobalLibrary::SetGlobalComponent(const UObject* WorldContextObject, FGameplayTag Tag,UPrimitiveComponent* Value)
{
	if (const auto world  = GEngine->GetEngineSubsystem<UEFGlobalSubsystem>()) 
	{
		return world->SetGlobalComponent(Tag , Value);
	}
}




UObject* UEFGlobalLibrary::GetGlobalObject(const UObject* WorldContextObject, FGameplayTag Tag , bool& Valid)
{
	if (const auto world  = GEngine->GetEngineSubsystem<UEFGlobalSubsystem>()) 
	{
		return world->GetGlobalObject(Tag , Valid);
	}
	Valid = false;
	return nullptr;
}

void UEFGlobalLibrary::SetGlobalObject(const UObject* WorldContextObject, FGameplayTag Tag, UObject* Value)
{
	if (const auto world = GEngine->GetEngineSubsystem<UEFGlobalSubsystem>()) 
	{
		return world->SetGlobalObject(Tag , Value);
	}
}
