// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGAnimationSet.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "EGMontageLibrary.generated.h"

class UAnimInstance;
class UAnimMontage;
class USkeletalMeshComponent;

UENUM(BlueprintType)
enum class EEGMontagePlayResult : uint8
{
	Failed,
	/** Played through the mesh's anim instance. */
	Played,
	/** The actor's IEGMontagePlayerInterface::PlayMontageOverride took it. */
	HandledByOwner
};

/**
 * One way to find the right mesh, animation set and playback path for an actor, used by every
 * system in the plugin that plays montages, so IEGMontagePlayerInterface works everywhere.
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGMontageLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** IEGMontagePlayerInterface::GetMontageMesh, else ACharacter::GetMesh, else the first skeletal mesh. */
	UFUNCTION(BlueprintPure, Category = "Extended|Montage")
	static USkeletalMeshComponent* GetMontageMesh(const AActor* Actor, FGameplayTag Purpose);

	/** IEGMontagePlayerInterface::GetAnimationSet, else Fallback. */
	UFUNCTION(BlueprintPure, Category = "Extended|Montage")
	static UEGAnimationSet* GetAnimationSet(const AActor* Actor, UEGAnimationSet* Fallback = nullptr);

	/**
	 * This tag across the actor's set and Fallback: the most specific tag found in either wins, the
	 * actor's set first for the same tag. bExactTag skips parent tags. Loads the montage if needed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Extended|Montage", meta = (AdvancedDisplay = "bExactTag"))
	static FEGResolvedAnimation ResolveAnimation(const AActor* Actor, FGameplayTag AnimationTag, int32 VariantIndex = -1, UEGAnimationSet* Fallback = nullptr, bool bExactTag = false);

	/** The sets ResolveAnimation searches, in order: the actor's, then Fallback (when different). */
	static void GetAnimationSets(const AActor* Actor, UEGAnimationSet* Fallback, TArray<const UEGAnimationSet*, TInlineAllocator<2>>& OutSets);

	UFUNCTION(BlueprintCallable, Category = "Extended|Montage", meta = (AdvancedDisplay = "StartSection,StartTime,Purpose"))
	static EEGMontagePlayResult PlayMontageOnActor(AActor* Actor, UAnimMontage* Montage, float PlayRate = 1.0f, FName StartSection = NAME_None, float StartTime = 0.0f, FGameplayTag Purpose = FGameplayTag());

	UFUNCTION(BlueprintCallable, Category = "Extended|Montage", meta = (AdvancedDisplay = "VariantIndex,Purpose,Fallback"))
	static EEGMontagePlayResult PlayAnimationOnActor(AActor* Actor, FGameplayTag AnimationTag, int32 VariantIndex = -1, FGameplayTag Purpose = FGameplayTag(), UEGAnimationSet* Fallback = nullptr);

	/** Null Montage stops every montage on the mesh. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Montage")
	static bool StopMontageOnActor(AActor* Actor, UAnimMontage* Montage, float BlendOutTime = 0.25f, FGameplayTag Purpose = FGameplayTag());

	/** For C++ callers that bind montage delegates: also returns the anim instance the montage plays on, when there is one. */
	static EEGMontagePlayResult PlayMontage(AActor* Actor, UAnimMontage* Montage, float PlayRate, FName StartSection, float StartTime, FGameplayTag Purpose, UAnimInstance*& OutAnimInstance);
};
