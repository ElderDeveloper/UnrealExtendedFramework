// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "EGSkillCheckTypes.h"

#include "EGSkillCheckWidgetBase.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGOnSkillCheckFinished, const FEGSkillCheckResult&, Result);

/**
 * UEGSkillCheckWidgetBase
 *
 * The rendering half of a skill check, and only the rendering half.
 *
 * ---------------------------------------------------------------------------
 * The widget owns no rules
 * ---------------------------------------------------------------------------
 *
 * Every decision — where the zone is, where the indicator is at second N, what grade a stop
 * earns — belongs to §FEGSkillCheckState, which has no world and no widget and can be run in a
 * test. This class turns a tick into an elapsed-seconds value, asks the model, and hands the
 * answer to three presentation hooks. A subclass binds its own images and implements the hooks;
 * it does not compute an angle, and it cannot grade an attempt.
 *
 * That split is why the donor is a *subclass* after the extraction rather than a caller: the
 * only thing `DOPSkillcheckWidget` ever had that was DOP's was two `UImage`s.
 *
 * ---------------------------------------------------------------------------
 * Elapsed time comes from the clock, not from a running total of frames
 * ---------------------------------------------------------------------------
 *
 * §StartSkillCheck stamps the world time and every tick asks the model for the angle at
 * `Now - StartTime`. A hitching frame therefore moves the indicator to exactly where a smooth
 * one would have. Consumers that need to drive the model on their own clock — a server
 * replaying a round, a test naming its seconds — talk to the state directly.
 *
 * The check never pauses the game and never touches audio. If a consumer needs the world to
 * carry on being heard while a player is looking at this, that is already true here; it is not
 * a property this class has to be asked for.
 */
UCLASS(Abstract)
class UNREALEXTENDEDGAMEPLAY_API UEGSkillCheckWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;
	virtual void NativeDestruct() override;

	/** Fires exactly once per started round, on the frame the stop is graded. */
	UPROPERTY(BlueprintAssignable, Category = "EG|SkillCheck")
	FEGOnSkillCheckFinished OnSkillCheckFinished;

	/**
	 * Arms a round from a fully described config. Returns false on a config that cannot play,
	 * with the reason logged — a refused start leaves the widget inert rather than half-armed.
	 */
	UFUNCTION(BlueprintCallable, Category = "EG|SkillCheck")
	bool StartSkillCheck(const FEGSkillCheckConfig& Config);

	/** Grades the stop where the indicator now stands, broadcasts, and completes the round. */
	UFUNCTION(BlueprintCallable, Category = "EG|SkillCheck")
	FEGSkillCheckResult StopSkillCheck();

	/** Abandons without grading and without broadcasting. Nothing is owed for a round not finished. */
	UFUNCTION(BlueprintCallable, Category = "EG|SkillCheck")
	void CancelSkillCheck();

	UFUNCTION(BlueprintPure, Category = "EG|SkillCheck")
	bool IsSkillCheckActive() const { return State.IsRunning(); }

	UFUNCTION(BlueprintPure, Category = "EG|SkillCheck")
	float GetCurrentAngleDegrees() const { return State.GetCurrentAngleDegrees(); }

	UFUNCTION(BlueprintPure, Category = "EG|SkillCheck")
	FEGSkillCheckConfig GetResolvedConfig() const { return State.GetResolvedConfig(); }

	UFUNCTION(BlueprintPure, Category = "EG|SkillCheck")
	FEGSkillCheckResult GetLastResult() const { return State.GetLastResult(); }

	/** Read-only view of the model, for a consumer that wants to grade a hypothetical stop. */
	const FEGSkillCheckState& GetState() const { return State; }

protected:
	/**
	 * The round is armed and its random choices are resolved. Draw the zone.
	 *
	 * The config handed here has no `bRandomize*` left set — every value in it is the value the
	 * round will actually use, on every machine that seeded it the same way.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "EG|SkillCheck")
	void OnSkillCheckStarted(const FEGSkillCheckConfig& ResolvedConfig);
	virtual void OnSkillCheckStarted_Implementation(const FEGSkillCheckConfig& ResolvedConfig) {}

	/** The indicator is here now. Called every tick of a live round, and once on start. */
	UFUNCTION(BlueprintNativeEvent, Category = "EG|SkillCheck")
	void OnSkillCheckAngleUpdated(float AngleDegrees);
	virtual void OnSkillCheckAngleUpdated_Implementation(float AngleDegrees) {}

	/** The round is over, one way or the other. `bGraded` is false for an abandonment. */
	UFUNCTION(BlueprintNativeEvent, Category = "EG|SkillCheck")
	void OnSkillCheckEnded(const FEGSkillCheckResult& Result, bool bGraded);
	virtual void OnSkillCheckEnded_Implementation(const FEGSkillCheckResult& Result, bool bGraded) {}

	/** Seconds of round time. Public to subclasses so a readout can show a countdown. */
	UFUNCTION(BlueprintPure, Category = "EG|SkillCheck")
	float GetElapsedSeconds() const { return State.GetElapsedSeconds(); }

	FEGSkillCheckState State;

private:
	/** World time at §StartSkillCheck. The tick derives elapsed from it rather than summing deltas. */
	double StartWorldTime = 0.0;

	/** Fallback total when there is no world to read a clock from. */
	float AccumulatedSeconds = 0.0f;

	bool bHasWorldClock = false;
};
