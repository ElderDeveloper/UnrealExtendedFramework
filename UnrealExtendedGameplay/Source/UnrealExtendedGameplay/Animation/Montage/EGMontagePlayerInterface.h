// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"

#include "EGMontagePlayerInterface.generated.h"

class UAnimMontage;
class UEGAnimationSet;
class USkeletalMeshComponent;

UINTERFACE(BlueprintType, MinimalAPI)
class UEGMontagePlayerInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * IEGMontagePlayerInterface
 *
 * Optional, on any actor the plugin plays montages on (cover, interaction, chess, the montage
 * library and async nodes). Nothing is required: without it montages play on ACharacter::GetMesh()
 * (or the first skeletal mesh) through its anim instance.
 */
class UNREALEXTENDEDGAMEPLAY_API IEGMontagePlayerInterface
{
	GENERATED_BODY()

public:
	/**
	 * The mesh montages for this purpose play on (ExtendedGameplay.Anim.Mesh.Body,
	 * ExtendedGameplay.Anim.Mesh.FirstPerson, or a game's own tag). Null uses the default.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Montage")
	USkeletalMeshComponent* GetMontageMesh(FGameplayTag Purpose) const;

	/** This character's animations by tag. Null leaves it to the caller's own set, if any. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Montage")
	UEGAnimationSet* GetAnimationSet() const;

	/**
	 * Take over playback (GAS, a replicated montage system, a custom animation layer). Return true
	 * when handled; the caller then plays nothing itself, and only tracks the montage's events
	 * if it ends up playing on GetMontageMesh's anim instance.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Montage")
	bool PlayMontageOverride(UAnimMontage* Montage, float PlayRate, FName StartSection, float StartTime, FGameplayTag Purpose);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Montage")
	bool StopMontageOverride(UAnimMontage* Montage, float BlendOutTime, FGameplayTag Purpose);

	// Defaults, so a native class only overrides what it needs.
	virtual USkeletalMeshComponent* GetMontageMesh_Implementation(FGameplayTag Purpose) const { return nullptr; }
	virtual UEGAnimationSet* GetAnimationSet_Implementation() const { return nullptr; }
	virtual bool PlayMontageOverride_Implementation(UAnimMontage* Montage, float PlayRate, FName StartSection, float StartTime, FGameplayTag Purpose) { return false; }
	virtual bool StopMontageOverride_Implementation(UAnimMontage* Montage, float BlendOutTime, FGameplayTag Purpose) { return false; }
};
