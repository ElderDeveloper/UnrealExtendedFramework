// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"

#include "EGSkillCheckTypes.generated.h"

/**
 * How the indicator travels.
 *
 * Two shapes, one model. `Continuous` is the donor's clock dial — the indicator runs round and
 * round and the zone may straddle 0/360. `PingPong` is a gauge needle sweeping back and forth
 * across an arc, which is a different *trajectory* and not a different game: both resolve to
 * "what angle is the indicator at after N seconds", both are evaluated by the same zone test.
 *
 * The alternative — leaving the ping-pong trajectory to the consumer — would mean one consumer
 * computed its own angle and only borrowed §EvaluateAtAngle, which is precisely how a shared
 * model ends up with a single real user of half of it.
 */
UENUM(BlueprintType)
enum class EEGSkillCheckSweepMode : uint8
{
	/** Wraps through 0/360 forever. The zone may straddle the seam. */
	Continuous,

	/** Triangle-wave between SweepMinAngleDegrees and SweepMaxAngleDegrees. A gauge needle. */
	PingPong
};

/**
 * How well one attempt landed.
 *
 * Deliberately three neutral grades and nothing else. This model does not know what a skill
 * check is FOR — it never heals, revives, pays, unlocks or advances a queue, and there is no
 * field here to smuggle such a thing through. Consumers map the tier to their own consequence.
 */
UENUM(BlueprintType)
enum class EEGSkillCheckTier : uint8
{
	Miss,
	Good,
	Perfect
};

/**
 * Everything that decides what one round looks like.
 *
 * Reproducible by construction: the only randomness is drawn from §Seed through an
 * `FRandomStream`, so the same seed produces the same round on the server, on the client, and
 * in a headless test. The donor rolled its zone and start angle from `FMath::FRandRange`, which
 * is global engine state — the same round could not be replayed, described, or verified.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGSkillCheckConfig
{
	GENERATED_BODY()

	/** The one source of randomness. Same seed, same round, on every machine. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillCheck")
	int32 Seed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillCheck")
	EEGSkillCheckSweepMode SweepMode = EEGSkillCheckSweepMode::Continuous;

	/** Magnitude only. §SweepDirection carries the sign, so a negative speed is never meaningful. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillCheck", meta = (ClampMin = "1.0"))
	float SweepDegreesPerSecond = 180.0f;

	/** +1 or -1. Normalized on §Start; anything else resolves to +1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillCheck")
	float SweepDirection = 1.0f;

	/** Roll the direction from the seed instead of using the authored one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillCheck")
	bool bRandomizeDirection = false;

	/** PingPong only. Ignored in Continuous, which always spans the full circle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillCheck")
	float SweepMinAngleDegrees = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillCheck")
	float SweepMaxAngleDegrees = 180.0f;

	/** Where the indicator starts. Used when §bRandomizeStartAngle is false. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillCheck")
	float StartAngleDegrees = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillCheck")
	bool bRandomizeStartAngle = true;

	/** Centre of the scoring band. Used when §bRandomizeZoneCentre is false. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillCheck")
	float ZoneCentreDegrees = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillCheck")
	bool bRandomizeZoneCentre = true;

	/** Full width of the Good band, centred on §ZoneCentreDegrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillCheck", meta = (ClampMin = "0.1"))
	float ZoneWidthDegrees = 60.0f;

	/**
	 * Full width of the inner Perfect band, same centre.
	 *
	 * Zero means the round has no Perfect grade at all — a legitimate configuration for a check
	 * that is pass/fail, and the reason Perfect is a width rather than a bool.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillCheck", meta = (ClampMin = "0.0"))
	float PerfectWidthDegrees = 15.0f;

	/**
	 * Refuses a configuration that cannot produce a playable round.
	 *
	 * Checked rather than clamped: a zero-width sweep or an inverted arc is an authoring mistake,
	 * and silently widening it would hide the mistake behind a round that plays almost right.
	 */
	bool IsValid(FString& OutError) const;
};

/**
 * What one attempt achieved. Neutral by construction — see EEGSkillCheckTier.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGSkillCheckResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SkillCheck")
	EEGSkillCheckTier Tier = EEGSkillCheckTier::Miss;

	/** 1 at the exact centre, falling to 0 at the Good band edge, 0 outside it. Presentation-grade. */
	UPROPERTY(BlueprintReadOnly, Category = "SkillCheck")
	float Accuracy = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "SkillCheck")
	float StopAngleDegrees = 0.0f;

	/** Wraparound-safe offset from the zone centre. Negative is behind it, positive ahead. */
	UPROPERTY(BlueprintReadOnly, Category = "SkillCheck")
	float SignedErrorDegrees = 0.0f;

	/** Seconds of round time at the stop. What a consumer judges solve plausibility against. */
	UPROPERTY(BlueprintReadOnly, Category = "SkillCheck")
	float ElapsedSeconds = 0.0f;
};

/**
 * Wraparound-safe angle arithmetic.
 *
 * §NormalizeAngleDegrees and §IsAngleInRange are the donor's, kept verbatim: they are the two
 * pieces of `DOPSkillcheckWidget` that were already right, including the half-open comparison
 * that makes a zone straddling 0/360 behave. Rewriting them to look tidier is how the 0/360
 * case quietly stops working.
 */
namespace EGSkillCheckAngle
{
	UNREALEXTENDEDGAMEPLAY_API float NormalizeAngleDegrees(float AngleDegrees);

	UNREALEXTENDEDGAMEPLAY_API bool IsAngleInRange(float AngleDegrees, float MinDegrees, float MaxDegrees);

	/** Shortest signed path from Min to Angle, in (-180, 180]. */
	UNREALEXTENDEDGAMEPLAY_API float SignedDeltaDegrees(float FromDegrees, float ToDegrees);
}

/**
 * FEGSkillCheckState
 *
 * The whole skill check as a plain struct: no widget, no world, no tick, no engine globals.
 *
 * ---------------------------------------------------------------------------
 * The angle is a function of elapsed time, not an accumulator
 * ---------------------------------------------------------------------------
 *
 * §AdvanceTo takes an ABSOLUTE elapsed-seconds value and computes the angle in closed form.
 * Two consequences, both of which the donor lacked. It is frame-rate independent — the donor
 * integrated `CurrentAngle += Speed * DeltaTime` inside `NativeTick`, so a hitching frame moved
 * the needle to a different place than a smooth one, and a round could not be described by
 * anything smaller than its whole frame history. And it runs headless — a test names the second
 * it cares about and asks for the angle, with no world, no widget and no ticking at all.
 *
 * ---------------------------------------------------------------------------
 * One attempt, one completion
 * ---------------------------------------------------------------------------
 *
 * §Stop resolves exactly once. A second stop, a stop after §Cancel, or a stop before §Start
 * returns the banked result and changes nothing — because every consumer of this model pays out
 * on completion, and a model that could complete twice would pay twice.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGSkillCheckState
{
	GENERATED_BODY()

	/**
	 * Resolves the config's random choices from its seed and arms the round.
	 *
	 * Returns false and leaves the state inert when the config cannot produce a playable round.
	 */
	bool Start(const FEGSkillCheckConfig& InConfig, FString* OutError = nullptr);

	/** Moves the indicator to where it stands after `ElapsedSeconds` of the round. Idempotent. */
	void AdvanceTo(float ElapsedSeconds);

	/** Advances to `ElapsedSeconds`, grades the stop, and completes the round. Resolves once. */
	FEGSkillCheckResult Stop(float ElapsedSeconds);

	/** Abandons without grading. A subsequent §Stop is refused, as it is after a completion. */
	void Cancel();

	/** Pure grading: where would a stop at this angle land? Never mutates, never completes. */
	FEGSkillCheckResult EvaluateAtAngle(float AngleDegrees) const;

	/** Pure trajectory: where is the indicator at `ElapsedSeconds`? The whole motion model. */
	float AngleAtElapsed(float ElapsedSeconds) const;

	bool IsRunning() const { return bRunning; }
	bool IsComplete() const { return bComplete; }
	bool IsCancelled() const { return bCancelled; }

	float GetCurrentAngleDegrees() const { return CurrentAngleDegrees; }
	float GetElapsedSeconds() const { return ElapsedSeconds; }

	/** The config with its random choices already resolved. What a client needs to draw the round. */
	const FEGSkillCheckConfig& GetResolvedConfig() const { return ResolvedConfig; }

	/** The graded result, or a default Miss while the round is unresolved. */
	const FEGSkillCheckResult& GetLastResult() const { return LastResult; }

private:
	UPROPERTY()
	FEGSkillCheckConfig ResolvedConfig;

	UPROPERTY()
	FEGSkillCheckResult LastResult;

	UPROPERTY()
	float CurrentAngleDegrees = 0.0f;

	UPROPERTY()
	float ElapsedSeconds = 0.0f;

	UPROPERTY()
	bool bRunning = false;

	UPROPERTY()
	bool bComplete = false;

	UPROPERTY()
	bool bCancelled = false;
};
