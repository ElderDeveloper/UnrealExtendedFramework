// Fill out your copyright notice in the Description page of Project Settings.


#include "EFGlobalSubsystem.h"
#include "UnrealExtendedFramework/Data/EFMacro.h"


void UEFGlobalSubsystem::SetGlobalActor(FGameplayTag Tag, AActor* Actor)
{
	EFGlobalActors.Emplace(Tag,Actor);
}




AActor* UEFGlobalSubsystem::GetGlobalActor(FGameplayTag Tag , bool& Valid)
{
	if (EFGlobalActors.Contains(Tag))
	{
		Valid = IsValid(EFGlobalActors[Tag]);
		return EFGlobalActors[Tag];
	}
	EFGlobalActors.Add(Tag,nullptr);
	Valid = false;
	return nullptr;
}




void UEFGlobalSubsystem::SetGlobalComponent(FGameplayTag Tag, UPrimitiveComponent* Component)
{
	EFGlobalComponents.Add(Tag,Component);
}




UPrimitiveComponent* UEFGlobalSubsystem::GetGlobalComponent(FGameplayTag Tag , bool& Valid)
{
	if (EFGlobalComponents.Contains(Tag))
	{
		Valid = IsValid(EFGlobalComponents[Tag]);
		return EFGlobalComponents[Tag];
	}
	EFGlobalComponents.Add(Tag,nullptr);
	Valid = false;
	return nullptr;
}




void UEFGlobalSubsystem::SetGlobalObject(FGameplayTag Tag, UObject* Object)
{
	EFGlobalObjects.Add(Tag,Object);
}




UObject* UEFGlobalSubsystem::GetGlobalObject(FGameplayTag Tag , bool& Valid)
{
	if (EFGlobalObjects.Contains(Tag))
	{
		Valid = IsValid(EFGlobalObjects[Tag]);
		return EFGlobalObjects[Tag];
	}
	EFGlobalObjects.Add(Tag,nullptr);
	Valid = false;
	return nullptr;
}




void UEFGlobalSubsystem::GetAllGlobalActors(bool Print ,TArray<FGameplayTag>& Tags, TArray<AActor*>& Actors)
{
	if (EFGlobalActors.Num() == 0)
	{
		UE_LOG(LogTemp,Warning,TEXT("Global Actor Array Empty"));
		return;
	}
	
	for (const auto i : EFGlobalActors)
	{
		Tags.Add(i.Key);
		Actors.Add(i.Value);
		
		if (Print)
		{
			FString S = IsValid(i.Value)? i.Value->GetName() : "";
			PRINT_STRING(1 , Green , i.Key.ToString() + " : " + S );
		}
	}
}




void UEFGlobalSubsystem::GetAllGlobalComponents(bool Print ,TArray<FGameplayTag>& Tags, TArray<UPrimitiveComponent*>& Components)
{
	for (const auto i : EFGlobalComponents)
	{
		Tags.Add(i.Key);
		Components.Add(i.Value);
		
		if (Print)
		{
			FString S = IsValid(i.Value)? i.Value->GetName() : "";
			PRINT_STRING(1 , Green , i.Key.ToString() + " : " + S );
		}
	}
}




void UEFGlobalSubsystem::GetAllGlobalObjects(bool Print ,TArray<FGameplayTag>& Tags, TArray<UObject*>& Objects)
{
	for (const auto i : EFGlobalObjects)
	{
		Tags.Add(i.Key);
		Objects.Add(i.Value);
		
		if (Print)
		{
			FString S = IsValid(i.Value)? i.Value->GetName() : "";
			PRINT_STRING(1 , Green , i.Key.ToString() + " : " + S );
		}
	}
}
