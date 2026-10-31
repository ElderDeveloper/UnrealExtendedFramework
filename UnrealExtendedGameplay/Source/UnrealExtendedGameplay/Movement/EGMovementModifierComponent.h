// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"

#include "EGMovementModifierComponent.generated.h"

class ACharacter;
class UCharacterMovementComponent;

/** Scales applied to the character's movement while the request is active. 1 leaves a value alone; 0 locks it. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGMovementModifier
{
	GENERATED_BODY()

	/** MaxAcceleration. 0 stops input from steering the character. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Modifier", meta = (ClampMin = "0"))
	float AccelerationScale = 1.0f;

	/** MaxWalkSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Modifier", meta = (ClampMin = "0"))
	float MaxWalkSpeedScale = 1.0f;

	/** RotationRate. 0 stops the character turning towards movement or control rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Modifier", meta = (ClampMin = "0"))
	float RotationRateScale = 1.0f;
};

USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGMovementModifierHandle
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Id = 0;

	bool IsValid() const { return Id != 0; }
	void Invalidate() { Id = 0; }
};

/**
 * FEGMovementModifierStack
 *
 * Active modifiers combine by multiplying, so an attack's acceleration lock and a slow effect both
 * hold, and removing one leaves the other. Free of actors so it can be tested on its own.
 */
struct UNREALEXTENDEDGAMEPLAY_API FEGMovementModifierStack
{
	int32 Push(const FEGMovementModifier& Modifier);
	bool Remove(int32 Id);
	bool IsEmpty() const { return Modifiers.Num() == 0; }
	FEGMovementModifier GetCombined() const;

	/**
	 * Recovers the unscaled value when something else changed the scaled one in between: the new
	 * base is Current / LastScale. A zero scale hides the base, so the previous base is kept.
	 */
	static float Rebase(float PreviousBase, float LastApplied, float Current, float LastScale);

private:
	TArray<TPair<int32, FEGMovementModifier>> Modifiers;
	int32 NextId = 0;
};

/**
 * UEGMovementModifierComponent
 *
 * The one owner of the character movement values that animation and gameplay both want to change
 * (acceleration, walk speed, rotation rate). Systems push a modifier and keep the handle instead
 * of writing the movement component directly and restoring a value they saved, which breaks as
 * soon as two of them overlap. Notifies add it on demand (FindOrAdd), so nothing needs setting up.
 *
 * Values changed by other code while modifiers are active are picked up as the new base the next
 * time a modifier is pushed or removed.
 */
UCLASS(ClassGroup = (Extended), meta = (BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGMovementModifierComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UEGMovementModifierComponent();

	/** The component on this character, created and registered if it has none. */
	static UEGMovementModifierComponent* FindOrAdd(ACharacter* Character);

	UFUNCTION(BlueprintCallable, Category = "Extended|Movement Modifier")
	FEGMovementModifierHandle PushModifier(const FEGMovementModifier& Modifier);

	/** Removes the modifier and invalidates the handle. Safe on an invalid handle. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Movement Modifier")
	void RemoveModifier(UPARAM(ref) FEGMovementModifierHandle& Handle);

	UFUNCTION(BlueprintPure, Category = "Extended|Movement Modifier")
	bool HasModifiers() const { return !Stack.IsEmpty(); }

	UFUNCTION(BlueprintPure, Category = "Extended|Movement Modifier")
	FEGMovementModifier GetCombinedModifier() const { return Stack.GetCombined(); }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UCharacterMovementComponent* GetMovement() const;
	void Apply();
	void Restore();

	FEGMovementModifierStack Stack;

	bool bHasBase = false;
	float BaseAcceleration = 0.0f;
	float BaseWalkSpeed = 0.0f;
	FRotator BaseRotationRate = FRotator::ZeroRotator;

	FEGMovementModifier LastScale;
	float LastAcceleration = 0.0f;
	float LastWalkSpeed = 0.0f;
	FRotator LastRotationRate = FRotator::ZeroRotator;
};
