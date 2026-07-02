#include "EGASGameplayAbility.h"

#include "EGASAbilitySystemComponent.h"

UEGASGameplayAbility::UEGASGameplayAbility(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerInitiated;
}

UEGASAbilitySystemComponent* UEGASGameplayAbility::GetEGASAbilitySystemComponentFromActorInfo() const
{
	return CurrentActorInfo ? Cast<UEGASAbilitySystemComponent>(CurrentActorInfo->AbilitySystemComponent.Get()) : nullptr;
}

AActor* UEGASGameplayAbility::GetAvatarActorFromActorInfo() const
{
	return CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
}

AActor* UEGASGameplayAbility::GetOwnerActorFromActorInfo() const
{
	return CurrentActorInfo ? CurrentActorInfo->OwnerActor.Get() : nullptr;
}
