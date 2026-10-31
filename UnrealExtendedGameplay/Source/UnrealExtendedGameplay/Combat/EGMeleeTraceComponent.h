// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "EGMeleeTypes.h"
#include "Engine/EngineTypes.h"
#include "UnrealExtendedGameplay/Animation/Events/EGAnimEventReceiver.h"

#include "EGMeleeTraceComponent.generated.h"

class UEGMeleeDamagePolicy;
class USceneComponent;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGOnMeleeHit, const FEGMeleeHit&, Hit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEGOnMeleeWindow, FGameplayTag, WindowTag, int32, WindowId);

/**
 * UEGMeleeTraceComponent
 *
 * Melee hit detection for one character. Windows are opened by Melee Window notifies (or any anim
 * event window whose tag matches a shape), or by code through OpenWindow. While a window is open,
 * every point of every matching shape is swept from where it was last frame to where it is now,
 * so a fast swing cannot pass through a target between two frames.
 *
 * Each actor is hit once per window. Hits go to the damage policy (authoritative hits only),
 * optional hit pause, and OnHit.
 */
UCLASS(ClassGroup = (Extended), meta = (BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGMeleeTraceComponent : public UActorComponent, public IEGAnimEventReceiver
{
	GENERATED_BODY()

public:
	UEGMeleeTraceComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
	TArray<FEGMeleeHitShape> Shapes;

	/** Object types the sweeps look for. The default (ObjectTypeQuery3) is Pawn in the default collision setup. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
	bool bTraceComplex = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
	EEGMeleeDetection Detection = EEGMeleeDetection::AuthorityAndLocalOwner;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "Melee")
	TObjectPtr<UEGMeleeDamagePolicy> DamagePolicy;

	/** Freeze both animations briefly on a hit (real seconds). 0 is off. Uses mesh anim rate, so movement is untouched. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee|Hit Pause", meta = (ClampMin = "0", Units = "s"))
	float HitPauseDuration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee|Hit Pause", meta = (ClampMin = "0", ClampMax = "1"))
	float HitPauseAnimRate = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee|Hit Pause")
	bool bHitPauseAttacker = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee|Hit Pause")
	bool bHitPauseVictim = true;

	UPROPERTY(BlueprintAssignable, Category = "Melee")
	FEGOnMeleeHit OnHit;

	UPROPERTY(BlueprintAssignable, Category = "Melee")
	FEGOnMeleeWindow OnWindowOpened;

	UPROPERTY(BlueprintAssignable, Category = "Melee")
	FEGOnMeleeWindow OnWindowClosed;

	/**
	 * Opens every shape at or under WindowTag. DefaultSource supplies sockets for shapes without a
	 * source of their own; null uses the owner's character mesh. Returns the window id, or
	 * INDEX_NONE when no shape matches.
	 */
	UFUNCTION(BlueprintCallable, Category = "Extended|Melee", meta = (AutoCreateRefTerm = "AttackTags"))
	int32 OpenWindow(FGameplayTag WindowTag, USkeletalMeshComponent* DefaultSource, const FGameplayTagContainer& AttackTags, float DamageMultiplier = 1.0f);

	/** Sweeps one last time and closes the window. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Melee")
	void CloseWindow(int32 WindowId);

	UFUNCTION(BlueprintCallable, Category = "Extended|Melee")
	void CloseAllWindows();

	/** Use this component's sockets for shapes at or under ShapeTag (an equipped weapon). Null clears the override. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Melee")
	void SetShapeSource(FGameplayTag ShapeTag, USceneComponent* Source);

	UFUNCTION(BlueprintPure, Category = "Extended|Melee")
	bool IsWindowOpen(FGameplayTag WindowTag) const;

	virtual void ReceiveAnimEvent_Implementation(const FEGAnimEvent& Event) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FActiveShape
	{
		int32 ShapeIndex = INDEX_NONE;
		TWeakObjectPtr<USceneComponent> Source;
		TArray<FVector> PreviousPoints;
	};

	struct FWindow
	{
		int32 Id = INDEX_NONE;
		FGameplayTag Tag;
		float DamageMultiplier = 1.0f;
		FGameplayTagContainer AttackTags;
		TArray<FActiveShape> ActiveShapes;
		TArray<TWeakObjectPtr<AActor>> HitActors;
	};

	int32 OpenWindowWithId(int32 WindowId, FGameplayTag WindowTag, USkeletalMeshComponent* DefaultSource, float DamageMultiplier, const FGameplayTagContainer& AttackTags);
	FWindow* FindWindow(int32 WindowId);
	void FlushDeferredOperations();

	bool ShouldDetect() const;
	USceneComponent* ResolveSource(const FEGMeleeHitShape& Shape, USkeletalMeshComponent* DefaultSource) const;
	void SamplePoints(const FEGMeleeHitShape& Shape, const USceneComponent* Source, TArray<FVector>& OutPoints) const;

	/** Sweeps the window's shapes from last frame's points to this frame's and records new hits. */
	void SweepWindow(FWindow& Window, TArray<FEGMeleeHit>& OutHits) const;
	void DeliverHits(const TArray<FEGMeleeHit>& Hits);
	void UpdateTickEnabled();

	TArray<FWindow> Windows;
	TMap<FGameplayTag, TWeakObjectPtr<USceneComponent>> SourceOverrides;

	/** True while a sweep holds a reference into Windows; Open/Close calls made then are deferred. */
	bool bSweeping = false;
	TArray<TFunction<void()>> DeferredOperations;
};
