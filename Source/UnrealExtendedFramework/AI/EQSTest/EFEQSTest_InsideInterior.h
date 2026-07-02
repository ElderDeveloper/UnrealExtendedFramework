// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EFEQSTest_InsideInterior.generated.h"

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFEQSTest_InsideInterior : public UEnvQueryTest
{
	GENERATED_BODY()

public:
	UEFEQSTest_InsideInterior(const FObjectInitializer& ObjectInitializer);

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

protected:
	/** Context that provides the Interior actors */
	UPROPERTY(EditDefaultsOnly, Category=Test)
	TSubclassOf<UEnvQueryContext> InteriorContext;
};
