// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UnrealExtendedGameplay/Systems/Outline/Data/EGOutlineData.h"
#include "EGOutlineSymbol.generated.h"

UCLASS()
class UNREALEXTENDEDGAMEPLAY_API AEGOutlineSymbol : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AEGOutlineSymbol();
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite , Category = "Outliner")
	USceneComponent* RootScene;;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite , Category = "Outliner")
	UStaticMeshComponent* SymbolMeshComponent;

	UPROPERTY(EditAnywhere , BlueprintReadWrite , Replicated , Category = "Outliner")
	FEGOutlineSymbolSettings SymbolSettings;

	UPROPERTY(EditAnywhere , BlueprintReadWrite , Category = "Outliner")
	bool ShouldReplicate = false;

 	UPROPERTY(EditAnywhere , BlueprintReadWrite , ReplicatedUsing="OnRep_Color",  Category = "Outliner")
	FLinearColor SymbolColor;

	UPROPERTY(EditAnywhere , BlueprintReadWrite , Category = "Outliner")
	float CurrentUpDownTime = 0.0f;
	
	UFUNCTION(BlueprintCallable , Category = "Outliner")
	void InitializeSymbol(FEGOutlineSymbolSettings NewSymbolSettings , bool bShouldReplicate);

	UFUNCTION(BlueprintCallable , Category = "Outliner")
	void SetSymbolColor(FLinearColor NewColor);
	
protected:

	
	UFUNCTION()
	void OnRep_Color();
	
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
