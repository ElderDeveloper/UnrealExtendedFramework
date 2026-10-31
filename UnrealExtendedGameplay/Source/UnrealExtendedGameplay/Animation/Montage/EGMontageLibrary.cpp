// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGMontageLibrary.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Assets/EFAssetLoading.h"
#include "Components/SkeletalMeshComponent.h"
#include "EGMontagePlayerInterface.h"
#include "GameFramework/Character.h"

USkeletalMeshComponent* UEGMontageLibrary::GetMontageMesh(const AActor* Actor, FGameplayTag Purpose)
{
	if (!IsValid(Actor))
	{
		return nullptr;
	}

	if (Actor->Implements<UEGMontagePlayerInterface>())
	{
		if (USkeletalMeshComponent* Mesh = IEGMontagePlayerInterface::Execute_GetMontageMesh(const_cast<AActor*>(Actor), Purpose))
		{
			return Mesh;
		}
	}

	if (const ACharacter* Character = Cast<ACharacter>(Actor))
	{
		if (USkeletalMeshComponent* Mesh = Character->GetMesh())
		{
			return Mesh;
		}
	}
	return Actor->FindComponentByClass<USkeletalMeshComponent>();
}

UEGAnimationSet* UEGMontageLibrary::GetAnimationSet(const AActor* Actor, UEGAnimationSet* Fallback)
{
	if (IsValid(Actor) && Actor->Implements<UEGMontagePlayerInterface>())
	{
		if (UEGAnimationSet* Set = IEGMontagePlayerInterface::Execute_GetAnimationSet(const_cast<AActor*>(Actor)))
		{
			return Set;
		}
	}
	return Fallback;
}

void UEGMontageLibrary::GetAnimationSets(const AActor* Actor, UEGAnimationSet* Fallback, TArray<const UEGAnimationSet*, TInlineAllocator<2>>& OutSets)
{
	OutSets.Reset();
	if (const UEGAnimationSet* ActorSet = GetAnimationSet(Actor, nullptr))
	{
		OutSets.Add(ActorSet);
	}
	if (Fallback && !OutSets.Contains(Fallback))
	{
		OutSets.Add(Fallback);
	}
}

FEGResolvedAnimation UEGMontageLibrary::ResolveAnimation(const AActor* Actor, FGameplayTag AnimationTag, int32 VariantIndex, UEGAnimationSet* Fallback, bool bExactTag)
{
	TArray<const UEGAnimationSet*, TInlineAllocator<2>> Sets;
	GetAnimationSets(Actor, Fallback, Sets);

	FEGResolvedAnimation Resolved;
	TSoftObjectPtr<UAnimMontage> Montage;
	FEGAnimationSetEntry Entry;
	if (UEGAnimationSet::PickVariantInSets(Sets, AnimationTag, bExactTag, VariantIndex, Montage, Entry))
	{
		// Normally already loaded: each animation set preloads its montages when it loads.
		Resolved.Montage = EFAssets::Resolve(Montage, TEXT("UEGMontageLibrary::ResolveAnimation"));
		Resolved.StartSection = Entry.StartSection;
		Resolved.PlayRate = Entry.PlayRate;
		Resolved.StartTime = Entry.StartTime;
	}
	return Resolved;
}

EEGMontagePlayResult UEGMontageLibrary::PlayMontage(AActor* Actor, UAnimMontage* Montage, float PlayRate, FName StartSection, float StartTime, FGameplayTag Purpose, UAnimInstance*& OutAnimInstance)
{
	OutAnimInstance = nullptr;
	if (!IsValid(Actor) || !Montage)
	{
		return EEGMontagePlayResult::Failed;
	}

	USkeletalMeshComponent* const Mesh = GetMontageMesh(Actor, Purpose);
	OutAnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;

	if (Actor->Implements<UEGMontagePlayerInterface>() && IEGMontagePlayerInterface::Execute_PlayMontageOverride(Actor, Montage, PlayRate, StartSection, StartTime, Purpose))
	{
		return EEGMontagePlayResult::HandledByOwner;
	}

	if (!OutAnimInstance || OutAnimInstance->Montage_Play(Montage, PlayRate, EMontagePlayReturnType::MontageLength, StartTime) <= 0.0f)
	{
		return EEGMontagePlayResult::Failed;
	}

	if (!StartSection.IsNone())
	{
		OutAnimInstance->Montage_JumpToSection(StartSection, Montage);
	}
	return EEGMontagePlayResult::Played;
}

EEGMontagePlayResult UEGMontageLibrary::PlayMontageOnActor(AActor* Actor, UAnimMontage* Montage, float PlayRate, FName StartSection, float StartTime, FGameplayTag Purpose)
{
	UAnimInstance* AnimInstance = nullptr;
	return PlayMontage(Actor, Montage, PlayRate, StartSection, StartTime, Purpose, AnimInstance);
}

EEGMontagePlayResult UEGMontageLibrary::PlayAnimationOnActor(AActor* Actor, FGameplayTag AnimationTag, int32 VariantIndex, FGameplayTag Purpose, UEGAnimationSet* Fallback)
{
	const FEGResolvedAnimation Resolved = ResolveAnimation(Actor, AnimationTag, VariantIndex, Fallback);
	if (!Resolved.IsValid())
	{
		return EEGMontagePlayResult::Failed;
	}
	return PlayMontageOnActor(Actor, Resolved.Montage, Resolved.PlayRate, Resolved.StartSection, Resolved.StartTime, Purpose);
}

bool UEGMontageLibrary::StopMontageOnActor(AActor* Actor, UAnimMontage* Montage, float BlendOutTime, FGameplayTag Purpose)
{
	if (!IsValid(Actor))
	{
		return false;
	}

	if (Actor->Implements<UEGMontagePlayerInterface>() && IEGMontagePlayerInterface::Execute_StopMontageOverride(Actor, Montage, BlendOutTime, Purpose))
	{
		return true;
	}

	USkeletalMeshComponent* const Mesh = GetMontageMesh(Actor, Purpose);
	UAnimInstance* const AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return false;
	}

	if (Montage)
	{
		AnimInstance->Montage_Stop(BlendOutTime, Montage);
	}
	else
	{
		AnimInstance->StopAllMontages(BlendOutTime);
	}
	return true;
}
