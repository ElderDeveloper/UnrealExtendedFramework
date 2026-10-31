// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGTurnInPlaceTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "EGTurnInPlaceAnimLibrary.generated.h"

class UAnimInstance;
class UAnimSequence;
class UEGTurnInPlaceComponent;

/**
 * The anim instance's half of turn in place. UEGAnimInstance calls the first two itself; a custom
 * anim instance calls GatherTurnInPlace on the game thread and ThreadSafeUpdateTurnInPlace on the
 * worker thread, keeps the FEGTurnInPlaceAnimData, and returns its Curves from
 * IEGTurnInPlaceAnimSource. The rest are the anim graph's node functions (Docs/TurnInPlace.md).
 *
 * The Turn In Place anim node (FAnimNode_EGTurnInPlace) does all of this by itself; these are for
 * a project that builds its own state machine.
 */
UCLASS()
class UNREALEXTENDEDGAMEPLAY_API UEGTurnInPlaceAnimLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Game thread (NativeUpdateAnimation / Event Blueprint Update Animation). */
	UFUNCTION(BlueprintCallable, Category = "Extended|Turn In Place", meta = (NotBlueprintThreadSafe))
	static void GatherTurnInPlace(const UEGTurnInPlaceComponent* Component, UPARAM(ref) FEGTurnInPlaceAnimData& Data);

	/**
	 * Worker thread (NativeThreadSafeUpdateAnimation / Blueprint Thread Safe Update Animation), after
	 * GatherTurnInPlace. Reads the curves now, refreshes "is turning" from them, latches "turned
	 * this entry", and computes the transitions, all before the state machine reads them.
	 */
	UFUNCTION(BlueprintCallable, Category = "Extended|Turn In Place", meta = (BlueprintThreadSafe, DefaultToSelf = "AnimInstance"))
	static void ThreadSafeUpdateTurnInPlace(const UAnimInstance* AnimInstance, UPARAM(ref) FEGTurnInPlaceAnimData& Data);

	// ----- Node functions ---------------------------------------------------------------------------

	/** Idle state: On Become Relevant. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Turn In Place|Graph", meta = (BlueprintThreadSafe))
	static void SetupIdle(UPARAM(ref) FEGTurnInPlaceAnimData& Data);

	/**
	 * Turn state's output pose: On Become Relevant. Runs before the evaluator's own setup. Picks the
	 * step and whether it is procedural (TurnInPlace.Node.bProcedural: no clip, blend in the idle pose).
	 */
	UFUNCTION(BlueprintCallable, Category = "Extended|Turn In Place|Graph", meta = (BlueprintThreadSafe))
	static void SetupTurnPose(UPARAM(ref) FEGTurnInPlaceAnimData& Data);

	/** Turn Sequence Evaluator: On Become Relevant. Set Sequence to the result and Explicit Time to 0. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Turn In Place|Graph", meta = (BlueprintThreadSafe))
	static UAnimSequence* SetupTurnAnim(UPARAM(ref) FEGTurnInPlaceAnimData& Data);

	/**
	 * Turn Sequence Evaluator: On Update. Set Sequence With Inertial Blending to OutSequence (the
	 * stance can change mid-turn) and Explicit Time to the returned time; without that the turn
	 * never advances.
	 */
	UFUNCTION(BlueprintCallable, Category = "Extended|Turn In Place|Graph", meta = (BlueprintThreadSafe))
	static float UpdateTurnAnim(UPARAM(ref) FEGTurnInPlaceAnimData& Data, float DeltaTime, UAnimSequence*& OutSequence);

	/** Recovery state's output pose: On Become Relevant. Recovery continues the same clip from where the turn is. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Turn In Place|Graph", meta = (BlueprintThreadSafe))
	static void SetupRecoveryPose(UPARAM(ref) FEGTurnInPlaceAnimData& Data);

	/**
	 * Recovery Sequence Evaluator: On Update. Set Sequence With Inertial Blending to OutSequence and
	 * Explicit Time to the returned time. It continues from the turn's time at 1x; restarting the
	 * clip would replay its turning frames and turn the character twice. Recovery -> Idle on
	 * TurnInPlace.Node.bRecoveryFinished.
	 */
	UFUNCTION(BlueprintCallable, Category = "Extended|Turn In Place|Graph", meta = (BlueprintThreadSafe))
	static float UpdateRecoveryAnim(UPARAM(ref) FEGTurnInPlaceAnimData& Data, float DeltaTime, UAnimSequence*& OutSequence);

	/** The turn sequence (or the recovery one, which continues the same clip). */
	UFUNCTION(BlueprintPure, Category = "Extended|Turn In Place|Graph", meta = (BlueprintThreadSafe))
	static UAnimSequence* GetTurnAnimation(const FEGTurnInPlaceAnimData& Data, bool bRecovery);
};
