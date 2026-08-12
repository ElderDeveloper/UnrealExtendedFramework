// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGSkillCheckTypes.h"

namespace EGSkillCheckAngle
{
	// Verbatim from DOPSkillcheckWidget.cpp. See the header note.
	float NormalizeAngleDegrees(float AngleDegrees)
	{
		AngleDegrees = FMath::Fmod(AngleDegrees, 360.0f);
		if (AngleDegrees < 0.0f)
		{
			AngleDegrees += 360.0f;
		}
		return AngleDegrees;
	}

	// Verbatim from DOPSkillcheckWidget.cpp. The second branch is the whole point: when the
	// normalized min is greater than the normalized max the zone straddles 0/360, and membership
	// is a union rather than an interval.
	bool IsAngleInRange(float AngleDegrees, float MinDegrees, float MaxDegrees)
	{
		const float Angle = NormalizeAngleDegrees(AngleDegrees);
		const float Min = NormalizeAngleDegrees(MinDegrees);
		const float Max = NormalizeAngleDegrees(MaxDegrees);
		if (Min <= Max)
		{
			return Angle >= Min && Angle <= Max;
		}
		return Angle >= Min || Angle <= Max;
	}

	float SignedDeltaDegrees(float FromDegrees, float ToDegrees)
	{
		return FMath::FindDeltaAngleDegrees(NormalizeAngleDegrees(FromDegrees), NormalizeAngleDegrees(ToDegrees));
	}
}

bool FEGSkillCheckConfig::IsValid(FString& OutError) const
{
	if (SweepDegreesPerSecond <= 0.0f)
	{
		OutError = TEXT("SweepDegreesPerSecond must be positive; SweepDirection carries the sign.");
		return false;
	}

	if (ZoneWidthDegrees <= 0.0f || ZoneWidthDegrees > 360.0f)
	{
		OutError = FString::Printf(TEXT("ZoneWidthDegrees must be in (0, 360]; got %.2f."), ZoneWidthDegrees);
		return false;
	}

	if (PerfectWidthDegrees < 0.0f || PerfectWidthDegrees > ZoneWidthDegrees)
	{
		OutError = FString::Printf(
			TEXT("PerfectWidthDegrees must be in [0, ZoneWidthDegrees]; got %.2f against a zone of %.2f."),
			PerfectWidthDegrees, ZoneWidthDegrees);
		return false;
	}

	if (SweepMode == EEGSkillCheckSweepMode::PingPong)
	{
		const float Span = SweepMaxAngleDegrees - SweepMinAngleDegrees;
		if (Span <= 0.0f)
		{
			OutError = FString::Printf(
				TEXT("PingPong needs SweepMaxAngleDegrees above SweepMinAngleDegrees; got [%.2f, %.2f]."),
				SweepMinAngleDegrees, SweepMaxAngleDegrees);
			return false;
		}
	}

	return true;
}

bool FEGSkillCheckState::Start(const FEGSkillCheckConfig& InConfig, FString* OutError)
{
	FString Error;
	if (!InConfig.IsValid(Error))
	{
		if (OutError)
		{
			*OutError = Error;
		}
		return false;
	}

	ResolvedConfig = InConfig;

	// Every draw happens unconditionally, in a fixed order, and only then is it used or
	// discarded. Otherwise flipping one bRandomize flag would shift the stream underneath the
	// other two, and a seed would no longer describe a round on its own.
	FRandomStream Stream(InConfig.Seed);
	const float RolledDirection = Stream.FRand() < 0.5f ? -1.0f : 1.0f;
	const float RolledStartAngle = Stream.FRandRange(0.0f, 360.0f);
	const float RolledZoneCentre = Stream.FRandRange(0.0f, 360.0f);

	ResolvedConfig.SweepDirection = InConfig.bRandomizeDirection
		? RolledDirection
		: (InConfig.SweepDirection < 0.0f ? -1.0f : 1.0f);
	ResolvedConfig.bRandomizeDirection = false;

	const float StartAngle = InConfig.bRandomizeStartAngle ? RolledStartAngle : InConfig.StartAngleDegrees;
	const float ZoneCentre = InConfig.bRandomizeZoneCentre ? RolledZoneCentre : InConfig.ZoneCentreDegrees;

	if (InConfig.SweepMode == EEGSkillCheckSweepMode::PingPong)
	{
		const float Span = InConfig.SweepMaxAngleDegrees - InConfig.SweepMinAngleDegrees;

		// A rolled angle spans the whole circle and the gauge rarely does, so rolls are mapped
		// ACROSS the arc rather than clamped into it. Clamping would pile every roll past the
		// top of the gauge onto the top of the gauge — a needle that starts at its stop peg two
		// times in three, and a zone the needle can never reach for the rest.
		ResolvedConfig.StartAngleDegrees = InConfig.bRandomizeStartAngle
			? InConfig.SweepMinAngleDegrees + (EGSkillCheckAngle::NormalizeAngleDegrees(StartAngle) / 360.0f) * Span
			: FMath::Clamp(StartAngle, InConfig.SweepMinAngleDegrees, InConfig.SweepMaxAngleDegrees);

		ResolvedConfig.ZoneCentreDegrees = InConfig.bRandomizeZoneCentre
			? InConfig.SweepMinAngleDegrees + (EGSkillCheckAngle::NormalizeAngleDegrees(ZoneCentre) / 360.0f) * Span
			: ZoneCentre;
	}
	else
	{
		ResolvedConfig.StartAngleDegrees = EGSkillCheckAngle::NormalizeAngleDegrees(StartAngle);
		ResolvedConfig.ZoneCentreDegrees = EGSkillCheckAngle::NormalizeAngleDegrees(ZoneCentre);
	}

	ResolvedConfig.bRandomizeStartAngle = false;
	ResolvedConfig.bRandomizeZoneCentre = false;

	LastResult = FEGSkillCheckResult();
	ElapsedSeconds = 0.0f;
	CurrentAngleDegrees = ResolvedConfig.StartAngleDegrees;
	bRunning = true;
	bComplete = false;
	bCancelled = false;

	return true;
}

float FEGSkillCheckState::AngleAtElapsed(float InElapsedSeconds) const
{
	const float Travel = ResolvedConfig.SweepDirection * ResolvedConfig.SweepDegreesPerSecond * InElapsedSeconds;

	if (ResolvedConfig.SweepMode == EEGSkillCheckSweepMode::Continuous)
	{
		return EGSkillCheckAngle::NormalizeAngleDegrees(ResolvedConfig.StartAngleDegrees + Travel);
	}

	const float Span = ResolvedConfig.SweepMaxAngleDegrees - ResolvedConfig.SweepMinAngleDegrees;
	if (Span <= 0.0f)
	{
		return ResolvedConfig.SweepMinAngleDegrees;
	}

	// Triangle wave: unfold the needle's path onto a line of length 2*Span, then fold it back.
	// A needle that reaches an end of its arc reverses, which is what makes the fold the model
	// rather than a special case bolted onto a wrap.
	const float Period = 2.0f * Span;
	float Phase = FMath::Fmod((ResolvedConfig.StartAngleDegrees - ResolvedConfig.SweepMinAngleDegrees) + Travel, Period);
	if (Phase < 0.0f)
	{
		Phase += Period;
	}

	const float Offset = Phase <= Span ? Phase : Period - Phase;
	return ResolvedConfig.SweepMinAngleDegrees + Offset;
}

void FEGSkillCheckState::AdvanceTo(float InElapsedSeconds)
{
	if (!bRunning)
	{
		return;
	}

	ElapsedSeconds = FMath::Max(0.0f, InElapsedSeconds);
	CurrentAngleDegrees = AngleAtElapsed(ElapsedSeconds);
}

FEGSkillCheckResult FEGSkillCheckState::EvaluateAtAngle(float AngleDegrees) const
{
	FEGSkillCheckResult Result;
	Result.StopAngleDegrees = ResolvedConfig.SweepMode == EEGSkillCheckSweepMode::PingPong
		? AngleDegrees
		: EGSkillCheckAngle::NormalizeAngleDegrees(AngleDegrees);
	Result.SignedErrorDegrees = EGSkillCheckAngle::SignedDeltaDegrees(ResolvedConfig.ZoneCentreDegrees, AngleDegrees);
	Result.ElapsedSeconds = ElapsedSeconds;

	const float GoodHalf = ResolvedConfig.ZoneWidthDegrees * 0.5f;
	const float PerfectHalf = ResolvedConfig.PerfectWidthDegrees * 0.5f;

	// A full-circle zone contains everything. Left to IsAngleInRange it would not: the
	// normalized min and max coincide, and the interval branch would admit exactly one angle.
	const bool bInGood = ResolvedConfig.ZoneWidthDegrees >= 360.0f
		|| EGSkillCheckAngle::IsAngleInRange(
			AngleDegrees,
			ResolvedConfig.ZoneCentreDegrees - GoodHalf,
			ResolvedConfig.ZoneCentreDegrees + GoodHalf);

	if (!bInGood)
	{
		Result.Tier = EEGSkillCheckTier::Miss;
		Result.Accuracy = 0.0f;
		return Result;
	}

	const bool bInPerfect = ResolvedConfig.PerfectWidthDegrees > 0.0f
		&& FMath::Abs(Result.SignedErrorDegrees) <= PerfectHalf;

	Result.Tier = bInPerfect ? EEGSkillCheckTier::Perfect : EEGSkillCheckTier::Good;
	Result.Accuracy = GoodHalf > 0.0f
		? FMath::Clamp(1.0f - (FMath::Abs(Result.SignedErrorDegrees) / GoodHalf), 0.0f, 1.0f)
		: 1.0f;

	return Result;
}

FEGSkillCheckResult FEGSkillCheckState::Stop(float InElapsedSeconds)
{
	// One attempt resolves once. Not a guard against misuse — a guard against paying twice for
	// one round, which is what every consumer of this model does on completion.
	if (!bRunning)
	{
		return LastResult;
	}

	AdvanceTo(InElapsedSeconds);

	bRunning = false;
	bComplete = true;
	LastResult = EvaluateAtAngle(CurrentAngleDegrees);

	return LastResult;
}

void FEGSkillCheckState::Cancel()
{
	if (!bRunning)
	{
		return;
	}

	bRunning = false;
	bCancelled = true;
}
