// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EFEQSTest_PathLength.generated.h"

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFEQSTest_PathLength : public UEnvQueryTest
{
	GENERATED_BODY()

public:
	UEFEQSTest_PathLength(const FObjectInitializer& ObjectInitializer);

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

protected:
	/** Context to measure path length to */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	TSubclassOf<UEnvQueryContext> PathTo;

	/** If true, path length will be tested in 3D (include height difference) */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	bool bIncludeHeight;

	/** If true, partial paths will be accepted */
	UPROPERTY(EditDefaultsOnly, Category = Test)
	bool bAllowPartialPath;
};
