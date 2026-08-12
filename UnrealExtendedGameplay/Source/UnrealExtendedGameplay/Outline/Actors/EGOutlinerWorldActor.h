// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EGOutlinerWorldActor.generated.h"

class UPostProcessComponent;

UCLASS()
class UNREALEXTENDEDGAMEPLAY_API AEGOutlinerWorldActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AEGOutlinerWorldActor();

	UPROPERTY(EditAnywhere , BlueprintReadWrite ,  Category = "Outliner")
	UPostProcessComponent* OutlinerPostProcessComponent;

	UFUNCTION(BlueprintCallable , Category = "Outliner")
	void SetUsingSceneDepth(bool bUseSceneDepth);

protected:
	UPROPERTY()
	UObject* SceneDepthMaterial;

	UPROPERTY()
	UObject* NonSceneDepthMaterial;
};
