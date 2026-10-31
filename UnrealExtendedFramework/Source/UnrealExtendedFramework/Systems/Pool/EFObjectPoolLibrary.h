// Copyright Moon Punch Games. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Systems/Pool/EFObjectPoolSubsystem.h"
#include "EFObjectPoolLibrary.generated.h"

/** Object pool nodes that work from any world context. */
UCLASS(meta = (ScriptName = "EFObjectPoolLibrary"))
class UNREALEXTENDEDFRAMEWORK_API UEFObjectPoolLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Spawn Actor, from a pool: reuses a released actor of the class when there is one. Life
	 * Seconds > 0 releases it after that long. Put per-use setup in On Acquired From Pool.
	 */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Pool", meta = (WorldContext = "WorldContextObject", DeterminesOutputType = "ActorClass", AdvancedDisplay = "Owner,Instigator"))
	static AActor* AcquirePooledActor(const UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, const FTransform& Transform,
		float LifeSeconds = 0.f, AActor* Owner = nullptr, APawn* Instigator = nullptr);

	/** Destroy Actor, for pooled actors: returns it to its pool. Destroys actors the pool did not spawn. */
	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Pool", meta = (DefaultToSelf = "Actor"))
	static void ReleasePooledActor(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "Extended Framework|Pool", meta = (WorldContext = "WorldContextObject"))
	static void PrewarmPool(const UObject* WorldContextObject, TSubclassOf<AActor> ActorClass, int32 Count);

	UFUNCTION(BlueprintPure, Category = "Extended Framework|Pool", meta = (DefaultToSelf = "Actor"))
	static bool IsPooledActor(const AActor* Actor);
};
