// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UEExtendedLadder.generated.h"

class UArrowComponent;
class UBoxComponent;
class UUEExtendedLadderComponent;

UCLASS()
class UEEXPANDEDFRAMEWORK_API AUEExtendedLadder : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AUEExtendedLadder();

	UPROPERTY(EditDefaultsOnly ,BlueprintReadWrite ,  Category="Components")
	UStaticMeshComponent* LadderMeshComponent;

	UPROPERTY(EditDefaultsOnly ,BlueprintReadWrite ,  Category="Components")
	UArrowComponent* LadderEntryArrow;

	UPROPERTY(EditDefaultsOnly ,BlueprintReadWrite ,  Category="Components")
	UArrowComponent* LadderExitArrow;

	UPROPERTY(EditDefaultsOnly ,BlueprintReadWrite ,  Category="Components")
	UBoxComponent* LadderEnterBox;

	UPROPERTY(EditDefaultsOnly ,BlueprintReadWrite ,  Category="Components")
	UBoxComponent* LadderClimbBox;


	UPROPERTY(EditAnywhere ,BlueprintReadWrite , meta = (MakeEditWidget = true), Category="Construction")
	FVector LadderSize;

	UPROPERTY(EditAnywhere ,BlueprintReadWrite ,  Category="Mesh")
	UStaticMesh* LadderMesh;
	
	UPROPERTY(EditAnywhere ,BlueprintReadWrite ,  Category="Mesh")
	float LadderMeshDistance;

	UPROPERTY(BlueprintReadWrite)
	ACharacter* PlayerRef;
	
	UPROPERTY(BlueprintReadWrite)
	UUEExtendedLadderComponent* FoundLadderComponent;
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;



	
	UFUNCTION()
	void OnLadderEnterBoxPlayerEnter(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnLadderEnterBoxPlayerLeave(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);


	
	UFUNCTION()
	void OnLadderClimbBoxPlayerEnter(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnLadderClimbBoxPlayerLeave(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);


};
