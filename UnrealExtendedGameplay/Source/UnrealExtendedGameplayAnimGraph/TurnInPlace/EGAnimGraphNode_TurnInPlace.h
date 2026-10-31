// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "AnimGraphNode_Base.h"
#include "CoreMinimal.h"
#include "UnrealExtendedGameplay/Animation/TurnInPlace/EGAnimNode_TurnInPlace.h"

#include "EGAnimGraphNode_TurnInPlace.generated.h"

/** The editor side of FAnimNode_EGTurnInPlace. */
UCLASS(MinimalAPI)
class UAnimGraphNode_EGTurnInPlace : public UAnimGraphNode_Base
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Settings")
	FAnimNode_EGTurnInPlace Node;

	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual FString GetNodeCategory() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
};
