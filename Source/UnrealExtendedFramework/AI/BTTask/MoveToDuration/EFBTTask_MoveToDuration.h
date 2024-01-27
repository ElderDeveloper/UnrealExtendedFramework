// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tasks/AITask_MoveTo.h"
#include "EFBTTask_MoveToDuration.generated.h"

/**
 * 
 */
UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFBTTask_MoveToDuration : public UAITask_MoveTo
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere)
	float Duration = 1.f;

protected:

	FTimerHandle Handle;

	UFUNCTION()
	void FinishDelay();

	virtual void Activate() override;
	
};
