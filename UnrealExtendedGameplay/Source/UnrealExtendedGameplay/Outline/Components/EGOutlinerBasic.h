// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EGOutlinerAbstract.h"
#include "Components/ActorComponent.h"
#include "EGOutlinerBasic.generated.h"


class AEGOutlinerWorldActor;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGOutlinerBasic : public UEGOutlinerAbstract
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UEGOutlinerBasic();

	UPROPERTY(BlueprintReadWrite , Category = "Outliner")
	AEGOutlinerWorldActor* OutlinerWorldActor;

	
	UPROPERTY(EditAnywhere , BlueprintReadWrite , Category = "Beginplay")
	bool bOutlineOnBeginPlay = false;

	UPROPERTY(EditAnywhere , BlueprintReadWrite , Category = "Beginplay")
	EGOutlineBehavior OwnerOutlineBehavior;

	UPROPERTY(EditAnywhere , BlueprintReadWrite , Category = "Beginplay")
	EGOutlineType OwnerOutlineType;

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void SwitchOutlineOnActor(AActor* ActorToOutline, EGOutlineBehavior OutlineBehavior, bool bEnable, EGOutlineType OutlineType) override;
	virtual void ServerSwitchOutlineOnActor(AActor* ActorToOutline, EGOutlineBehavior, bool bEnable, EGOutlineType OutlineType) override;


	

};
