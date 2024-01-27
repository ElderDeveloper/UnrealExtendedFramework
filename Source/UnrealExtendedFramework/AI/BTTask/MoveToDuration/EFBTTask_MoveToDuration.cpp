// Fill out your copyright notice in the Description page of Project Settings.


#include "EFBTTask_MoveToDuration.h"

void UEFBTTask_MoveToDuration::FinishDelay()
{
	FinishMoveTask(EPathFollowingResult::Success);
}

void UEFBTTask_MoveToDuration::Activate()
{
	Super::Activate();

	 GetWorld()->GetTimerManager().SetTimer(Handle,this,&UEFBTTask_MoveToDuration::FinishDelay,Duration,false);
}
