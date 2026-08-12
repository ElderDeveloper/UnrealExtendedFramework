// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGSkillCheckWidgetBase.h"

#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogEGSkillCheck, Log, All);

bool UEGSkillCheckWidgetBase::StartSkillCheck(const FEGSkillCheckConfig& Config)
{
	// A round already under way is abandoned rather than restarted on top of itself: the
	// alternative leaves one started round with two possible completions.
	if (State.IsRunning())
	{
		CancelSkillCheck();
	}

	FString Error;
	if (!State.Start(Config, &Error))
	{
		UE_LOG(LogEGSkillCheck, Warning, TEXT("StartSkillCheck refused: %s"), *Error);
		return false;
	}

	AccumulatedSeconds = 0.0f;
	bHasWorldClock = false;
	if (const UWorld* World = GetWorld())
	{
		StartWorldTime = World->GetTimeSeconds();
		bHasWorldClock = true;
	}

	OnSkillCheckStarted(State.GetResolvedConfig());
	OnSkillCheckAngleUpdated(State.GetCurrentAngleDegrees());

	return true;
}

void UEGSkillCheckWidgetBase::NativeTick(const FGeometry& MyGeometry, float DeltaTime)
{
	Super::NativeTick(MyGeometry, DeltaTime);

	if (!State.IsRunning())
	{
		return;
	}

	float Elapsed = 0.0f;
	if (bHasWorldClock)
	{
		if (const UWorld* World = GetWorld())
		{
			Elapsed = static_cast<float>(World->GetTimeSeconds() - StartWorldTime);
		}
	}
	else
	{
		AccumulatedSeconds += DeltaTime;
		Elapsed = AccumulatedSeconds;
	}

	State.AdvanceTo(Elapsed);
	OnSkillCheckAngleUpdated(State.GetCurrentAngleDegrees());
}

FEGSkillCheckResult UEGSkillCheckWidgetBase::StopSkillCheck()
{
	if (!State.IsRunning())
	{
		return State.GetLastResult();
	}

	const FEGSkillCheckResult Result = State.Stop(State.GetElapsedSeconds());

	OnSkillCheckEnded(Result, /*bGraded*/ true);
	OnSkillCheckFinished.Broadcast(Result);

	return Result;
}

void UEGSkillCheckWidgetBase::CancelSkillCheck()
{
	if (!State.IsRunning())
	{
		return;
	}

	State.Cancel();

	// No broadcast. OnSkillCheckFinished is what a consumer pays out on, and an abandoned round
	// earned nothing — a "cancelled" result travelling the same channel as a graded one is how a
	// Miss gets awarded to somebody who closed the screen.
	OnSkillCheckEnded(State.GetLastResult(), /*bGraded*/ false);
}

void UEGSkillCheckWidgetBase::NativeDestruct()
{
	// Every teardown path lands here — closed, level torn down, pawn destroyed. A round still
	// running when its widget dies must resolve to nothing, not sit armed on a dead object.
	CancelSkillCheck();

	Super::NativeDestruct();
}
