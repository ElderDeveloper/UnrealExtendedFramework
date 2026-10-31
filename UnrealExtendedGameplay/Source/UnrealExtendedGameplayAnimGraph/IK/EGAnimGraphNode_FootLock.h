// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "AnimGraphNode_SkeletalControlBase.h"
#include "CoreMinimal.h"
#include "UnrealExtendedGameplay/Animation/IK/EGAnimNode_FootLock.h"

#include "EGAnimGraphNode_FootLock.generated.h"

/** The editor side of FAnimNode_EGFootLock. */
UCLASS(MinimalAPI)
class UAnimGraphNode_EGFootLock : public UAnimGraphNode_SkeletalControlBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	FAnimNode_EGFootLock Node;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;

protected:
	virtual FText GetControllerDescription() const override;
	virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
};
