// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "UObject/Object.h"
#include "UEExtendedEQSContext_Player.generated.h"

/**
 * 
 */
UCLASS()
class UEEXPANDEDFRAMEWORK_API UUEExtendedEQSContext_Player : public UEnvQueryContext
{
	GENERATED_BODY()

	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};
