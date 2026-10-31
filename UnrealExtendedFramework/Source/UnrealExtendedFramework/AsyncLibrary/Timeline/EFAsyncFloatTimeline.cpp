// Copyright Moon Punch Games. All Rights Reserved.


#include "EFAsyncFloatTimeline.h"


namespace EFAsyncFloatTimelinePrivate
{
	/** Playing timelines by name, so that starting one with a name in use replaces the old one. */
	TMap<FName, TWeakObjectPtr<UEFAsyncFloatTimeline>> PlayingTimelines;

	UEFAsyncFloatTimeline* FindPlaying(const FName TimelineName)
	{
		const TWeakObjectPtr<UEFAsyncFloatTimeline>* Found = PlayingTimelines.Find(TimelineName);
		UEFAsyncFloatTimeline* Timeline = Found ? Found->Get() : nullptr;
		return Timeline && !Timeline->IsFinished() ? Timeline : nullptr;
	}
}


UEFAsyncFloatTimeline* UEFAsyncFloatTimeline::ExtendedFloatTimeline(const FName TimelineName , UCurveFloat* FloatCurve ,const UObject* WorldContextObject , float PassTime , TEnumAsByte<EFTimelinePlayType> TimelinePlayType , bool bTickWhenPaused)
{
	if (!ensure(WorldContextObject) && !ensure(FloatCurve))	return nullptr;

	if (!FloatCurve)
	{
		UE_LOG(LogBlueprint,Warning,TEXT("Float Curve Is Not Valid")); return nullptr;
	}
	if (FloatCurve->FloatCurve.IsEmpty())
	{
		UE_LOG(LogBlueprint,Warning,TEXT("Float Curve Is Valid But Its Empty")); return nullptr;
	}
	if (PassTime <= 0.f && TimelinePlayType != Ext_StopTimeline)
	{
		UE_LOG(LogBlueprint,Warning,TEXT("Float Timeline Pass Time Must Be Greater Than Zero")); return nullptr;
	}


	// Store The Current Timeline Time If We Use Play Or Reverse
	float TimelineCurrentTime = 0.f;
	bool Exist = false;

	// If We Have A Timeline With The Same Name Get Its Current Time And Stop It So That Its Loops End
	if (UEFAsyncFloatTimeline* Playing = EFAsyncFloatTimelinePrivate::FindPlaying(TimelineName))
	{
		TimelineCurrentTime = Playing->TimePassed;
		Exist = true;
		Playing->InternalCompleted();
	}

	if (TimelinePlayType == Ext_StopTimeline)
	{
		return nullptr;
	}


	// Fill Simple Variables To Newly Created Timeline
	UEFAsyncFloatTimeline* Timeline = NewObject<UEFAsyncFloatTimeline>();
	Timeline->ExtendedTimelineName = TimelineName;
	Timeline->LoopTime = PassTime;
	Timeline->SelectedFloatCurve = FloatCurve;
	Timeline->CurveLastTime = FloatCurve->FloatCurve.GetLastKey().Time;
	Timeline->CurveFirstTime = FloatCurve->FloatCurve.GetFirstKey().Time;
	Timeline->bTickWhenPaused = bTickWhenPaused;
	Timeline->InitAsyncAction(const_cast<UObject*>(WorldContextObject));

	// Remember It By Name So That We Can Find It Afterwards
	EFAsyncFloatTimelinePrivate::PlayingTimelines.Emplace(TimelineName, Timeline);


	// Select What We Will Do For Newly Created Timeline
	switch (TimelinePlayType)
	{
		case Ext_Play:
			Timeline->TimelinePlaySide = Ext_Forward;
			Timeline->TimePassed = Exist ? TimelineCurrentTime : Timeline->CurveFirstTime;
			break;

		case Ext_PlayFromStart:
			Timeline->TimelinePlaySide = Ext_Forward;
			Timeline->TimePassed = Timeline->CurveFirstTime;
			break;

		case Ext_Reverse:
			Timeline->TimelinePlaySide = Ext_Backward;
			Timeline->TimePassed = Exist ? TimelineCurrentTime : Timeline->CurveLastTime;
			break;

		case Ext_ReverseFromEnd:
			Timeline->TimelinePlaySide = Ext_Backward;
			Timeline->TimePassed = Timeline->CurveLastTime;
			break;

		default:
			break;
	}

	return Timeline;
}



void UEFAsyncFloatTimeline::ExtendedFloatTimelineManipulate(const FName TimelineName,const UObject* WorldContextObject, TEnumAsByte<EFTimelinePlayType> TimelinePlayType)
{
	if (!ensure(WorldContextObject))	return;

	UEFAsyncFloatTimeline* Timeline = EFAsyncFloatTimelinePrivate::FindPlaying(TimelineName);
	if (!Timeline) return;

	// Select What We Will Do For Timeline
	switch (TimelinePlayType)
	{

		case Ext_Play:
			Timeline->TimelinePlaySide = Ext_Forward;
			return ;

		case Ext_PlayFromStart:
			Timeline->TimelinePlaySide = Ext_Forward;
			Timeline->TimePassed = Timeline->CurveFirstTime;
			return ;

		case Ext_Reverse:
			Timeline->TimelinePlaySide = Ext_Backward;
			return ;

		case Ext_ReverseFromEnd:
			Timeline->TimelinePlaySide = Ext_Backward;
			Timeline->TimePassed = Timeline->CurveLastTime;
			return ;

		case Ext_StopTimeline:
			Timeline->InternalCompleted();
			return;

		default: return;
	}
}


void UEFAsyncFloatTimeline::InternalTick()
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

	Loop.Broadcast(SelectedFloatCurve->GetFloatValue(TimePassed));

	// Loop's handler may have stopped or replaced this timeline.
	if (IsFinished())
	{
		return;
	}

	switch (TimelinePlaySide)
	{
	case Ext_Forward:

		TimePassed += LoopTime;
		if (TimePassed >= CurveLastTime)
		{
			FinishTimeline();
		}
		break;

	case Ext_Backward:

		TimePassed -= LoopTime;
		if (TimePassed <= CurveFirstTime)
		{
			FinishTimeline();
		}
		break;
	}
}

void UEFAsyncFloatTimeline::InternalCompleted()
{
	StopTimeline();
	FinishAction();
}

void UEFAsyncFloatTimeline::FinishTimeline()
{
	StopTimeline();
	if (FinishAction())
	{
		Completed.Broadcast();
	}
}

void UEFAsyncFloatTimeline::StopTimeline()
{
	TickTimer.Clear();

	// Forget the name only while it still means this timeline, not one that replaced it.
	const TWeakObjectPtr<UEFAsyncFloatTimeline>* Registered = EFAsyncFloatTimelinePrivate::PlayingTimelines.Find(ExtendedTimelineName);
	if (Registered && (!Registered->IsValid() || Registered->Get() == this))
	{
		EFAsyncFloatTimelinePrivate::PlayingTimelines.Remove(ExtendedTimelineName);
	}
}






void UEFAsyncFloatTimeline::ExecuteAction()
{
	Reset();
}

void UEFAsyncFloatTimeline::CancelWork()
{
	StopTimeline();
}

void UEFAsyncFloatTimeline::Reset()
{
	if (IsFinished())
	{
		return;
	}

	if (!TickTimer.Start(GetWorldContextObject(), FTimerDelegate::CreateUObject(this, &UEFAsyncFloatTimeline::InternalTick), LoopTime, true, bTickWhenPaused))
	{
		StopTimeline();
		FailAction(FEFResult::NotAvailable(GetResultSource(), TEXT("a world for the world context")));
	}
}
