// Copyright Moon Punch Games. All Rights Reserved.

#include "Systems/Pool/EFObjectPoolLibrary.h"

#include "GameFramework/Actor.h"

AActor* UEFObjectPoolLibrary::AcquirePooledActor(const UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, const FTransform& Transform,
	float LifeSeconds, AActor* Owner, APawn* Instigator)
{
	UEFObjectPoolSubsystem* Pool = UEFObjectPoolSubsystem::Get(WorldContextObject);
	return Pool ? Pool->AcquireActor(ActorClass, Transform, Owner, Instigator, LifeSeconds) : nullptr;
}

void UEFObjectPoolLibrary::ReleasePooledActor(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return;
	}
	if (UEFObjectPoolSubsystem* Pool = UEFObjectPoolSubsystem::Get(Actor))
	{
		Pool->ReleaseActor(Actor);
	}
	else
	{
		Actor->Destroy();
	}
}

void UEFObjectPoolLibrary::PrewarmPool(const UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, int32 Count)
{
	if (UEFObjectPoolSubsystem* Pool = UEFObjectPoolSubsystem::Get(WorldContextObject))
	{
		Pool->Prewarm(ActorClass, Count);
	}
}

bool UEFObjectPoolLibrary::IsPooledActor(const AActor* Actor)
{
	const UEFObjectPoolSubsystem* Pool = Actor ? UEFObjectPoolSubsystem::Get(Actor) : nullptr;
	return Pool && Pool->IsPooledActor(Actor);
}
