// ... existing code ...
#pragma once

#include "CoreMinimal.h"
#include "NavModifierVolume.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "EFEQSTest_NavMeshCost.generated.h"

/**
// ... existing code ...
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFEQSTest_NavMeshCost : public UEnvQueryTest
{
	GENERATED_BODY()

public:
	UEFEQSTest_NavMeshCost();
	
	
	UPROPERTY(EditDefaultsOnly, Category=Score)
	float SearchDistance = 3000.f;
	
	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;
	bool IsPointInNavModifierBounds(const FVector& Point, ANavModifierVolume* NavModifier) const;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;


};