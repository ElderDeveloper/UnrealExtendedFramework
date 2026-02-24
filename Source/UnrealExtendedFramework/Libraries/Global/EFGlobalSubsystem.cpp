// Fill out your copyright notice in the Description page of Project Settings.


#include "EFGlobalSubsystem.h"
#include "UnrealExtendedFramework/Data/EFMacro.h"


void UEFGlobalSubsystem::SetGlobalActor(FGameplayTag Tag, AActor* Actor)
{
	EFGlobalActors.Emplace(Tag, Actor);
}

AActor* UEFGlobalSubsystem::GetGlobalActor(FGameplayTag Tag, bool& Valid)
{
	if (AActor** Found = EFGlobalActors.Find(Tag))
	{
		Valid = IsValid(*Found);
		return *Found;
	}
	// BUG FIX: Previously auto-inserted a nullptr entry for missing tags (map pollution).
	// Now returns nullptr without modifying the map.
	Valid = false;
	return nullptr;
}

bool UEFGlobalSubsystem::RemoveGlobalActor(FGameplayTag Tag)
{
	return EFGlobalActors.Remove(Tag) > 0;
}

bool UEFGlobalSubsystem::ContainsGlobalActor(FGameplayTag Tag) const
{
	return EFGlobalActors.Contains(Tag);
}

void UEFGlobalSubsystem::ClearAllGlobalActors()
{
	EFGlobalActors.Empty();
}

void UEFGlobalSubsystem::SetGlobalObject(FGameplayTag Tag, UObject* Object)
{
	EFGlobalObjects.Emplace(Tag, Object);
}

UObject* UEFGlobalSubsystem::GetGlobalObject(FGameplayTag Tag, bool& Valid)
{
	if (UObject** Found = EFGlobalObjects.Find(Tag))
	{
		Valid = IsValid(*Found);
		return *Found;
	}
	// BUG FIX: Same map pollution fix as GetGlobalActor.
	Valid = false;
	return nullptr;
}

bool UEFGlobalSubsystem::RemoveGlobalObject(FGameplayTag Tag)
{
	return EFGlobalObjects.Remove(Tag) > 0;
}

bool UEFGlobalSubsystem::ContainsGlobalObject(FGameplayTag Tag) const
{
	return EFGlobalObjects.Contains(Tag);
}

void UEFGlobalSubsystem::ClearAllGlobalObjects()
{
	EFGlobalObjects.Empty();
}

void UEFGlobalSubsystem::GetAllGlobalActors(bool Print, TArray<FGameplayTag>& Tags, TArray<AActor*>& Actors)
{
	if (EFGlobalActors.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Global Actor Array Empty"));
		return;
	}
	
	// BUG FIX: Use const reference to avoid copying each TMap entry
	for (const auto& i : EFGlobalActors)
	{
		Tags.Add(i.Key);
		Actors.Add(i.Value);
		
		if (Print)
		{
			FString S = IsValid(i.Value) ? i.Value->GetName() : "";
			PRINT_STRING(1, Green, i.Key.ToString() + " : " + S);
		}
	}
}

void UEFGlobalSubsystem::GetAllGlobalObjects(bool Print, TArray<FGameplayTag>& Tags, TArray<UObject*>& Objects)
{
	// BUG FIX: Use const reference to avoid copying each TMap entry
	for (const auto& i : EFGlobalObjects)
	{
		Tags.Add(i.Key);
		Objects.Add(i.Value);
		
		if (Print)
		{
			FString S = IsValid(i.Value) ? i.Value->GetName() : "";
			PRINT_STRING(1, Green, i.Key.ToString() + " : " + S);
		}
	}
}
