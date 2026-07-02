// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EFEQSTest_HeightDifference.generated.h"

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFEQSTest_HeightDifference : public UEnvQueryTest
{
	GENERATED_BODY()

public:
	UEFEQSTest_HeightDifference(const FObjectInitializer& ObjectInitializer);

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

protected:
	/** Context to measure height difference to */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	TSubclassOf<UEnvQueryContext> HeightReferenceTo;

	/** If true, absolute height difference will be used */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	bool bUseAbsoluteValue;
};
