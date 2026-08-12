// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EGOutlinerBasic.h"
#include "EGOutlineAdvanced.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGOutlineAdvanced : public UEGOutlinerBasic
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UEGOutlineAdvanced();

protected:

	virtual void ServerSpawnReplicatedSymbol(FTransform SpawnTransform, FEGOutlineSymbolSettings SymbolSettings) override;
	
	virtual void ServerDestroySpawnedSymbol(FName Key) override;

	
	virtual void SpawnClientOnlySymbol(FTransform SpawnTransform , FEGOutlineSymbolSettings SymbolSettings) override;
	
	virtual void DestroyClientOnlySymbol(FName Key) override;
	
	virtual void SetSymbolColorOnClient(FName Key, FLinearColor Color) override;
	
	virtual void ServerSetSymbolColor(FName Key, FLinearColor Color) override;
	
	virtual void DestroySymbolByActor(AActor* Actor) override;
	
	virtual void ServerDestroySymbolByActor(AActor* Actor) override;


	virtual void SetUsingSceneDepth(bool bUseSceneDepth) override;

	virtual bool IsUsingSceneDepth(AActor* Actor) override;

	virtual bool IsActorOutlined(AActor* Actor) override;

	virtual bool ActorHasSymbolAttached(AActor* Actor, AActor*& Symbol) override;
	
	// Called when the game starts
	virtual void BeginPlay() override;
	
};
