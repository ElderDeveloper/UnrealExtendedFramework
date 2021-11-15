// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedFloatTimeline.h"

void UUEExtendedFloatTimeline::InternalTick()
{
	if (SelectedFloatCurve)
	{
		Loop.Broadcast(SelectedFloatCurve->GetFloatValue(TimePassed));
		TimePassed += LoopTime;

		if (TimePassed >= CurveLastTime)
		{
			Completed.Broadcast();
			InternalCompleted();
		}
	}
}

void UUEExtendedFloatTimeline::InternalCompleted()
{
	if (WorldContext)
	{
		WorldContext->GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
		TimerHandle.Invalidate();
		SetReadyToDestroy();
		MarkPendingKill();
	}
}

UUEExtendedFloatTimeline* UUEExtendedFloatTimeline::ExtendedFloatTimeline(UCurveFloat* FloatCurve,const UObject* WorldContextObject, float PassTime)
{
	if (!ensure(WorldContextObject) && !ensure(FloatCurve))	return nullptr;

	if (FloatCurve->FloatCurve.IsEmpty())
	{
		UE_LOG(LogBlueprint,Warning,TEXT("Float Curve Is Valid But Its Empty")); return nullptr;
	}
	UUEExtendedFloatTimeline* Node = NewObject<UUEExtendedFloatTimeline>();
	if (Node)
	{
		Node->WorldContext = WorldContextObject;
		Node->LoopTime = PassTime;
		Node->SelectedFloatCurve = FloatCurve;
		Node->TimePassed = FloatCurve->FloatCurve.GetFirstKey().Time;
		Node->CurveLastTime = FloatCurve->FloatCurve.GetLastKey().Time;
	}
	return Node;
}

void UUEExtendedFloatTimeline::Activate()
{
	Super::Activate();

	if (WorldContext)
	{
		WorldContext->GetWorld()->GetTimerManager().SetTimer(TimerHandle,this,&UUEExtendedFloatTimeline::InternalTick,LoopTime,true);
	}
}
