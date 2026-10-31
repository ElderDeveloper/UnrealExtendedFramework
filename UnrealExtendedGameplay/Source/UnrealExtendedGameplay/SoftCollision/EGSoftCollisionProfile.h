// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EGSoftCollisionTypes.h"

#include "EGSoftCollisionProfile.generated.h"

class UEGSoftCollisionDriver;

/**
 * How one kind of pawn pushes and is pushed. Several pawns share one; a component without a
 * profile uses this class's defaults, so a pawn works with nothing assigned.
 *
 * Drivers are instanced into the asset and therefore shared by every pawn using it: a driver keeps
 * no per-pawn state. An empty driver slot is filled automatically from the pawn's movement:
 * a Character gets the root motion driver when a player controls it and the force driver
 * otherwise, a simulating physics body gets the physics driver, any other pawn movement gets the
 * pawn movement driver.
 */
UCLASS(BlueprintType)
class UNREALEXTENDEDGAMEPLAY_API UEGSoftCollisionProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body")
	FEGSoftCollisionShape Shape;

	/** Push shape size relative to the collision shape. Above 1 starts pushing just before the capsules touch. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body", meta = (ClampMin = "0.1", ClampMax = "4.0"))
	float RadiusScale = 1.1f;

	/** Resistance and momentum. A heavier pawn takes a smaller share of every push. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body", meta = (ClampMin = "0.01"))
	float Weight = 1.0f;

	/** How hard this pawn pushes others. 0 never pushes anyone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body", meta = (ClampMin = "0.0"))
	float PushPower = 1.0f;

	/**
	 * A Character moved by gameplay root motion (a montage, a dash or lunge source) is not pushed, so
	 * a crowd cannot bend a lunge off its warp target. Soft pushes themselves never count.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body")
	bool bSuppressDuringRootMotion = true;

	/** Push falling Characters too (knockbacks, jumps). Off keeps launches and jumps exact. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body")
	bool bPushWhileFalling = false;

	/** Push scale for Characters that use RVO avoidance, so the two systems do not fight. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Body", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AvoidanceScale = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Push", meta = (ShowOnlyInnerProperties))
	FEGSoftCollisionTuning Tuning;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scan", meta = (ShowOnlyInnerProperties))
	FEGSoftCollisionScanSettings Scan;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Network", meta = (ShowOnlyInnerProperties))
	FEGSoftCollisionNetSettings Network;

	/** Used while a player controls the pawn. Empty picks one from the pawn's movement. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Drivers")
	TObjectPtr<UEGSoftCollisionDriver> PlayerDriver;

	/** Used while the server or nobody controls the pawn. Empty picks one from the pawn's movement. */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Drivers")
	TObjectPtr<UEGSoftCollisionDriver> AIDriver;
};
