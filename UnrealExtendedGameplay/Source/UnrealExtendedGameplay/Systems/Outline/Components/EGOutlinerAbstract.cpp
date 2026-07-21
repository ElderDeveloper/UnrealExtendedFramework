// Fill out your copyright notice in the Description page of Project Settings.


#include "EGOutlinerAbstract.h"


// Sets default values for this component's properties
UEGOutlinerAbstract::UEGOutlinerAbstract()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UEGOutlinerAbstract::ServerSwitchOutlineOnActor_Implementation(AActor* ActorToOutline, EGOutlineBehavior,bool bEnable, EGOutlineType OutlineType)
{
}

void UEGOutlinerAbstract::ServerSpawnReplicatedSymbol_Implementation(FTransform SpawnTransform,FEGOutlineSymbolSettings SymbolSettings)
{
}

void UEGOutlinerAbstract::ServerDestroySpawnedSymbol_Implementation(FName Key)
{
}

void UEGOutlinerAbstract::ServerSetSymbolColor_Implementation(FName Key, FLinearColor Color)
{
}

void UEGOutlinerAbstract::ServerDestroySymbolByActor_Implementation(AActor* Actor)
{
}



void UEGOutlinerAbstract::SpawnClientOnlySymbol(FTransform SpawnTransform, FEGOutlineSymbolSettings SymbolSettings)
{
}

void UEGOutlinerAbstract::DestroyClientOnlySymbol(FName Key)
{
}

void UEGOutlinerAbstract::SetSymbolColorOnClient(FName Key, FLinearColor Color)
{
}

void UEGOutlinerAbstract::DestroySymbolByActor(AActor* Actor)
{
}

void UEGOutlinerAbstract::SwitchOutlineOnActor(AActor* ActorToOutline, EGOutlineBehavior OutlineBehavior, bool bEnable,EGOutlineType OutlineType)
{
}

void UEGOutlinerAbstract::SetUsingSceneDepth(bool bUseSceneDepth)
{
}

bool UEGOutlinerAbstract::IsUsingSceneDepth(AActor* Actor)
{
	return false;
}

bool UEGOutlinerAbstract::IsActorOutlined(AActor* Actor)
{
	return false;
}

bool UEGOutlinerAbstract::ActorHasSymbolAttached(AActor* Actor, AActor*& Symbol)
{
	Symbol = nullptr;
	return false;
}
