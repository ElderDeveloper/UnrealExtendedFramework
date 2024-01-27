// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "EGPatrolGeneratorData.h"
#include "EGProceduralPatrolGeneratorSceneComponent.generated.h"

class USphereComponent;
class UBoxComponent;

UCLASS(ClassGroup=(Custom),Blueprintable , BlueprintType, meta=(BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGProceduralPatrolGeneratorSceneComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UEGProceduralPatrolGeneratorSceneComponent();
	
	UPROPERTY(BlueprintReadWrite , EditAnywhere , Category = "PatrolGeneratorShape")
	EProceduralPatrolShape PatrolShape;

	UPROPERTY(BlueprintReadWrite , EditAnywhere , meta=(EditCondition = "PatrolShape==EProceduralPatrolShape::Box" , EditConditionHides ) , Category = "PatrolGeneratorShape")
	FVector BoxCollisionShape = FVector{ 64 ,64 ,64};

	UPROPERTY(BlueprintReadWrite , EditAnywhere , meta=(EditCondition = "PatrolShape==EProceduralPatrolShape::Sphere" , EditConditionHides ) , Category = "PatrolGeneratorShape")
	float SphereRadius = 64;


	UPROPERTY(BlueprintReadWrite , EditAnywhere , Category = "PatrolGeneratorControl")
	int32 NumberOfPatrolPoints = 4;

	UPROPERTY(BlueprintReadWrite , EditAnywhere , Category = "PatrolGeneratorControl")
	float DistanceBetweenPatrolPoints = 100;

	UPROPERTY(BlueprintReadWrite , EditAnywhere , Category = "PatrolGeneratorControl")
	bool bCheckNavMesh = true;

	UPROPERTY(BlueprintReadWrite , EditAnywhere , meta=(MakeEditWidget) , Category = "PatrolGeneratorControl")
	TArray<FVector> PatrolPointLocation;

	UPROPERTY(BlueprintReadWrite , EditAnywhere , meta=(MakeEditWidget) , Category = "PatrolGeneratorWarning")
	UTexture2D* PatrolPointAcceptedTexture;

	UPROPERTY(BlueprintReadWrite , EditAnywhere , meta=(MakeEditWidget) , Category = "PatrolGeneratorWarning")
	UTexture2D* PatrolPointRefusedTexture;

protected:

	UPROPERTY(BlueprintReadWrite  , Category = "PatrolGenerator")
	UBoxComponent* BoxComponent;

	UPROPERTY(BlueprintReadWrite , Category = "PatrolGenerator")
	USphereComponent* SphereComponent;

	UPROPERTY(BlueprintReadWrite , Category = "PatrolGenerator")
	TArray<UBillboardComponent*> PatrolPointComponents;
	
	UPROPERTY(BlueprintReadWrite)
	FTimerHandle PointGenerationHandle;

	UPROPERTY()
	class UNavigationSystemV1* NavSys;

	UFUNCTION(BlueprintCallable)
	void UpdateShapes();

	UFUNCTION(BlueprintCallable)
	void UpdatePatrolPoints(UBillboardComponent* BillboardComponent , int32 Index);

	UFUNCTION(BlueprintPure , Category = "PatrolGenerator")
	FVector GetRandomRelativeLocation() const;

	UFUNCTION(BlueprintPure , Category = "PatrolGenerator")
	float GetTraceSize() const;

	UFUNCTION(BlueprintCallable)
	void ClearPatrolPoints();

	UFUNCTION(BlueprintNativeEvent , BlueprintCallable , CallInEditor , Category = "PatrolGeneratorControl")
	void GeneratePatrolPoints();

	void GeneratePoints();

	bool CheckIfPointIsReachable(const FVector& Point) const;

	FVector WorldToLocal(const FVector& WorldLocation) const;
	FVector LocalToWorld(const FVector& LocalLocation) const;
};
