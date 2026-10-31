// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotify.h"
#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

#include "EGCameraShakeNotify.generated.h"

class UCameraShakeBase;

/** Plays a world camera shake centred on the mesh, for every local camera within range. */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGCameraShakeNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify")
	TSubclassOf<UCameraShakeBase> CameraShake;

	/** Cameras within this distance of the mesh get the full shake. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify", meta = (ClampMin = "0", Units = "cm"))
	float InnerRadius = 300.0f;

	/** Cameras between InnerRadius and this get a weaker shake; beyond it, none. At or below InnerRadius there is no falloff band. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify", meta = (ClampMin = "0", Units = "cm"))
	float OuterRadius = 1500.0f;

	/** Exponent of the falloff between the two radii; 1 is linear. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AnimNotify", meta = (ClampMin = "0"))
	float Falloff = 1.0f;
};
