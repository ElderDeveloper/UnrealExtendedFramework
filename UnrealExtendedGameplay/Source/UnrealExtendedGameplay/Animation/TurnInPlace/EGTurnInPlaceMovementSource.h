// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGTurnInPlaceTypes.h"
#include "UObject/Object.h"

#include "EGTurnInPlaceMovementSource.generated.h"

class AActor;
class UActorComponent;
class USkeletalMeshComponent;

/**
 * UEGTurnInPlaceMovementSource
 *
 * How UEGTurnInPlaceComponent reads and rotates its owner, picked on the component (Movement
 * Source). The default, Character Movement, covers ACharacter. A pawn on another movement system
 * (Mover, a custom pawn movement) subclasses this in C++ or Blueprint: fill the movement info,
 * and apply rotation the way that system expects. Turn in place then drives rotation from its
 * component (the hooks are character-only).
 *
 * Stateless: one instance may serve every call of a frame, in any order.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class UNREALEXTENDEDGAMEPLAY_API UEGTurnInPlaceMovementSource : public UObject
{
	GENERATED_BODY()

public:
	/** Fills what turn in place reads this frame. False: the owner cannot turn in place now. */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|Turn In Place")
	bool GatherMovement(const AActor* Owner, FEGTurnInPlaceMovementInfo& OutInfo) const;
	virtual bool GatherMovement_Implementation(const AActor* Owner, FEGTurnInPlaceMovementInfo& OutInfo) const;

	/** Sets the owner's rotation. Default: its root component, without a sweep for a yaw-only change. */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|Turn In Place")
	void ApplyRotation(AActor* Owner, const FRotator& NewRotation) const;
	virtual void ApplyRotation_Implementation(AActor* Owner, const FRotator& NewRotation) const;

	/** The mesh that plays the turn animations. Default: the owner's first skeletal mesh. */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|Turn In Place")
	USkeletalMeshComponent* GetMesh(const AActor* Owner) const;
	virtual USkeletalMeshComponent* GetMesh_Implementation(const AActor* Owner) const;

	/**
	 * The component whose tick moves the owner. When turn in place drives rotation itself it ticks
	 * after this one and before the mesh, so the animation sees the final rotation.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|Turn In Place")
	UActorComponent* GetMovementComponent(const AActor* Owner) const;
	virtual UActorComponent* GetMovementComponent_Implementation(const AActor* Owner) const;

	virtual UWorld* GetWorld() const override;
};

/**
 * Character Movement: ACharacter with UCharacterMovementComponent (any subclass). Reads the
 * rotation flags (orient to movement, controller desired rotation, controller yaw), custom
 * gravity, root motion, and the movement base.
 */
UCLASS(DisplayName = "Character Movement")
class UNREALEXTENDEDGAMEPLAY_API UEGTurnInPlaceCharacterSource : public UEGTurnInPlaceMovementSource
{
	GENERATED_BODY()

public:
	virtual bool GatherMovement_Implementation(const AActor* Owner, FEGTurnInPlaceMovementInfo& OutInfo) const override;
	virtual USkeletalMeshComponent* GetMesh_Implementation(const AActor* Owner) const override;
	virtual UActorComponent* GetMovementComponent_Implementation(const AActor* Owner) const override;
};
