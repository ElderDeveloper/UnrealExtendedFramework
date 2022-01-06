// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AnimGraphRuntime/Public/BoneControllers/AnimNode_SkeletalControlBase.h"
#include "UEExtendedIKNode.generated.h"



USTRUCT(BlueprintInternalUseOnly)
struct FUEExtendedIKNode : public FAnimNode_SkeletalControlBase
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Links)
	FPoseLink BasePose;

	/*
	// Update incoming component pose.
	virtual void UpdateComponentPose_AnyThread(const FAnimationUpdateContext& Context) override;
	*/
};
