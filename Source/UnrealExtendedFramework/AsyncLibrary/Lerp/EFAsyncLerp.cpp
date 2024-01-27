// Fill out your copyright notice in the Description page of Project Settings.


#include "EFAsyncLerp.h"

#include "Kismet/KismetSystemLibrary.h"

UEFAsyncLerp* UEFAsyncLerp::AsyncLerp(const UObject* WorldContextObject, float Duration, bool Reversed,bool EaseInEaseOut)
{
	UEFAsyncLerp* Node = NewObject<UEFAsyncLerp>();
	if (Node)
	{
		Node->WorldContext=WorldContextObject;
		Node->LerpDuration = Duration;
		Node->Reversed = Reversed;
		Node->EaseInEaseOut = EaseInEaseOut;
	}
	return Node;
}

void UEFAsyncLerp::Timer()
{
	const float CurrentGameTime = UKismetSystemLibrary::GetGameTimeInSeconds(WorldContext->GetWorld());

	if (CurrentGameTime > TargetGameTime)
	{
		if (!FinalLoop)
		{
			FinalLoop = true;
			Loop.Broadcast(Reversed ? 0.f : 1.f);
			return;
		}
		
		Finished.Broadcast();
		WorldContext->GetWorld()->GetTimerManager().ClearTimer(LerpHandle);
		return;
	}
	
	if (Reversed)
	{
		if (EaseInEaseOut)
		{
			LerpAlpha = FMath::InterpEaseInOut(1.f, 0.f, (CurrentGameTime - CreatedGameTime) / (TargetGameTime - CreatedGameTime), 2.f);
		}
		else
		{
			LerpAlpha = 1 - (CurrentGameTime - CreatedGameTime) / (TargetGameTime - CreatedGameTime);
		}
	}
	else
	{
		if (EaseInEaseOut)
		{
			LerpAlpha = FMath::InterpEaseInOut(0.f, 1.f, (CurrentGameTime - CreatedGameTime) / (TargetGameTime - CreatedGameTime), 2.f);
		}
		else
		{
			LerpAlpha = (CurrentGameTime - CreatedGameTime) / (TargetGameTime - CreatedGameTime);
		}
	}

	Loop.Broadcast(LerpAlpha);
}

void UEFAsyncLerp::Activate()
{
	Super::Activate();
	
	if (WorldContext)
	{
		CreatedGameTime = UKismetSystemLibrary::GetGameTimeInSeconds(WorldContext->GetWorld());
		TargetGameTime = CreatedGameTime + LerpDuration;
		Timer();
		WorldContext->GetWorld()->GetTimerManager().SetTimer( LerpHandle , this , &UEFAsyncLerp::Timer , 0.01 , true);
	}
}
