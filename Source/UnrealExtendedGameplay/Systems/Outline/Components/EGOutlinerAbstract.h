// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UnrealExtendedGameplay/Systems/Outline/Data/EGOutlineData.h"
#include "EGOutlinerAbstract.generated.h"


class AEGOutlineSymbol;

UCLASS(Abstract)
class UNREALEXTENDEDGAMEPLAY_API UEGOutlinerAbstract : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UEGOutlinerAbstract();

	 // Server Only

	UPROPERTY(EditAnywhere , BlueprintReadWrite , Category = "Outliner")
	bool UseSceneDepthTest = false;

	UPROPERTY(EditAnywhere , BlueprintReadWrite , Category = "Outliner")
	TMap<FName , AEGOutlineSymbol*> SpawnedSymbols;
	
	UFUNCTION(BlueprintCallable ,  Server , Reliable , Category = "Outliner" )
	virtual void ServerSwitchOutlineOnActor(AActor* ActorToOutline ,EGOutlineBehavior OutlineBehavior , bool bEnable , EGOutlineType OutlineType = EGOutlineType::Primary);

	UFUNCTION(BlueprintCallable ,  Server , Reliable , Category = "Outliner" )
	virtual void ServerSpawnReplicatedSymbol(FTransform SpawnTransform , FEGOutlineSymbolSettings SymbolSettings);

	UFUNCTION(BlueprintCallable ,  Server , Reliable , Category = "Outliner" )
	virtual void ServerDestroySpawnedSymbol(FName Key);
	
	UFUNCTION(BlueprintCallable ,  Server , Reliable , Category = "Outliner" )
	virtual void ServerSetSymbolColor(FName Key , FLinearColor Color);
	
	UFUNCTION(BlueprintCallable ,  Server , Reliable , Category = "Outliner" )
	virtual void ServerDestroySymbolByActor(AActor* Actor);

	// Client only

	UFUNCTION(BlueprintCallable , Category = "Outliner" )
	virtual void SpawnClientOnlySymbol(FTransform SpawnTransform , FEGOutlineSymbolSettings SymbolSettings);

	UFUNCTION(BlueprintCallable , Category = "Outliner" )
	virtual void DestroyClientOnlySymbol(FName Key);

	UFUNCTION(BlueprintCallable , Category = "Outliner" )
	virtual void SetSymbolColorOnClient(FName Key , FLinearColor Color);

	UFUNCTION(BlueprintCallable , Category = "Outliner" )
	virtual void DestroySymbolByActor(AActor* Actor);


	// Functions

	UFUNCTION(BlueprintCallable , Category = "Outliner" )
	virtual void SwitchOutlineOnActor(AActor* ActorToOutline ,EGOutlineBehavior OutlineBehavior, bool bEnable , EGOutlineType OutlineType = EGOutlineType::Primary);

	UFUNCTION(BlueprintCallable , Category = "Outliner" )
	virtual void SetUsingSceneDepth(bool bUseSceneDepth);

	UFUNCTION(BlueprintPure , Category = "Outliner" )
	virtual bool IsUsingSceneDepth(AActor* Actor);

	UFUNCTION(BlueprintPure , Category = "Outliner" )
	virtual bool IsActorOutlined(AActor* Actor);

	UFUNCTION(BlueprintPure , Category = "Outliner" )
	virtual bool ActorHasSymbolAttached(AActor* Actor , AActor*& Symbol);
	
};
