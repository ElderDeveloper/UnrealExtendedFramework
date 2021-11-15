﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "UEExtendedVectorTimeline.h"


void UUEExtendedVectorTimeline::InternalTick()
{
	if (SelectedFloatCurve)
	{
		Loop.Broadcast(SelectedFloatCurve->GetVectorValue(TimePassed));
		TimePassed += LoopTime;

		if (TimePassed >= CurveLastTime)
		{
			Completed.Broadcast();
			InternalCompleted();
		}
	}
}

void UUEExtendedVectorTimeline::InternalCompleted()
{
	if (WorldContext)
	{
		WorldContext->GetWorld()->GetTimerManager().ClearTimer(TimerHandle);
		TimerHandle.Invalidate();
		SetReadyToDestroy();
		MarkPendingKill();
	}
}

UUEExtendedVectorTimeline* UUEExtendedVectorTimeline::ExtendedFloatTimeline(UCurveVector* VectorCurve,const UObject* WorldContextObject, float PassTime)
{
	if (!ensure(WorldContextObject) && !ensure(VectorCurve))	return nullptr;

	if (VectorCurve->FloatCurves->IsEmpty())
	{
		UE_LOG(LogBlueprint,Warning,TEXT("Vector Curve Is Valid But It Has One Or More Empty Curves , Curve Must Have a Key In Every Dimension etc X , Y , Z ")); return nullptr;
	}
	
	UUEExtendedVectorTimeline* Node = NewObject<UUEExtendedVectorTimeline>();
	if (Node)
	{
		Node->WorldContext = WorldContextObject;
		Node->LoopTime = PassTime;
		Node->SelectedFloatCurve = VectorCurve;

		float Smallest = 9999999;
		float Biggest = 0;

	
		for (const auto i : VectorCurve->FloatCurves)
		{
			if (Biggest < i.GetLastKey().Time)
				Biggest = i.GetLastKey().Time;
			
			if (Smallest > i.GetFirstKey().Time)
				Smallest = i.GetFirstKey().Time;
		}
		
		Node->TimePassed = Smallest;
		Node->CurveLastTime = Biggest;
	}
	return Node;
}

void UUEExtendedVectorTimeline::Activate()
{
	Super::Activate();

	if (WorldContext)
	{
		WorldContext->GetWorld()->GetTimerManager().SetTimer(TimerHandle,this,&UUEExtendedVectorTimeline::InternalTick,LoopTime,true);
	}
}