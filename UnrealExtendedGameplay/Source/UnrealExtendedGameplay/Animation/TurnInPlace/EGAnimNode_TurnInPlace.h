// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "Animation/AnimNodeBase.h"
#include "AnimNodes/AnimNode_SequenceEvaluator.h"
#include "CoreMinimal.h"
#include "EGTurnInPlaceTypes.h"
#include "UObject/ObjectKey.h"

#include "EGAnimNode_TurnInPlace.generated.h"

class UAnimSequenceBase;
class UEGTurnInPlaceComponent;
struct FBoneContainer;

/**
 * FAnimNode_EGTurnInPlace ("Turn In Place" in the AnimBP)
 *
 * The whole anim side of turn in place in one node, in any anim instance. Put it after the
 * locomotion (or inside its Idle state) with that pose as Source. It reads the owner's
 * UEGTurnInPlaceComponent each frame and plays Idle -> Turn -> Recovery -> Idle by itself,
 * crossfading between the source pose and the turn clips, then writes the turn curves into its
 * output pose for the component to consume.
 *
 * Clips without the turn curves are read from their root track. Steps without a clip (and settle
 * turns) turn procedurally on the source pose; pair it with the Foot Lock node.
 *
 * The curves it writes must reach the output pose at full weight: no blend, Inertialization or
 * layer after it may scale them (add RemainingTurnYaw and TurnYawWeight to any Inertialization's
 * Filtered Curves).
 */
USTRUCT(BlueprintInternalUseOnly)
struct UNREALEXTENDEDGAMEPLAY_API FAnimNode_EGTurnInPlace : public FAnimNode_Base
{
	GENERATED_BODY()

	/** The pose while not turning: the idle pose, or the whole locomotion when the node sits after it. */
	UPROPERTY(EditAnywhere, Category = Links)
	FPoseLink Source;

	/** Blend from the source pose into a turn, and from one turn into the next. */
	UPROPERTY(EditAnywhere, Category = "Turn In Place", meta = (PinHiddenByDefault, ClampMin = "0", Units = "Seconds"))
	float TurnBlendTime = 0.2f;

	/** Blend back to the source pose once a turn's recovery has played out. */
	UPROPERTY(EditAnywhere, Category = "Turn In Place", meta = (PinHiddenByDefault, ClampMin = "0", Units = "Seconds"))
	float RecoveryBlendTime = 0.25f;

	/** Blend back to the source pose when a turn stops early (the character moves, or turn in place stops being enabled). */
	UPROPERTY(EditAnywhere, Category = "Turn In Place", meta = (PinHiddenByDefault, ClampMin = "0", Units = "Seconds"))
	float AbortBlendTime = 0.15f;

	/**
	 * Hold the root bone at its reference pose while a turn clip plays: the capsule does the
	 * turning. Leave on unless the clips are authored with the root already locked.
	 */
	UPROPERTY(EditAnywhere, Category = "Turn In Place")
	bool bLockRootBone = true;

	/** Clips without curves: remaining root yaw within this counts as finished (the curve modifier's Finished Tolerance). */
	UPROPERTY(EditAnywhere, Category = "Turn In Place|Root Track", meta = (ClampMin = "0", Units = "Degrees"))
	float RootYawFinishedTolerance = 1.0f;

	/** Clips without curves: the turn ends this long before the clip ends at the latest, for the recovery. */
	UPROPERTY(EditAnywhere, Category = "Turn In Place|Root Track", meta = (ClampMin = "0", Units = "Seconds"))
	float RootYawRecoveryTime = 0.2f;

	/** Clips without curves: how often the root track is sampled, once per clip. */
	UPROPERTY(EditAnywhere, Category = "Turn In Place|Root Track", meta = (ClampMin = "5", ClampMax = "120"))
	float RootYawSampleRate = 30.0f;

	// FAnimNode_Base
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
	virtual bool HasPreUpdate() const override { return true; }
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;

private:
	enum class EPhase : uint8
	{
		Idle,
		Turn,
		Recovery
	};

	/** What the node knows about a clip: whether it has the curves, and its root yaw track if not. */
	struct FClipInfo
	{
		bool bHasCurves = false;
		bool bApproximated = false;
		FEGTurnYawTrack Track;
	};

	static constexpr int32 NumPlayers = 2;

	FAnimNode_SequenceEvaluator_Standalone& GetPlayer(int32 Index) { return Index == 0 ? PlayerA : PlayerB; }
	TObjectPtr<UAnimSequenceBase>& GetPlayerSequence(int32 Index) { return Index == 0 ? PlayerSequenceA : PlayerSequenceB; }

	void ResetState();
	void StartTurn(const FAnimationUpdateContext& Context, const FEGTurnInPlaceGraphInput& FrameInput);
	void EndTurn(float BlendTime);
	void BlendTo(int32 Target, float BlendTime);
	void UpdateBlend(float DeltaTime);
	void SetPlayerTime(int32 Index, double Time);

	/** The turn curves at the node's current time. */
	FEGTurnInPlaceCurves EvaluateTurnCurves(const FAnimInstanceProxy* Proxy, bool bRecovery);
	const FClipInfo& FindOrBuildClipInfo(const FAnimInstanceProxy* Proxy, const UAnimSequenceBase* Sequence);
	bool SampleRootYaw(const UAnimSequenceBase* Sequence, const FBoneContainer& RequiredBones, TArray<double>& OutTimes, TArray<double>& OutYaws) const;

	/** From the component on the game thread (PreUpdate). Holds the turn set for the worker. */
	UPROPERTY(Transient)
	FEGTurnInPlaceGraphInput Input;

	/** Keep the clips the players hold alive even if the turn set is swapped mid-blend. */
	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> PlayerSequenceA = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAnimSequenceBase> PlayerSequenceB = nullptr;

	FAnimNode_SequenceEvaluator_Standalone PlayerA;
	FAnimNode_SequenceEvaluator_Standalone PlayerB;

	TWeakObjectPtr<const UEGTurnInPlaceComponent> CachedComponent;
	TWeakObjectPtr<const AActor> CachedOwner;

	FEGTurnInPlaceNodeState Node;
	EPhase Phase = EPhase::Idle;
	bool bTurnedThisEntry = false;

	/** The player of the playing turn; INDEX_NONE while idle and for a procedural turn. */
	int32 CurrentPlayer = INDEX_NONE;

	/** [0] the source pose, [1 + i] player i. They sum to 1. */
	float Weights[1 + NumPlayers] = { 1.0f, 0.0f, 0.0f };
	float FromWeights[1 + NumPlayers] = { 1.0f, 0.0f, 0.0f };
	int32 BlendTarget = 0;
	float BlendDuration = 0.0f;
	float BlendElapsed = 0.0f;

	/** Written into the output pose, and what the component reads next. */
	FEGTurnInPlaceCurves OutputCurves;

	TMap<TObjectKey<UAnimSequenceBase>, FClipInfo> ClipInfos;

	/** Set on the worker, reported from PreUpdate on the game thread. */
	FString PendingWarning;
};
