// Copyright Moon Punch Games. All Rights Reserved.


#include "EFAsyncVectorTimeline.h"



void UEFAsyncVectorTimeline::InternalTick()
{
	if (IsFinished())
	{
		return;
	}

	// The curve, or whoever started the timeline, is gone: stop without firing anything.
	if (!SelectedFloatCurve || !GetWorldContextObject())
	{
		InternalCompleted();
		return;
	}

	Loop.Broadcast(SelectedFloatCurve->GetVectorValue(TimePassed));
	TimePassed += LoopTime;

	if (TimePassed >= CurveLastTime)
	{
		CancelWork();
		if (FinishAction())
		{
			Completed.Broadcast();
		}
	}
}

void UEFAsyncVectorTimeline::InternalCompleted()
{
	CancelWork();
	FinishAction();
}

UEFAsyncVectorTimeline* UEFAsyncVectorTimeline::ExtendedFloatTimeline(UCurveVector* VectorCurve,const UObject* WorldContextObject, float PassTime)
{
	if (!ensure(WorldContextObject) && !ensure(VectorCurve))	return nullptr;

	if (!VectorCurve)
	{
		UE_LOG(LogBlueprint,Warning,TEXT("Vector Curve Is Not Valid")); return nullptr;
	}
	// Every dimension needs a key: the first and last key times below are read from all three.
	if (VectorCurve->FloatCurves[0].IsEmpty() || VectorCurve->FloatCurves[1].IsEmpty() || VectorCurve->FloatCurves[2].IsEmpty())
	{
		UE_LOG(LogBlueprint,Warning,TEXT("Vector Curve Is Valid But It Has One Or More Empty Curves , Curve Must Have a Key In Every Dimension etc X , Y , Z ")); return nullptr;
	}
	if (PassTime <= 0.f)
	{
		UE_LOG(LogBlueprint,Warning,TEXT("Vector Timeline Pass Time Must Be Greater Than Zero")); return nullptr;
	}

	UEFAsyncVectorTimeline* Node = NewObject<UEFAsyncVectorTimeline>();
	Node->LoopTime = PassTime;
	Node->SelectedFloatCurve = VectorCurve;

	float Smallest = 9999999;
	float Biggest = 0;

	for (const FRichCurve& Curve : VectorCurve->FloatCurves)
	{
		if (Biggest < Curve.GetLastKey().Time)
			Biggest = Curve.GetLastKey().Time;

		if (Smallest > Curve.GetFirstKey().Time)
			Smallest = Curve.GetFirstKey().Time;
	}

	Node->TimePassed = Smallest;
	Node->CurveLastTime = Biggest;
	Node->InitAsyncAction(const_cast<UObject*>(WorldContextObject));
	return Node;
}

void UEFAsyncVectorTimeline::ExecuteAction()
{
	if (!TickTimer.Start(GetWorldContextObject(), FTimerDelegate::CreateUObject(this, &UEFAsyncVectorTimeline::InternalTick), LoopTime, true))
	{
		FailAction(FEFResult::NotAvailable(GetResultSource(), TEXT("a world for the world context")));
	}
}

void UEFAsyncVectorTimeline::CancelWork()
{
	TickTimer.Clear();
}
