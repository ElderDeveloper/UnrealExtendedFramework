// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EFEnums.generated.h"



UENUM(BlueprintType,Blueprintable)
enum EFHitDirection
{
	No , Front , Back , Left , Right
};



UENUM(BlueprintType , Blueprintable)
enum EFButtonAction
{
	Press,
	Release
};

UENUM()
enum EFExtendedLoopOutput
{
	ExtendedLoop,
	ExtendedComplete
};

UENUM(BlueprintType)
enum EFConditionOutput
{
	UEF_True,
	UEF_False
};


UENUM(BlueprintType , Blueprintable)
enum class EFProjectDirectory : uint8
{
	ProjectDir,
	ProjectConfigDir,
	ProjectContentDir,
	ProjectIntermediateDir,
	ProjectSavedDir,
	ProjectPluginsDir,
	ProjectLogDir,
	ProjectModsDir
};


UENUM(BlueprintType , Blueprintable)
enum EFApplyDamageType
{
	UEF_ApplyDamage,
	UEF_ApplyPointDamage,
	UEF_ApplyRadialDamage,
	UEF_ApplyRadialDamageFalloff,
};

UENUM(BlueprintType , Blueprintable)
enum EFDamageDirection
{
	UEF_DamageForward		UMETA(DisplayName = "UEF_DamageForward"),
	UEF_DamageBackward		UMETA(DisplayName = "UEF_DamageBackward"),
	UEF_DamageLeft			UMETA(DisplayName = "UEF_DamageLeft"),
	UEF_DamageRight			UMETA(DisplayName = "UEF_DamageRight")
};


UENUM(BlueprintType , Blueprintable)
enum EFPerceptionFaction
{
	Hostile,
	Ally,
	Neutral
};

UENUM(BlueprintType , Blueprintable)
enum EFPerceptionState
{
	EFP_Idle,
	EFP_VaguelySensed,
	EFP_Warned,
	EFP_Alerted
};

UCLASS()
class UEFEnums : public UObject
{
	GENERATED_BODY()
};