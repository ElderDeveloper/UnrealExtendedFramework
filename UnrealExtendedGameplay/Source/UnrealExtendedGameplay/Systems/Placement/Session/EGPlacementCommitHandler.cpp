// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGPlacementCommitHandler.h"

#include "Assets/EFAssetLoading.h"
#include "Containers/Ticker.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementEvaluator.h"
#include "UnrealExtendedGameplay/Systems/Placement/Core/EGPlacementTags.h"
#include "UnrealExtendedGameplay/Systems/Placement/Session/EGPlacementSessionComponent.h"

#define LOCTEXT_NAMESPACE "EGPlacementCommit"

FEGPlacementDispatchResult UEGPlacementCommitHandler::Commit(const FEGPlacementCommitRequest& Request, FOnComplete OnComplete)
{
	TSharedRef<bool> bDropped = MakeShared<bool>(false);
	const int64 CallId = Pending.Add([OnComplete = MoveTemp(OnComplete), bDropped](const FEGPlacementOutcome& Outcome)
	{
		if (!*bDropped && OnComplete)
		{
			OnComplete(Outcome);
		}
	});

	const int64 PreviousDispatching = DispatchingCallId;
	DispatchingCallId = CallId;
	FEGPlacementDispatchResult Result = DispatchCommit(Request, CallId);
	DispatchingCallId = PreviousDispatching;

	TOptional<FEGPlacementOutcome> Early;
	for (int32 Index = 0; Index < EarlyOutcomes.Num(); ++Index)
	{
		if (EarlyOutcomes[Index].Key == CallId)
		{
			Early = EarlyOutcomes[Index].Value;
			EarlyOutcomes.RemoveAt(Index);
			break;
		}
	}

	if (Result.Dispatch == EEGPlacementDispatch::Refused)
	{
		// A refused call never completes.
		*bDropped = true;
		Pending.Complete(CallId, FEGPlacementOutcome());
		if (Result.Reasons.Num() == 0)
		{
			Result.Reasons.Emplace(TAG_EGPlacement_Reason, LOCTEXT("Refused", "The commit was refused."), this);
		}
		return Result;
	}

	++FEGPlacementStats::Commits;
	if (Early.IsSet())
	{
		// Completed inside DispatchCommit: deliver on the next tick so the caller sees Dispatched first.
		TWeakObjectPtr<UEGPlacementCommitHandler> WeakThis(this);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis, CallId, Outcome = Early.GetValue()](float)
		{
			if (UEGPlacementCommitHandler* Handler = WeakThis.Get())
			{
				Handler->Pending.Complete(CallId, Outcome);
			}
			return false;
		}));
	}
	else if (!ReportsOutcomes())
	{
		// Dispatch is final for handlers that cannot report; the callback is dropped, never faked.
		*bDropped = true;
		Pending.Complete(CallId, FEGPlacementOutcome());
	}
	return Result;
}

bool UEGPlacementCommitHandler::CompleteCommit(int64 CallId, const FEGPlacementOutcome& Outcome)
{
	if (CallId == DispatchingCallId && CallId != 0)
	{
		if (EarlyOutcomes.ContainsByPredicate([CallId](const TPair<int64, FEGPlacementOutcome>& Entry) { return Entry.Key == CallId; }))
		{
			return false;
		}
		EarlyOutcomes.Emplace(CallId, Outcome);
		return true;
	}
	return Pending.Complete(CallId, Outcome);
}

void UEGPlacementCommitHandler::CancelPendingCommits()
{
	EarlyOutcomes.Reset();
	Pending.CompleteAll(FEGPlacementOutcome::Make(EEGPlacementOutcomeKind::Cancelled));
}

void UEGPlacementCommitHandler::BeginDestroy()
{
	EarlyOutcomes.Reset();
	Pending.Reset();
	Super::BeginDestroy();
}

FEGPlacementDispatchResult UEGPlacementCommitHandler::DispatchCommit_Implementation(const FEGPlacementCommitRequest& Request, int64 CallId)
{
	return FEGPlacementDispatchResult::Refused(TAG_EGPlacement_Reason_NotConfigured, LOCTEXT("NoDispatch", "The commit handler does not implement DispatchCommit."), this);
}

bool UEGPlacementCommitHandler::HasLocalAuthority(const FEGPlacementCommitRequest& Request)
{
	const AActor* Requester = Request.Requester.Get();
	const UWorld* World = Requester ? Requester->GetWorld() : (Request.Session.IsValid() ? Request.Session->GetWorld() : nullptr);
	return World && World->GetNetMode() != NM_Client;
}

// ---------------------------------------------------------------------------------------------
// Spawn actor
// ---------------------------------------------------------------------------------------------

FEGPlacementDispatchResult UEGPlacementCommit_SpawnActor::DispatchCommit_Implementation(const FEGPlacementCommitRequest& Request, int64 CallId)
{
	if (!HasLocalAuthority(Request))
	{
		return FEGPlacementDispatchResult::Refused(TAG_EGPlacement_Reason_NoAuthority, LOCTEXT("SpawnAuthority", "Spawning needs authority; send the proposal through the project's command."), this);
	}
	const TSoftClassPtr<AActor>& ClassRef = Request.ActorClass.IsNull() ? DefaultActorClass : Request.ActorClass;
	UClass* Class = EFAssets::Resolve(ClassRef, TEXT("UEGPlacementCommit_SpawnActor"));
	AActor* Requester = Request.Requester.Get();
	UWorld* World = Requester ? Requester->GetWorld() : (Request.Session.IsValid() ? Request.Session->GetWorld() : nullptr);
	if (!Class || !World)
	{
		return FEGPlacementDispatchResult::Refused(TAG_EGPlacement_Reason_NotConfigured, LOCTEXT("NoClass", "No actor class to spawn."), this);
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = CollisionHandling;
	Params.Owner = Requester;
	Params.Instigator = Cast<APawn>(Requester);
	const FTransform Pose = Request.Evaluation.Pose;
	AActor* Spawned = World->SpawnActor(Class, &Pose, Params);
	FEGPlacementOutcome Outcome = FEGPlacementOutcome::Make(Spawned ? EEGPlacementOutcomeKind::Applied : EEGPlacementOutcomeKind::Failed, Spawned, Pose);
	if (!Spawned)
	{
		Outcome.Reasons.Emplace(TAG_EGPlacement_Reason_Blocked, LOCTEXT("SpawnFailed", "The actor could not be spawned."), this);
	}
	CompleteCommit(CallId, Outcome);
	return FEGPlacementDispatchResult::Dispatched();
}

// ---------------------------------------------------------------------------------------------
// Move actor
// ---------------------------------------------------------------------------------------------

void UEGPlacementCommit_MoveActor::OnSessionBegin_Implementation(UEGPlacementSessionComponent* Session, const FEGPlacementSelection& Selection)
{
	Moving = Selection.MovedActor;
	bCommitted = false;
	bOriginStored = Moving.IsValid();
	Origin = bOriginStored ? Moving->GetActorTransform() : FTransform::Identity;
}

void UEGPlacementCommit_MoveActor::OnPreviewUpdated_Implementation(UEGPlacementSessionComponent* Session, const FEGPlacementEvaluation& Evaluation, FGameplayTag ModeTag)
{
	AActor* Actor = Moving.Get();
	if (bMoveWhilePreviewing && Actor && Actor->HasAuthority() && Evaluation.IsValid())
	{
		Actor->SetActorTransform(FTransform(Evaluation.Pose.GetRotation(), Evaluation.Pose.GetLocation(), Actor->GetActorScale3D()), false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void UEGPlacementCommit_MoveActor::OnSessionEnd_Implementation(UEGPlacementSessionComponent* Session, bool bCancelled)
{
	AActor* Actor = Moving.Get();
	if (bMoveWhilePreviewing && bOriginStored && !bCommitted && Actor && Actor->HasAuthority())
	{
		Actor->SetActorTransform(Origin, false, nullptr, ETeleportType::TeleportPhysics);
	}
	Moving.Reset();
	bOriginStored = false;
}

FEGPlacementDispatchResult UEGPlacementCommit_MoveActor::DispatchCommit_Implementation(const FEGPlacementCommitRequest& Request, int64 CallId)
{
	AActor* Actor = Request.MovedActor.Get();
	if (!Actor)
	{
		return FEGPlacementDispatchResult::Refused(TAG_EGPlacement_Reason_InvalidInput, LOCTEXT("NoActor", "Nothing to move."), this);
	}
	if (!Actor->HasAuthority())
	{
		return FEGPlacementDispatchResult::Refused(TAG_EGPlacement_Reason_NoAuthority, LOCTEXT("MoveAuthority", "Moving needs authority; send the proposal through the project's command."), this);
	}
	const FTransform Pose(Request.Evaluation.Pose.GetRotation(), Request.Evaluation.Pose.GetLocation(), Actor->GetActorScale3D());
	Actor->SetActorTransform(Pose, false, nullptr, ETeleportType::TeleportPhysics);
	bCommitted = true;
	Origin = Pose;
	CompleteCommit(CallId, FEGPlacementOutcome::Make(EEGPlacementOutcomeKind::Applied, Actor, Pose));
	return FEGPlacementDispatchResult::Dispatched();
}

#undef LOCTEXT_NAMESPACE
