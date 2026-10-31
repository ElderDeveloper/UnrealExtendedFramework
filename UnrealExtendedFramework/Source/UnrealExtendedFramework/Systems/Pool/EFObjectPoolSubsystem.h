// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/SubclassOf.h"
#include "EFObjectPoolSubsystem.generated.h"

class AActor;
class APawn;
class UActorComponent;
struct FStreamableHandle;

/** Counters for one pooled class, for debug UI and Extended.Pool.Stats. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDFRAMEWORK_API FEFPoolStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Pool")
	FString ClassName;

	/** Acquired and not yet released. */
	UPROPERTY(BlueprintReadOnly, Category = "Pool")
	int32 Active = 0;

	/** Hidden in the pool, ready for the next acquire. */
	UPROPERTY(BlueprintReadOnly, Category = "Pool")
	int32 Available = 0;

	/** Actors the pool had to spawn. */
	UPROPERTY(BlueprintReadOnly, Category = "Pool")
	int32 Spawned = 0;

	/** Acquires served by a pooled actor instead of a spawn. */
	UPROPERTY(BlueprintReadOnly, Category = "Pool")
	int32 Reused = 0;

	/** Released actors destroyed because the pool was full. */
	UPROPERTY(BlueprintReadOnly, Category = "Pool")
	int32 Overflowed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Pool")
	int32 MaxPooled = 0;
};

/**
 * Reuses actors instead of spawning and destroying them: projectiles, impact effects, decals,
 * damage numbers, pickups, enemies of a wave.
 *
 *     AMyProjectile* Shot = UEFObjectPoolSubsystem::Get(this)->Acquire<AMyProjectile>(ProjectileClass, Muzzle, this, Pawn, 5.f);
 *     ...
 *     UEFObjectPoolSubsystem::Get(this)->ReleaseActor(Shot);   // instead of Destroy
 *
 * - Acquire places the actor, shows it, restores its collision and tick and reactivates the
 *   components that were active when it was first spawned, then calls IEFPoolable::OnAcquiredFromPool.
 * - Release calls OnReleasedToPool, then hides it, turns collision and tick off, deactivates its
 *   components, stops movement and physics, detaches it and clears its timers.
 * - LifeSeconds releases the actor automatically, like a pooled SetLifeSpan.
 * - Releasing an actor the pool did not spawn destroys it, so Release is always safe to call.
 * - An actor destroyed while pooled simply leaves the pool.
 *
 * One pool per world. Designed for actors that are not replicated or that only the server
 * spawns; a pooled replicated actor stays relevant while hidden, so set it dormant on release if
 * that matters. Niagara systems have their own pooling (ENCPoolMethod); use that for effects
 * that are only a Niagara component.
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFObjectPoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UEFObjectPoolSubsystem* Get(const UObject* WorldContextObject);

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	/** Takes an actor of ActorClass from the pool, or spawns one. LifeSeconds > 0 releases it after that long. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Pool", meta = (DeterminesOutputType = "ActorClass", AdvancedDisplay = "Owner,Instigator"))
	AActor* AcquireActor(TSubclassOf<AActor> ActorClass, const FTransform& Transform, AActor* Owner = nullptr, APawn* Instigator = nullptr, float LifeSeconds = 0.f);

	template <typename TActor>
	TActor* Acquire(TSubclassOf<TActor> ActorClass, const FTransform& Transform, AActor* Owner = nullptr, APawn* Instigator = nullptr, float LifeSeconds = 0.f)
	{
		return Cast<TActor>(AcquireActor(ActorClass.Get(), Transform, Owner, Instigator, LifeSeconds));
	}

	/** Returns the actor to its pool. Actors the pool did not spawn are destroyed. Releasing twice is harmless. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Pool")
	void ReleaseActor(AActor* Actor);

	/** Spawns hidden actors until Count are available. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Pool")
	void Prewarm(TSubclassOf<AActor> ActorClass, int32 Count);

	/** Inactive actors kept at most for ActorClass, overriding the settings for this world. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Pool")
	void SetMaxPooled(TSubclassOf<AActor> ActorClass, int32 MaxPooled);

	/** Destroys the inactive actors of ActorClass, or of every class when None. Active actors are kept. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Pool")
	void ClearPool(TSubclassOf<AActor> ActorClass);

	/** True when the pool spawned the actor (active or not). */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Pool")
	bool IsPooledActor(const AActor* Actor) const;

	/** True when the pool spawned the actor and it is currently acquired. */
	UFUNCTION(BlueprintPure, Category = "Extended Framework|Pool")
	bool IsActorAcquired(const AActor* Actor) const;

	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Pool")
	TArray<FEFPoolStats> GetPoolStats() const;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	struct FActorState
	{
		TWeakObjectPtr<UClass> Class;
		bool bAcquired = false;
		bool bWasHidden = false;
		bool bCollisionWasEnabled = true;
		bool bTickWasEnabled = true;
		/** Components active when the actor was first spawned; reactivated on every acquire. */
		TArray<TWeakObjectPtr<UActorComponent>> ActiveComponents;
		FTimerHandle LifeTimer;
	};

	struct FPool
	{
		TArray<TWeakObjectPtr<AActor>> Available;
		int32 Active = 0;
		int32 Spawned = 0;
		int32 Reused = 0;
		int32 Overflowed = 0;
		/** -1: from the settings. */
		int32 MaxPooledOverride = -1;
	};

	/** Prewarms the classes listed in the settings, once the world's actors have begun play. */
	void PrewarmFromSettings();
	FPool& FindOrAddPool(UClass* Class);
	int32 GetMaxPooled(UClass* Class) const;
	AActor* SpawnForPool(UClass* Class, const FTransform& Transform, AActor* Owner, APawn* Instigator);
	void Activate(AActor* Actor, const FActorState& State);
	void Deactivate(AActor* Actor);
	void NotifyAcquired(AActor* Actor);
	void NotifyReleased(AActor* Actor);
	void HandleLifeExpired(TWeakObjectPtr<AActor> Actor);

	UFUNCTION()
	void HandlePooledActorDestroyed(AActor* DestroyedActor);

	TMap<TWeakObjectPtr<UClass>, FPool> Pools;
	TMap<TWeakObjectPtr<AActor>, FActorState> Tracked;

	/** Loads prewarm classes that were not loaded at begin play; they are prewarmed when it finishes. */
	TSharedPtr<FStreamableHandle> PrewarmLoadHandle;
};
