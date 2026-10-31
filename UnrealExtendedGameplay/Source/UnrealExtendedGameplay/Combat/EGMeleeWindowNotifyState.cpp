// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGMeleeWindowNotifyState.h"

#include "Components/SkeletalMeshComponent.h"
#include "EGMeleeTraceComponent.h"
#include "Engine/World.h"
#include "UnrealExtendedGameplay/Animation/Debug/EGAnimDebug.h"
#include "UnrealExtendedGameplay/Animation/EGAnimationTags.h"

FGameplayTag UEGMeleeWindowNotifyState::GetEventTag() const
{
	return WindowTag.IsValid() ? WindowTag : FGameplayTag(TAG_EGMelee_Window);
}

FEGAnimEventPayload UEGMeleeWindowNotifyState::GetEventPayload() const
{
	FEGAnimEventPayload Payload;
	Payload.Magnitude = DamageMultiplier;
	Payload.ExtraTags = AttackTags;
	return Payload;
}

void UEGMeleeWindowNotifyState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	const AActor* const Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	const UWorld* const World = MeshComp ? MeshComp->GetWorld() : nullptr;
	if (Owner && World && World->IsGameWorld() && !Owner->FindComponentByClass<UEGMeleeTraceComponent>())
	{
		EGAnimDebug::WarnOnce(this, FString::Printf(TEXT("%s plays a Melee Window but has no UEGMeleeTraceComponent; nothing is traced."), *GetNameSafe(Owner->GetClass())));
	}
}
