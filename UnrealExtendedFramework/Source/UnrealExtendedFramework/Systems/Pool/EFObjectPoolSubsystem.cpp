// Copyright Moon Punch Games. All Rights Reserved.

#include "Systems/Pool/EFObjectPoolSubsystem.h"

#include "Components/ActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/MovementComponent.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Subsystems/EFSubsystems.h"
#include "Systems/Pool/EFObjectPoolSettings.h"
#include "Systems/Pool/EFPoolableInterface.h"
#include "TimerManager.h"
#include "UnrealExtendedFramework.h"

namespace EFObjectPoolPrivate
{
	void NotifyPoolable(AActor* Actor, bool bAcquired)
	{
		auto Notify = [bAcquired](UObject* Object)
		{
			if (Object && Object->GetClass()->ImplementsInterface(UEFPoolable::StaticClass()))
			{
				bAcquired ? IEFPoolable::Execute_OnAcquiredFromPool(Object) : IEFPoolable::Execute_OnReleasedToPool(Object);
			}
		};
		Notify(Actor);
		TInlineComponentArray<UActorComponent*> Components(Actor);
		for (UActorComponent* Component : Components)
		{
			Notify(Component);
		}
	}

	FAutoConsoleCommandWithWorldAndArgs StatsCommand(
		TEXT("Extended.Pool.Stats"),
		TEXT("Prints the object pool counters of the current world: active, available, spawned, reused, overflowed per class."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			const UEFObjectPoolSubsystem* Pool = World ? World->GetSubsystem<UEFObjectPoolSubsystem>() : nullptr;
			if (!Pool)
			{
				UE_LOG(LogExtendedFramework, Display, TEXT("Extended.Pool.Stats: no object pool in this world."));
				return;
			}
			const TArray<FEFPoolStats> Stats = Pool->GetPoolStats();
			UE_LOG(LogExtendedFramework, Display, TEXT("Extended.Pool.Stats: %d pooled class(es)"), Stats.Num());
			for (const FEFPoolStats& Entry : Stats)
			{
				UE_LOG(LogExtendedFramework, Display, TEXT("  %-40s active %3d  available %3d/%-3d  spawned %4d  reused %5d  overflowed %4d"),
					*Entry.ClassName, Entry.Active, Entry.Available, Entry.MaxPooled, Entry.Spawned, Entry.Reused, Entry.Overflowed);
			}
		}));
}

const FEFPoolClassSettings* UEFObjectPoolSettings::FindClassSettings(const UClass* Class) const
{
	return Classes.FindByPredicate([Class](const FEFPoolClassSettings& Entry)
	{
		return Class && Entry.ActorClass.ToSoftObjectPath() == FSoftObjectPath(Class);
	});
}

UEFObjectPoolSubsystem* UEFObjectPoolSubsystem::Get(const UObject* WorldContextObject)
{
	return EF::GetWorldSubsystem<UEFObjectPoolSubsystem>(WorldContextObject);
}

bool UEFObjectPoolSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UEFObjectPoolSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	for (const FEFPoolClassSettings& Entry : UEFObjectPoolSettings::Get()->Classes)
	{
		if (Entry.PrewarmCount > 0)
		{
			if (UClass* Class = Entry.ActorClass.LoadSynchronous())
			{
				Prewarm(Class, Entry.PrewarmCount);
			}
		}
	}
}

void UEFObjectPoolSubsystem::Deinitialize()
{
	// The world is going away and takes its actors with it; only forget them.
	Pools.Reset();
	Tracked.Reset();
	Super::Deinitialize();
}

UEFObjectPoolSubsystem::FPool& UEFObjectPoolSubsystem::FindOrAddPool(UClass* Class)
{
	return Pools.FindOrAdd(Class);
}

int32 UEFObjectPoolSubsystem::GetMaxPooled(UClass* Class) const
{
	if (const FPool* Pool = Pools.Find(Class))
	{
		if (Pool->MaxPooledOverride >= 0)
		{
			return Pool->MaxPooledOverride;
		}
	}
	const UEFObjectPoolSettings* Settings = UEFObjectPoolSettings::Get();
	const FEFPoolClassSettings* ClassSettings = Settings->FindClassSettings(Class);
	return ClassSettings ? ClassSettings->MaxPooled : Settings->DefaultMaxPooled;
}

AActor* UEFObjectPoolSubsystem::SpawnForPool(UClass* Class, const FTransform& Transform, AActor* Owner, APawn* Instigator)
{
	UWorld* World = GetWorld();
	if (!World || !Class)
	{
		return nullptr;
	}

	FActorSpawnParameters Parameters;
	Parameters.Owner = Owner;
	Parameters.Instigator = Instigator;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Actor = World->SpawnActor<AActor>(Class, Transform, Parameters);
	if (!Actor)
	{
		UE_LOG(LogExtendedFramework, Warning, TEXT("Object pool: %s could not be spawned."), *Class->GetName());
		return nullptr;
	}

	FActorState& State = Tracked.Add(Actor);
	State.Class = Class;
	State.bWasHidden = Actor->IsHidden();
	State.bCollisionWasEnabled = Actor->GetActorEnableCollision();
	State.bTickWasEnabled = Actor->IsActorTickEnabled();
	TInlineComponentArray<UActorComponent*> Components(Actor);
	for (UActorComponent* Component : Components)
	{
		if (Component && Component->IsActive())
		{
			State.ActiveComponents.Add(Component);
		}
	}

	Actor->OnDestroyed.AddUniqueDynamic(this, &UEFObjectPoolSubsystem::HandlePooledActorDestroyed);
	++FindOrAddPool(Class).Spawned;
	return Actor;
}

AActor* UEFObjectPoolSubsystem::AcquireActor(TSubclassOf<AActor> ActorClass, const FTransform& Transform, AActor* Owner, APawn* Instigator, float LifeSeconds)
{
	UClass* Class = ActorClass.Get();
	if (!Class || !GetWorld())
	{
		return nullptr;
	}

	AActor* Actor = nullptr;
	{
		FPool& Pool = FindOrAddPool(Class);
		while (!Actor && Pool.Available.Num() > 0)
		{
			AActor* Candidate = Pool.Available.Pop().Get();
			if (IsValid(Candidate) && !Candidate->IsActorBeingDestroyed() && Tracked.Contains(Candidate))
			{
				Actor = Candidate;
			}
		}
		if (Actor)
		{
			++Pool.Reused;
		}
	}

	if (Actor)
	{
		Actor->SetOwner(Owner);
		Actor->SetInstigator(Instigator);
		Actor->SetActorTransform(Transform, /*bSweep*/ false, nullptr, ETeleportType::ResetPhysics);
		Activate(Actor, Tracked.FindChecked(Actor));
	}
	else
	{
		Actor = SpawnForPool(Class, Transform, Owner, Instigator);
		if (!Actor)
		{
			return nullptr;
		}
	}

	FActorState& State = Tracked.FindChecked(Actor);
	State.bAcquired = true;
	++FindOrAddPool(Class).Active;

	if (LifeSeconds > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(State.LifeTimer,
			FTimerDelegate::CreateUObject(this, &UEFObjectPoolSubsystem::HandleLifeExpired, TWeakObjectPtr<AActor>(Actor)), LifeSeconds, false);
	}

	NotifyAcquired(Actor);
	return Actor;
}

void UEFObjectPoolSubsystem::ReleaseActor(AActor* Actor)
{
	if (!IsValid(Actor) || Actor->IsActorBeingDestroyed())
	{
		return;
	}

	FActorState* State = Tracked.Find(Actor);
	if (!State)
	{
		// Not ours: releasing means "done with it", which for a normal actor is Destroy.
		Actor->Destroy();
		return;
	}
	if (!State->bAcquired)
	{
		return;  // already in the pool
	}

	State->bAcquired = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(State->LifeTimer);
	}

	UClass* Class = State->Class.Get() ? State->Class.Get() : Actor->GetClass();
	FPool& Pool = FindOrAddPool(Class);
	Pool.Active = FMath::Max(Pool.Active - 1, 0);

	NotifyReleased(Actor);
	Deactivate(Actor);

	// Pruned here so a pool of destroyed entries does not count against the limit.
	Pool.Available.RemoveAll([](const TWeakObjectPtr<AActor>& Weak) { return !Weak.IsValid(); });
	if (Pool.Available.Num() >= GetMaxPooled(Class))
	{
		++Pool.Overflowed;
		Tracked.Remove(Actor);
		Actor->OnDestroyed.RemoveDynamic(this, &UEFObjectPoolSubsystem::HandlePooledActorDestroyed);
		Actor->Destroy();
		return;
	}
	Pool.Available.Add(Actor);
}

void UEFObjectPoolSubsystem::Prewarm(TSubclassOf<AActor> ActorClass, int32 Count)
{
	UClass* Class = ActorClass.Get();
	if (!Class || Count <= 0)
	{
		return;
	}

	const int32 Target = FMath::Min(Count, FMath::Max(GetMaxPooled(Class), 0));
	while (FindOrAddPool(Class).Available.Num() < Target)
	{
		AActor* Actor = SpawnForPool(Class, FTransform::Identity, nullptr, nullptr);
		if (!Actor)
		{
			return;
		}
		NotifyReleased(Actor);
		Deactivate(Actor);
		FindOrAddPool(Class).Available.Add(Actor);
	}
}

void UEFObjectPoolSubsystem::SetMaxPooled(TSubclassOf<AActor> ActorClass, int32 MaxPooled)
{
	if (UClass* Class = ActorClass.Get())
	{
		FindOrAddPool(Class).MaxPooledOverride = FMath::Max(MaxPooled, 0);
	}
}

void UEFObjectPoolSubsystem::ClearPool(TSubclassOf<AActor> ActorClass)
{
	UClass* OnlyClass = ActorClass.Get();
	for (TPair<TWeakObjectPtr<UClass>, FPool>& Pair : Pools)
	{
		if (OnlyClass && Pair.Key.Get() != OnlyClass)
		{
			continue;
		}
		TArray<TWeakObjectPtr<AActor>> Available = MoveTemp(Pair.Value.Available);
		Pair.Value.Available.Reset();
		for (const TWeakObjectPtr<AActor>& Weak : Available)
		{
			if (AActor* Actor = Weak.Get())
			{
				Tracked.Remove(Actor);
				Actor->OnDestroyed.RemoveDynamic(this, &UEFObjectPoolSubsystem::HandlePooledActorDestroyed);
				Actor->Destroy();
			}
		}
	}
}

bool UEFObjectPoolSubsystem::IsPooledActor(const AActor* Actor) const
{
	return Actor && Tracked.Contains(Actor);
}

bool UEFObjectPoolSubsystem::IsActorAcquired(const AActor* Actor) const
{
	const FActorState* State = Actor ? Tracked.Find(Actor) : nullptr;
	return State && State->bAcquired;
}

TArray<FEFPoolStats> UEFObjectPoolSubsystem::GetPoolStats() const
{
	TArray<FEFPoolStats> Result;
	for (const TPair<TWeakObjectPtr<UClass>, FPool>& Pair : Pools)
	{
		UClass* Class = Pair.Key.Get();
		if (!Class)
		{
			continue;
		}
		FEFPoolStats& Stats = Result.AddDefaulted_GetRef();
		Stats.ClassName = Class->GetName();
		Stats.Active = Pair.Value.Active;
		Stats.Available = Pair.Value.Available.FilterByPredicate([](const TWeakObjectPtr<AActor>& Weak) { return Weak.IsValid(); }).Num();
		Stats.Spawned = Pair.Value.Spawned;
		Stats.Reused = Pair.Value.Reused;
		Stats.Overflowed = Pair.Value.Overflowed;
		Stats.MaxPooled = GetMaxPooled(Class);
	}
	Result.Sort([](const FEFPoolStats& A, const FEFPoolStats& B) { return A.ClassName < B.ClassName; });
	return Result;
}

void UEFObjectPoolSubsystem::Activate(AActor* Actor, const FActorState& State)
{
	Actor->SetActorHiddenInGame(State.bWasHidden);
	Actor->SetActorEnableCollision(State.bCollisionWasEnabled);
	Actor->SetActorTickEnabled(State.bTickWasEnabled);
	for (const TWeakObjectPtr<UActorComponent>& Weak : State.ActiveComponents)
	{
		if (UActorComponent* Component = Weak.Get())
		{
			Component->Activate(/*bReset*/ true);
		}
	}
}

void UEFObjectPoolSubsystem::Deactivate(AActor* Actor)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(Actor);
	}
	Actor->SetLifeSpan(0.f);
	Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Actor->SetOwner(nullptr);
	Actor->SetActorHiddenInGame(true);
	Actor->SetActorEnableCollision(false);
	Actor->SetActorTickEnabled(false);

	TInlineComponentArray<UActorComponent*> Components(Actor);
	for (UActorComponent* Component : Components)
	{
		if (!Component)
		{
			continue;
		}
		if (UMovementComponent* Movement = Cast<UMovementComponent>(Component))
		{
			Movement->StopMovementImmediately();
		}
		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
		{
			if (Primitive->IsSimulatingPhysics())
			{
				Primitive->SetPhysicsLinearVelocity(FVector::ZeroVector);
				Primitive->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
			}
		}
		if (Component->IsActive())
		{
			Component->Deactivate();
		}
	}
}

void UEFObjectPoolSubsystem::NotifyAcquired(AActor* Actor)
{
	EFObjectPoolPrivate::NotifyPoolable(Actor, /*bAcquired*/ true);
}

void UEFObjectPoolSubsystem::NotifyReleased(AActor* Actor)
{
	EFObjectPoolPrivate::NotifyPoolable(Actor, /*bAcquired*/ false);
}

void UEFObjectPoolSubsystem::HandleLifeExpired(TWeakObjectPtr<AActor> Actor)
{
	if (AActor* Expired = Actor.Get())
	{
		ReleaseActor(Expired);
	}
}

void UEFObjectPoolSubsystem::HandlePooledActorDestroyed(AActor* DestroyedActor)
{
	FActorState State;
	if (!Tracked.RemoveAndCopyValue(DestroyedActor, State))
	{
		return;
	}
	if (FPool* Pool = Pools.Find(State.Class))
	{
		if (State.bAcquired)
		{
			Pool->Active = FMath::Max(Pool->Active - 1, 0);
		}
		Pool->Available.RemoveAll([DestroyedActor](const TWeakObjectPtr<AActor>& Weak) { return !Weak.IsValid() || Weak.Get() == DestroyedActor; });
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(State.LifeTimer);
	}
}
