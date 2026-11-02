// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementNetTypes.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementTypes.h"

#include "EGPlacementSessionTypes.generated.h"

class UEGPlacementAimProvider;
class UEGPlacementProfile;
class UEGPlacementSessionComponent;

UENUM(BlueprintType)
enum class EEGPlacementSessionState : uint8
{
	Idle,
	/** Loading the selection's visual and soft data. */
	Preparing,
	Previewing,
	/** A commit was dispatched and its outcome has not arrived. */
	AwaitingOutcome,
};

/** Whether a semantic input was used. Handled input must not fall through to the equipped item. */
UENUM(BlueprintType)
enum class EEGPlacementInputResult : uint8
{
	NotHandled,
	Handled,
};

/** What a mode does when its result is invalid. */
UENUM(BlueprintType)
enum class EEGPlacementModeOnInvalid : uint8
{
	/** Show this mode's invalid result. */
	Stop,
	/** Let the next mode try. */
	FallThrough,
};

/** What the session does after a commit was applied (or dispatched, by a handler that cannot report outcomes). */
UENUM(BlueprintType)
enum class EEGPlacementAfterCommit : uint8
{
	/** Keep previewing the same selection: place another. Whether there is another is the project's call. */
	PreviewAgain,
	EndSession,
};

UENUM(BlueprintType)
enum class EEGPlacementDispatch : uint8
{
	Dispatched,
	Refused,
};

UENUM(BlueprintType)
enum class EEGPlacementOutcomeKind : uint8
{
	Applied,
	Rejected,
	Failed,
	Cancelled,
};

/** One way the session can place the selection, tried in order. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementMode
{
	GENERATED_BODY()

	/** Routed by the commit handler: "stock the shelf", "place in the world". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FGameplayTag ModeTag;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadWrite, Category = "Placement")
	TObjectPtr<UEGPlacementAimProvider> Aim;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	TObjectPtr<UEGPlacementProfile> Profile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	EEGPlacementModeOnInvalid OnInvalid = EEGPlacementModeOnInvalid::Stop;
};

/** What a session places. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementSelection
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FEGPlacementSpec Spec;

	/** What the preview draws. Empty: the ActorClass renderer builds it from ActorClass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FEGPlacementVisual Visual;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	TSoftClassPtr<AActor> ActorClass;

	/** The project's identity for the item. Empty uses Spec.Source. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	FEGPlacementHandle Source;

	/** Uniform scale of the placed object. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (ClampMin = "0.01"))
	float Scale = 1.0f;

	/** Empty uses the session's default modes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	TArray<FEGPlacementMode> Modes;

	/** The actor being moved, for move sessions. Ignored by the session's traces. */
	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	TWeakObjectPtr<AActor> MovedActor;

	/** More actors the session's traces ignore (the carried item, the hands). */
	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	TArray<TObjectPtr<AActor>> IgnoredActors;

	/** Copied into every evaluation's context for project rules. */
	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	FInstancedStruct ContextData;

	/** Extra soft data to load before previewing. */
	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	TArray<FSoftObjectPath> Preload;

	FEGPlacementHandle GetSource() const { return Source.IsValid() ? Source : Spec.Source; }
};

/** What a commit handler gets. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementCommitRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	FGameplayTag ModeTag;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	FEGPlacementHandle Source;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	FEGPlacementSpec Spec;

	/** The evaluation the requester confirmed, fresh at confirm time. */
	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	FEGPlacementEvaluation Evaluation;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	FEGPlacementIntent Intent;

	/** Ready for the project's RPC or command. */
	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	FEGPlacementProposal Proposal;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	TObjectPtr<UEGPlacementProfile> Profile = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	TWeakObjectPtr<AActor> Requester;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	TWeakObjectPtr<UEGPlacementSessionComponent> Session;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	TWeakObjectPtr<AActor> MovedActor;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	FInstancedStruct ContextData;
};

/** A commit's final answer, reported exactly once. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementOutcome
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	EEGPlacementOutcomeKind Kind = EEGPlacementOutcomeKind::Failed;

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	TArray<FEGPlacementReason> Reasons;

	/** The placed or moved actor, when there is one. */
	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	TWeakObjectPtr<AActor> Actor;

	/** The pose actually applied. */
	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	FTransform AppliedPose = FTransform::Identity;

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	FInstancedStruct ProjectData;

	static FEGPlacementOutcome Make(EEGPlacementOutcomeKind Kind, AActor* Actor = nullptr, const FTransform& Pose = FTransform::Identity)
	{
		FEGPlacementOutcome Outcome;
		Outcome.Kind = Kind;
		Outcome.Actor = Actor;
		Outcome.AppliedPose = Pose;
		return Outcome;
	}
};

/** A commit handler's immediate answer. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementDispatchResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	EEGPlacementDispatch Dispatch = EEGPlacementDispatch::Refused;

	UPROPERTY(BlueprintReadWrite, Category = "Placement")
	TArray<FEGPlacementReason> Reasons;

	static FEGPlacementDispatchResult Dispatched()
	{
		FEGPlacementDispatchResult Result;
		Result.Dispatch = EEGPlacementDispatch::Dispatched;
		return Result;
	}

	static FEGPlacementDispatchResult Refused(const FGameplayTag& Tag, FText Text, const UObject* Raiser = nullptr)
	{
		FEGPlacementDispatchResult Result;
		Result.Dispatch = EEGPlacementDispatch::Refused;
		Result.Reasons.Emplace(Tag, MoveTemp(Text), Raiser);
		return Result;
	}
};

/**
 * Payload of the Extended.Gameplay.Placement.Event.* messages, for listeners such as tutorials or
 * analytics. Listeners never replace the per-call outcome.
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGPlacementSessionMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	TWeakObjectPtr<UEGPlacementSessionComponent> Session;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	TWeakObjectPtr<AActor> Requester;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	FGameplayTag ModeTag;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	FEGPlacementHandle Source;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	FTransform Pose = FTransform::Identity;

	/** Set for Outcome messages. */
	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	bool bHasOutcome = false;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	EEGPlacementOutcomeKind Outcome = EEGPlacementOutcomeKind::Failed;

	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	TArray<FEGPlacementReason> Reasons;
};
