// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EFEQSTest_ConeShape.generated.h"

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFEQSTest_ConeShape : public UEnvQueryTest
{
	GENERATED_BODY()

public:
	UEFEQSTest_ConeShape(const FObjectInitializer& ObjectInitializer);

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

protected:
	/** Context to use as cone target */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	TSubclassOf<UEnvQueryContext> ConeTarget;

	/** Cone angle in degrees */
	UPROPERTY(EditDefaultsOnly, Category = Test, meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float ConeAngle;

	/** Maximum distance from cone origin */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	float MaxDistance;
};
