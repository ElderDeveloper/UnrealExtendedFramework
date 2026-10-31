// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGFootstepNotify.h"

#include "Components/SkeletalMeshComponent.h"
#include "EGFootstepComponent.h"
#include "Engine/World.h"
#include "Perception/AISense_Hearing.h"
#include "UnrealExtendedGameplay/Animation/Debug/EGAnimDebug.h"
#include "UnrealExtendedGameplay/Animation/EGAnimationTags.h"

FGameplayTag UEGFootstepNotify::GetEventTag() const
{
	return TAG_EGAnim_Footstep;
}

FEGAnimEventPayload UEGFootstepNotify::GetEventPayload() const
{
	FEGAnimEventPayload Payload;
	Payload.SocketName = FootSocket;
	Payload.Magnitude = NoiseLoudness;
	return Payload;
}

FString UEGFootstepNotify::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Footstep (%s)"), *FootSocket.ToString());
}

void UEGFootstepNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	AActor* const Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	UWorld* const World = MeshComp ? MeshComp->GetWorld() : nullptr;
	if (!Owner || !World)
	{
		return;
	}

	if (World->IsGameWorld() && !MeshComp->DoesSocketExist(FootSocket))
	{
		EGAnimDebug::WarnOnce(this, FString::Printf(TEXT("Footstep socket %s does not exist on %s."), *FootSocket.ToString(), *GetNameSafe(MeshComp->GetSkeletalMeshAsset())));
	}

	const FVector FootLocation = MeshComp->GetSocketLocation(FootSocket);

	if (World->GetNetMode() != NM_DedicatedServer)
	{
		if (UEGFootstepComponent* const Footsteps = Owner->FindComponentByClass<UEGFootstepComponent>())
		{
			Footsteps->SpawnFootStepEvents(FootLocation);
		}
	}

	if (bReportNoise)
	{
		UAISense_Hearing::ReportNoiseEvent(MeshComp, FootLocation, NoiseLoudness, Owner, NoiseMaxRange, NoiseTag);
	}
}
