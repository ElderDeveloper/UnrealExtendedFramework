// Fill out your copyright notice in the Description page of Project Settings.


#include "EGOutlineAdvanced.h"
#include "UnrealExtendedGameplay/Systems/Outline/Actors/EGOutlinerWorldActor.h"
#include "UnrealExtendedGameplay/Systems/Outline/Actors/EGOutlineSymbol.h"


// Sets default values for this component's properties
UEGOutlineAdvanced::UEGOutlineAdvanced()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UEGOutlineAdvanced::ServerSpawnReplicatedSymbol(FTransform SpawnTransform, FEGOutlineSymbolSettings SymbolSettings)
{
	Super::ServerSpawnReplicatedSymbol(SpawnTransform, SymbolSettings);

	const auto Symbol =  GetWorld()->SpawnActor<AEGOutlineSymbol>(AEGOutlineSymbol::StaticClass(), SpawnTransform);
	Symbol->InitializeSymbol(SymbolSettings, true);
	SpawnedSymbols.Add(SymbolSettings.SpawnedActorName, Symbol);
	
}

void UEGOutlineAdvanced::ServerDestroySpawnedSymbol(FName Key)
{
	Super::ServerDestroySpawnedSymbol(Key);
	
	if (const auto Symbol = *SpawnedSymbols.Find(Key))
	{
		Symbol->Destroy();
	}
}

void UEGOutlineAdvanced::SpawnClientOnlySymbol(FTransform SpawnTransform , FEGOutlineSymbolSettings SymbolSettings)
{
	Super::SpawnClientOnlySymbol(SpawnTransform , SymbolSettings);
	
	const auto Symbol =  GetWorld()->SpawnActor<AEGOutlineSymbol>(AEGOutlineSymbol::StaticClass(), SpawnTransform);
	Symbol->InitializeSymbol(SymbolSettings, false);
	SpawnedSymbols.Add(SymbolSettings.SpawnedActorName, Symbol);
}

void UEGOutlineAdvanced::DestroyClientOnlySymbol(FName Key)
{
	Super::DestroyClientOnlySymbol(Key);

	if (const auto Symbol = *SpawnedSymbols.Find(Key))
	{
		Symbol->Destroy();
	}
}

void UEGOutlineAdvanced::SetSymbolColorOnClient(FName Key, FLinearColor Color)
{
	Super::SetSymbolColorOnClient(Key, Color);

	if (const auto Symbol = *SpawnedSymbols.Find(Key))
	{
		Symbol->SetSymbolColor(Color);
	}
}

void UEGOutlineAdvanced::ServerSetSymbolColor(FName Key, FLinearColor Color)
{
	Super::ServerSetSymbolColor(Key, Color);
	
	if (const auto Symbol = *SpawnedSymbols.Find(Key))
	{
		Symbol->SetSymbolColor(Color);
	}
}

void UEGOutlineAdvanced::DestroySymbolByActor(AActor* Actor)
{
	Super::DestroySymbolByActor(Actor);
	if (Actor)
	{
		Actor->Destroy();
	}
}

void UEGOutlineAdvanced::ServerDestroySymbolByActor(AActor* Actor)
{
	Super::ServerDestroySymbolByActor(Actor);

	if (Actor)
	{
		Actor->Destroy();
	}
}

void UEGOutlineAdvanced::SetUsingSceneDepth(bool bUseSceneDepth)
{
	Super::SetUsingSceneDepth(bUseSceneDepth);

	if (OutlinerWorldActor)
	{
		OutlinerWorldActor->SetUsingSceneDepth(bUseSceneDepth);
	}
}

bool UEGOutlineAdvanced::IsUsingSceneDepth(AActor* Actor)
{
	return Super::IsUsingSceneDepth(Actor);
}

bool UEGOutlineAdvanced::IsActorOutlined(AActor* Actor)
{
	if (Actor)
	{
		TArray<UPrimitiveComponent*> Components;
		Actor->GetComponents<UPrimitiveComponent>(Components, true);
		
		for (const auto Component : Components)
		{
			if (Component->bRenderCustomDepth)
			{
				return true;
			}
		}
	}
	return false;
}

bool UEGOutlineAdvanced::ActorHasSymbolAttached(AActor* Actor, AActor*& Symbol)
{
	if (Actor)
	{
		TArray<AActor*> AttachedActors;
		Actor->GetAttachedActors(AttachedActors, true);

		for (const auto AttachedActor : AttachedActors)
		{
			if (AttachedActor->IsA(AEGOutlineSymbol::StaticClass()))
			{
				Symbol = AttachedActor;
				return true;
			}
		}
	}
	Symbol = nullptr;
	return false;
}


// Called when the game starts
void UEGOutlineAdvanced::BeginPlay()
{
	Super::BeginPlay();
	SetUsingSceneDepth(UseSceneDepthTest);
}

