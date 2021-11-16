﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UEExtendedTimelineData.generated.h"


UENUM()
enum  EExtendedTimelinePlayType
{
	Ext_Play			UMETA(DisplayName = "Ext_Play"),
	Ext_PlayFromStart	UMETA(DisplayName = "Ext_PlayFromStart"),
	Ext_Reverse			UMETA(DisplayName = "Ext_Reverse"),
	Ext_ReverseFromEnd  UMETA(DisplayName = "Ext_ReverseFromEnd"),
	Ext_StopTimeline	UMETA(DisplayName = "Ext_StopTimeline"),
	Ext_Empty			UMETA(DisplayName = "Ext_Empty"),
};

UENUM()
enum EExtendedTimelinePlaySide
{
	Ext_Forward,
	Ext_Backward
};


UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedTimelineData : public UObject
{
	GENERATED_BODY()
};
