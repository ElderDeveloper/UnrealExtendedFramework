// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DataProviders/AIDataProvider.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "EFEQSGenerator_InteriorGrid.generated.h"

class UNavigationSystemV1;

UCLASS(meta = (DisplayName = "Points: Interior Grid"))
class UNREALEXTENDEDFRAMEWORK_API UEFEQSGenerator_InteriorGrid : public UEnvQueryGenerator
{
	GENERATED_BODY()

public:
	UEFEQSGenerator_InteriorGrid(const FObjectInitializer& ObjectInitializer);

	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

protected:
	/** context to find the interior actors */
	UPROPERTY(EditDefaultsOnly, Category=Generator)
	TSubclassOf<UEnvQueryContext> GenerateIn;

	/** Distance between grid points */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
	FAIDataProviderFloatValue SpaceBetween;

	/** Max vertical distance from NavMesh to accept a projected point */
	UPROPERTY(EditDefaultsOnly, Category = Generator)
	FAIDataProviderFloatValue ProjectionVerticalOffset;
};
