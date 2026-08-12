// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedFramework/Data/EFTraceData.h"
#include "UObject/Object.h"
#include "EGDynamicTargetingComponent.generated.h"


class APlayerController;
class UEGTargetingWidget;
class UEGTargetingTargetComponent;


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDynamicTargetLockedOnOff, AActor*, TargetActor);



DECLARE_LOG_CATEGORY_EXTERN(LogTargeting, Log, All);

#define TargetingError(Text) UE_LOG(LogTargeting,Error,Text);
#define TargetingWarning(Text) UE_LOG(LogTargeting,Warning,Text);
#define TargetingLog(Text) UE_LOG(LogTargeting,Log,Text);


UCLASS(ClassGroup=(Extended),BlueprintType , Blueprintable , meta=(BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGDynamicTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

	public:
	// Sets default values for this component's properties
	UEGDynamicTargetingComponent();

	
	// The minimum distance to enable target locked on.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Target System")
	float MaximumDistanceToEnable = 1200.0f;


	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Target System")
	float ControlRotationInterpSpeed = 5;
	

	UPROPERTY(EditDefaultsOnly , BlueprintReadWrite , Category= "Target System")
	FSphereTraceStruct TargetActorSearchSphere;
	
	UPROPERTY(EditDefaultsOnly , BlueprintReadWrite , Category= "Target System")
	FLineTraceStruct TargetObstacleLineTrace;

	

	// Whether or not the character rotation should be controlled when Target is locked on.
	//
	// If true, it'll set the value of bUseControllerRotationYaw and bOrientationToMovement variables on Target locked on / off.
	//
	// Set it to true if you want the character to rotate around the locked on target to enable you to setup strafe animations.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Target System")
	bool bShouldControlRotation = false;

	// Whether to accept pitch input when bAdjustPitchBasedOnDistanceToTarget is disabled
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Target System")
	bool bIgnoreLookInput = true;

	// The amount of time to break line of sight when actor gets behind an Object.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Target System")
	float BreakLineOfSightDelay = 2.0f;

	// The amount of time to start targeting when looking at actor
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Target System")
	float ActorTargetingDelay = 2.0f;
	
	// Lower this value is, easier it will be to switch new target on right or left. Must be < 1.0f if controlling with gamepad stick
	//
	// When using Sticky Feeling feature, it has no effect (see StickyRotationThreshold)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Target System")
	float StartRotatingThreshold = 0.85f;
	
	
	// Setting this to true will tell the Target System to adjust the Pitch Offset (the Y axis) when locked on,
	// depending on the distance to the target actor.
	//
	// It will ensure that the Camera will be moved up vertically the closer this Actor gets to its target.
	//
	// Formula:
	//
	//   (DistanceToTarget * PitchDistanceCoefficient + PitchDistanceOffset) * -1.0f
	//
	// Then Clamped by PitchMin / PitchMax
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Target System|Pitch Offset")
	bool bAdjustPitchBasedOnDistanceToTarget = true;

	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Target System|Pitch Offset")
	float PitchDistanceCoefficient = -0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Target System|Pitch Offset")
	float PitchDistanceOffset = 60.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Target System|Pitch Offset")
	float PitchMin = -50.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Target System|Pitch Offset")
	float PitchMax = -20.0f;

	
	
	// Set it to true / false whether you want a sticky feeling when switching target
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Target System|Sticky Feeling on Target Switch")
	bool bEnableStickyTarget = false;

	
	// This value gets multiplied to the AxisValue to check against StickyRotationThreshold.
	// Only used when Sticky Target is enabled.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Target System|Sticky Feeling on Target Switch")
	float AxisMultiplier = 1.0f;

	// Lower this value is, easier it will be to switch new target on right or left.
	// This is similar to StartRotatingThreshold, but you should set this to a much higher value.
	// Only used when Sticky Target is enabled.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Target System|Sticky Feeling on Target Switch")
	float StickyRotationThreshold = 30.0f;

	
	
	// Called when a target is locked off, either if it is out of reach (based on MinimumDistanceToEnable) or behind an Object.
	UPROPERTY(BlueprintAssignable, Category = "Target System")
	FOnDynamicTargetLockedOnOff OnTargetLockedOff;

	// Called when a target is locked on
	UPROPERTY(BlueprintAssignable, Category = "Target System")
	FOnDynamicTargetLockedOnOff OnTargetLockedOn;




private:
	
	UPROPERTY()
	AActor* OwnerActor;
	UPROPERTY()
	APawn* OwnerPawn;
	UPROPERTY()
	APlayerController* OwnerPlayerController;
	
	UPROPERTY()
	AActor* TargetedActor;

	UPROPERTY()
	AActor* LocalTargetedActor;

	bool IsTargetEnabled = false;

	UPROPERTY()
	UEGTargetingTargetComponent* TargetedActorComponent;
	
	
	FTimerHandle LineOfSightBreakTimerHandle;
	FTimerHandle SwitchingTargetTimerHandle;


	bool bIsSwitchingTarget = false;
	float ClosestTargetDistance = 0.0f;
	bool bDesireToSwitch = false;
	float StartRotatingStack = 0.0f;
	float CurrentBreakLineOfSight = 0.f;
	float CurrentActorTargetingDelay = 0.f;


	TArray<AActor*> FindTargetsInRange(TArray<AActor*> ActorsToLook, float RangeMin, float RangeMax) const;
	AActor* FindNearestTarget(const TArray<AActor*>& Actors);

	
	bool CheckShouldBreakLineOfSight(float DeltaTime);
	bool IsInViewport(const AActor* TargetActor) const;


	//~ Actor rotation
	FRotator GetControlRotationOnTarget(const AActor* OtherActor) const;
	void SetControlRotationOnTarget(AActor* TargetActor) const;
	void ControlRotation(bool ShouldControlRotation) const;

	float GetAngleUsingCameraRotation(const AActor* ActorToLook) const;
	float GetAngleUsingCharacterRotation(const AActor* ActorToLook) const;

	
	
	//~ Targeting
	void TargetLockOn(AActor* TargetToLockOn);
	

	/**
	 *  Sets up cached Owner PlayerController from Owner Pawn.
	 *  For local split screen, Pawn's Controller may not have been setup already when this component begins play.
	 */
	 void SetupLocalPlayerController();


	/*
	 * Return if We Should Target Actor If Focus Enough
	 */
	void CheckActorInFocus();
	



protected:

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;


public:

	
	// Function to call to manually untarget.
	UFUNCTION(BlueprintCallable, Category = "Target System")
	void TargetLockOff();
	
	
	// Returns the reference to currently targeted Actor if any
	UFUNCTION(BlueprintCallable, Category = "Target System")
	FORCEINLINE AActor* GetLockedOnTargetActor() const { return TargetedActor; }

	
	// Returns true / false whether the system is targeting an actor
	UFUNCTION(BlueprintCallable, Category = "Target System")
	FORCEINLINE bool GetIsTargetingEnabled() const { return IsValid(TargetedActor); }
};
