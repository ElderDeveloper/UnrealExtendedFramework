// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UEExtendedMantleComponent.generated.h"


UENUM(BlueprintType)
enum EMantleType
{
	HighMantle ,
	LowMantle ,
	FallingCatch
};


class UAnimMontage;
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UEEXPANDEDFRAMEWORK_API UUEExtendedMantleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UUEExtendedMantleComponent();

	UPROPERTY(EditDefaultsOnly,Category="Trace Settings")
	bool CanMantle = true;

	UPROPERTY(EditDefaultsOnly,Category="Trace Settings")
	TEnumAsByte<ETraceTypeQuery> TraceChannel;

	UPROPERTY(EditDefaultsOnly,Category="Trace Settings")
	FName TraceProfileName;
	
	UPROPERTY(EditDefaultsOnly,Category="Trace Settings")
	float MaxLedgeHeight = 150.0f;

	UPROPERTY(EditDefaultsOnly,Category="Trace Settings")
	float MinLedgeHeight = 50.0f;

	UPROPERTY(EditDefaultsOnly,Category="Trace Settings")
	float ReachDistance = 70.0f;

	UPROPERTY(EditDefaultsOnly,Category="Trace Settings")
	float ForwardTraceRadius = 30.f;

	UPROPERTY(EditDefaultsOnly,Category="Trace Settings")
	float DownwardTraceRadius = 30.f;

	UPROPERTY(EditDefaultsOnly,Category="Trace Settings")
	TEnumAsByte<EDrawDebugTrace::Type> DrawDebugType;


	

	UPROPERTY(EditDefaultsOnly,Category="Mantle Montages")
	UAnimMontage* HighMantleMontage;

	UPROPERTY(EditDefaultsOnly,Category="Mantle Montages")
	UAnimMontage* LowMantleMontage;

	UPROPERTY(EditDefaultsOnly,Category="Mantle Montages")
	UAnimMontage* FallingCatchMontage;



	UFUNCTION(BlueprintPure)
	FORCEINLINE bool GetIsMantling () const { return Mantling; }

	UFUNCTION(BlueprintCallable)
	void SetCanMantle (const bool Enable ) {  CanMantle = Enable; }
	
	
protected:

	bool Mantling = false;



	UPROPERTY()
	class ACharacter* OwnerCharacter;
	
	UPROPERTY()
	class UCharacterMovementComponent* OwnerMovement;

	UFUNCTION()
	void OnMontageBlendOut(UAnimMontage* Montage, bool bInterrupted);
	
	// Called when the game starts
	virtual void BeginPlay() override;
	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;

private:

	FVector GetCapsuleBaseLocation(float ZOffset = 0) const;
	FVector GetCapsuleLocationFromBase(FVector BaseLocation , float ZOffset = 0) const;

	FVector GetPlayerMovementForward() const;
	FVector GetPlayerMovementRight() const;
	FVector GetPlayerMovementInput() const;

	UAnimMontage* GetMantleMontage(EMantleType MantleType) const;

	bool CheckMantleState();
	
	void StartMantle(float MantleHeight , EMantleType MantleType);


	
};
