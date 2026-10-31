// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UnrealExtendedGameplay/Animation/Events/EGAnimEventNotify.h"

#include "EGFootstepNotify.generated.h"

/**
 * A foot touched the ground. Calls the owner's UEGFootstepComponent at the foot socket (surface
 * sound, effects, decal), optionally reports an AI hearing noise, and raises the
 * ExtendedGameplay.Anim.Footstep anim event for anything else that listens.
 *
 * Effects are skipped on dedicated servers; the noise is only heard on the server, where AI
 * perception runs.
 */
UCLASS(meta = (DisplayName = "Footstep"))
class UNREALEXTENDEDGAMEPLAY_API UEGFootstepNotify : public UEGAnimEventNotifyBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep")
	FName FootSocket = TEXT("foot_l");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Noise")
	bool bReportNoise = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Noise", meta = (EditCondition = "bReportNoise", ClampMin = "0"))
	float NoiseLoudness = 1.0f;

	/** Max range at which the step can be heard, multiplied by loudness. 0 or less: the listener's range. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Noise", meta = (EditCondition = "bReportNoise", Units = "cm"))
	float NoiseMaxRange = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Noise", meta = (EditCondition = "bReportNoise"))
	FName NoiseTag = TEXT("Footstep");

	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	virtual FGameplayTag GetEventTag() const override;
	virtual FEGAnimEventPayload GetEventPayload() const override;
};
