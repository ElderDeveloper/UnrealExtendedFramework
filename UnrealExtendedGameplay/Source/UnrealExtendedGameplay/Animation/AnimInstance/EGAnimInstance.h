// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Animation/AnimInstance.h"
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UnrealExtendedGameplay/Animation/TurnInPlace/EGTurnInPlaceAnimSource.h"
#include "UnrealExtendedGameplay/Animation/TurnInPlace/EGTurnInPlaceTypes.h"

#include "EGAnimInstance.generated.h"

class ACharacter;
class APawn;
class UCharacterMovementComponent;
class UEGTurnInPlaceComponent;

/**
 * UEGAnimInstance
 *
 * A base for character AnimBPs. Gameplay is read once per update on the game thread (velocity,
 * acceleration, movement mode, aim, IEGAnimStateProvider tags); everything derived from it is
 * computed in NativeThreadSafeUpdateAnimation, off the game thread, so the AnimBP can use
 * Thread Safe Update Animation and property access without casts in its event graph.
 *
 * With a UEGTurnInPlaceComponent on the owner it also runs turn in place's anim side and serves its
 * curves (IEGTurnInPlaceAnimSource); the graph then only needs the state machine (TurnInPlace.md).
 */
UCLASS(Abstract, Blueprintable)
class UNREALEXTENDEDGAMEPLAY_API UEGAnimInstance : public UAnimInstance, public IEGTurnInPlaceAnimSource
{
	GENERATED_BODY()

public:
	// ----- Settings ----------------------------------------------------------------------------

	/** Yaw rate (deg/s) that gives full lean. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Settings", meta = (ClampMin = "1"))
	float FullLeanYawRate = 180.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Settings", meta = (ClampMin = "0"))
	float LeanInterpSpeed = 8.0f;

	/** Below this ground speed (cm/s) the character counts as standing still. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion|Settings", meta = (ClampMin = "0"))
	float MoveSpeedThreshold = 3.0f;

	// ----- Gathered on the game thread ------------------------------------------------------------

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Locomotion")
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Locomotion")
	FVector Acceleration = FVector::ZeroVector;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Locomotion")
	FRotator ActorRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Locomotion")
	FRotator AimRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Locomotion")
	float MaxWalkSpeed = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Locomotion")
	bool bIsFalling = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Locomotion")
	bool bIsCrouching = false;

	/** Tags from every IEGAnimStateProvider on the owner (the actor and its components). */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "State")
	FGameplayTagContainer StateTags;

	// ----- Derived on the worker thread ----------------------------------------------------------

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Locomotion")
	float Speed = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Locomotion")
	float GroundSpeed = 0.0f;

	/** Movement direction relative to the actor's facing, -180..180 degrees (0 forward, 90 right). */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Locomotion")
	float Direction = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Locomotion")
	bool bHasAcceleration = false;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Locomotion")
	bool bShouldMove = false;

	/** Aim relative to the actor, for aim offsets. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Locomotion")
	float AimPitch = 0.0f;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Locomotion")
	float AimYaw = 0.0f;

	/** -1..1 from the turn rate, smoothed. */
	UPROPERTY(Transient, BlueprintReadOnly, Category = "Locomotion")
	float Lean = 0.0f;

	UFUNCTION(BlueprintPure, Category = "Extended|Anim State", meta = (BlueprintThreadSafe))
	bool HasStateTag(FGameplayTag Tag) const { return StateTags.HasTag(Tag); }

	// ----- Turn in place ----------------------------------------------------------------------------

	/** Pass by reference to the UEGTurnInPlaceAnimLibrary node functions; read Output for transitions. */
	UPROPERTY(Transient, BlueprintReadWrite, Category = "Turn In Place")
	FEGTurnInPlaceAnimData TurnInPlace;

	virtual FEGTurnInPlaceCurves GetTurnInPlaceCurves_Implementation() const override { return TurnInPlace.Curves; }

	/** Find the owner's IEGAnimStateProvider components again (after adding or removing one at runtime). */
	UFUNCTION(BlueprintCallable, Category = "Extended|Anim State")
	void RefreshStateProviders();

protected:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeBeginPlay() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;

	/** Game thread: read the owner. Override to gather more; call Super. */
	virtual void GatherGameThreadData(float DeltaSeconds);

	/** Worker thread: derive values from what was gathered. Override to add more; call Super. */
	virtual void ComputeThreadSafeData(float DeltaSeconds);

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Owner")
	TObjectPtr<APawn> OwningPawn;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Owner")
	TObjectPtr<ACharacter> OwningCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterMovementComponent> CharacterMovement;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Owner")
	TObjectPtr<UEGTurnInPlaceComponent> TurnInPlaceComponent;

private:
	TArray<TWeakObjectPtr<UObject>> StateProviders;
	float PreviousYaw = 0.0f;
	bool bHasPreviousYaw = false;
};
