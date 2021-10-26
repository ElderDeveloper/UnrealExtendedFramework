// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "UEExtendedDistanceBetweenContextsTest.generated.h"



UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedDistanceBetweenContextsTest : public UEnvQueryTest
{
	GENERATED_UCLASS_BODY()


	
	UPROPERTY(EditDefaultsOnly, Category=Distance)
	float MinDistance = 50.f;

	/**
	 * 0 = Infinite
	 **/
	UPROPERTY(EditDefaultsOnly, Category=Distance)
	float MaxDistance = 0.f;

	UPROPERTY(EditDefaultsOnly, Category=Distance)
	float PlusDistance = 50.f;
	
	UPROPERTY(EditDefaultsOnly, Category=Distance)
	bool ToPlayer = false;

	UPROPERTY(EditDefaultsOnly, Category=Distance)
	TSubclassOf<AActor> SearchActor;

	void FindActor(AActor*& Actor);
	
	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

};
