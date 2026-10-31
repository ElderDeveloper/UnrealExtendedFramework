// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "CoreMinimal.h"

#include "EGReportNoiseEventNotify.generated.h"

/**
 * Reports an AI hearing noise at the owner's location. AI perception only exists on the server, so
 * on clients this does nothing.
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGReportNoiseEventNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	/** Loudness of the noise. If MaxRange is non-zero, modifies MaxRange, otherwise modifies the squared distance of the sensor's range. */
	UPROPERTY(EditAnywhere, Category = "Noise")
	float Loudness = 1;

	/** Max range at which the sound can be heard, multiplied by Loudness. Values <= 0 mean no limit (still limited by listener's range however). */
	UPROPERTY(EditAnywhere, Category = "Noise")
	float MaxRange = 200;

	/** Identifier for the event. */
	UPROPERTY(EditAnywhere, Category = "Noise")
	FName Tag;

	/** Log every report, to track activation. */
	UPROPERTY(EditAnywhere, Category = "Noise")
	bool PrintToLog = false;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};
