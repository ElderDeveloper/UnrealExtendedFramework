// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UnrealExtendedGameplay/SoftCollision/EGSoftCollisionTypes.h"

#include "EGSoftCollisionDriver.generated.h"

class APawn;
class UEGSoftCollisionComponent;

/**
 * UEGSoftCollisionDriver
 * Decides how a push moves a pawn. Instanced into a profile (or the class default object when the
 * profile leaves the slot empty) and therefore shared: a driver must keep no per-pawn state. The
 * component owns the active push and passes everything a driver needs.
 *
 * The hot path is plain C++ virtuals, because a thousand server pawns call TickPush every frame and
 * a BlueprintNativeEvent always goes through ProcessEvent. Blueprint drivers derive from
 * UEGSoftCollisionDriver_Blueprint instead.
 *
 * The component calls, in order: CancelPush for the push being replaced, ApplyPush for the new one,
 * TickPush every frame before movement while it is active (only when NeedsTick), and CancelPush
 * again if it is cut short. A push that simply runs out of time is not cancelled.
 */
UCLASS(Abstract, EditInlineNew, DefaultToInstanced, CollapseCategories)
class UNREALEXTENDEDGAMEPLAY_API UEGSoftCollisionDriver : public UObject
{
	GENERATED_BODY()

public:
	/** Where this driver runs. */
	virtual EEGSoftCollisionDriverRole GetRole() const { return EEGSoftCollisionDriverRole::ServerAuthority; }

	/** Whether the driver can move this pawn at all. */
	virtual bool SupportsPawn(const APawn& Pawn) const { return false; }

	/**
	 * Start a push. OutHandle is the driver's own identifier for it (a root motion source ID, for
	 * one), handed back to TickPush and CancelPush. Returns false when nothing was applied.
	 */
	virtual bool ApplyPush(UEGSoftCollisionComponent& Component, const FEGSoftPushCommand& Command, int32& OutHandle) const { return false; }

	/** Whether active pushes need TickPush every frame. */
	virtual bool NeedsTick() const { return false; }

	/** Called every frame before movement while a push from this driver is active. Elapsed counts from its start. */
	virtual void TickPush(UEGSoftCollisionComponent& Component, const FEGSoftPushCommand& Command, int32 Handle, float Elapsed, float DeltaSeconds) const {}

	/** Remove a push before it ends. */
	virtual void CancelPush(UEGSoftCollisionComponent& Component, const FEGSoftPushCommand& Command, int32 Handle) const {}

	/** Whether the push identified by Handle is still running, for drivers that can tell. */
	virtual bool IsPushRunning(const UEGSoftCollisionComponent& Component, int32 Handle) const { return true; }

protected:
	/** Top-up used by the velocity drivers: the speed missing along Direction to reach Target, never negative. */
	static float ComputeVelocityDeficit(const FVector& CurrentVelocity, const FVector& Direction, float TargetSpeed);
};

/**
 * Base for drivers written in Blueprint. Slower than a native driver (every call goes through
 * ProcessEvent), so keep it for a handful of special pawns.
 */
UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew, DefaultToInstanced, CollapseCategories)
class UNREALEXTENDEDGAMEPLAY_API UEGSoftCollisionDriver_Blueprint : public UEGSoftCollisionDriver
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soft Collision")
	EEGSoftCollisionDriverRole Role = EEGSoftCollisionDriverRole::ServerAuthority;

	/** Whether TickPush (K2_TickPush) runs every frame while a push is active. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Soft Collision")
	bool bTickActivePushes = false;

	virtual EEGSoftCollisionDriverRole GetRole() const override { return Role; }
	virtual bool SupportsPawn(const APawn& Pawn) const override;
	virtual bool ApplyPush(UEGSoftCollisionComponent& Component, const FEGSoftPushCommand& Command, int32& OutHandle) const override;
	virtual bool NeedsTick() const override { return bTickActivePushes; }
	virtual void TickPush(UEGSoftCollisionComponent& Component, const FEGSoftPushCommand& Command, int32 Handle, float Elapsed, float DeltaSeconds) const override;
	virtual void CancelPush(UEGSoftCollisionComponent& Component, const FEGSoftPushCommand& Command, int32 Handle) const override;

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Extended|Soft Collision", meta = (DisplayName = "Supports Pawn"))
	bool K2_SupportsPawn(const APawn* Pawn) const;

	/** Return the handle you want back in Tick and Cancel, or -1 when nothing was applied. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Extended|Soft Collision", meta = (DisplayName = "Apply Push"))
	int32 K2_ApplyPush(UEGSoftCollisionComponent* Component, const FEGSoftPushCommand& Command) const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Extended|Soft Collision", meta = (DisplayName = "Tick Push"))
	void K2_TickPush(UEGSoftCollisionComponent* Component, const FEGSoftPushCommand& Command, int32 Handle, float Elapsed, float DeltaSeconds) const;

	UFUNCTION(BlueprintImplementableEvent, Category = "Extended|Soft Collision", meta = (DisplayName = "Cancel Push"))
	void K2_CancelPush(UEGSoftCollisionComponent* Component, const FEGSoftPushCommand& Command, int32 Handle) const;
};
