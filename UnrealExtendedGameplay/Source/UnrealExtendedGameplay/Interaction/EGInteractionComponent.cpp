// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGInteractionComponent.h"
#include "Internationalization/StringTableRegistry.h"

#include "Assets/EFAssetLoading.h"
#include "Blueprint/UserWidget.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "EGInteractableActor.h"
#include "EGInteractableInterface.h"
#include "EnhancedInputComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagAssetInterface.h"
#include "InputAction.h"
#include "UI/EGInteractionPromptWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogEGInteraction, Log, All);

namespace EGInteraction
{
	// Functions, not namespace-scope constants: LOCTABLE resolves the string table entry as soon as it is
	// built, which starts ICU. At static-init time in a packaged (monolithic) game the pak holding the ICU
	// data is not mounted yet, so a namespace-scope LOCTABLE kills the exe before main (exit 777006).
	FText NoTargetReason() { return LOCTABLE("ExtendedFramework", "EGInteraction.NoTarget"); }
	FText RejectedReason() { return LOCTABLE("ExtendedFramework", "EGInteraction.Rejected"); }
	FText NotAllowedReason() { return LOCTABLE("ExtendedFramework", "EGInteraction.NotAllowed"); }
	FText NotInteractableReason() { return LOCTABLE("ExtendedFramework", "EGInteraction.NotInteractable"); }
	FText TargetMismatchReason() { return LOCTABLE("ExtendedFramework", "EGInteraction.TargetMismatch"); }
	FText OutOfRangeReason() { return LOCTABLE("ExtendedFramework", "EGInteraction.OutOfRange"); }
	FText NotFacingReason() { return LOCTABLE("ExtendedFramework", "EGInteraction.NotFacing"); }
	FText RefusedReason() { return LOCTABLE("ExtendedFramework", "EGInteraction.Refused"); }
	FText BusyReason() { return LOCTABLE("ExtendedFramework", "EGInteraction.Busy"); }
}

UEGInteractionComponent::UEGInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UEGInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	// Whether this component ticks depends on who controls the pawn, and on a
	// client that answer changes after BeginPlay: the controller replicates in
	// later, through OnRep_Controller. Deciding once here would leave a client
	// pawn's tick off for good, so the decision is re-run on every controller
	// change for the life of the component.
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		OwnerPawn->ReceiveControllerChangedDelegate.AddUniqueDynamic(this, &UEGInteractionComponent::HandleOwnerControllerChanged);
	}

	// Binding waits for this load (the tick retries it) instead of loading the input action synchronously.
	if (bAutoBindInteractionInput && !InteractionInputAction.IsNull())
	{
		InteractionInputActionLoadHandle = EFAssets::Preload({ InteractionInputAction.ToSoftObjectPath() });
	}

	RefreshOwnerControlState();
}

void UEGInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		OwnerPawn->ReceiveControllerChangedDelegate.RemoveDynamic(this, &UEGInteractionComponent::HandleOwnerControllerChanged);
	}

	ClearInteractionInputBindings();
	InteractionInputActionLoadHandle.Reset();
	InteractionEnd();
	ClearFocus();
	HidePromptWidget();
	Super::EndPlay(EndPlayReason);
}

void UEGInteractionComponent::HandleOwnerControllerChanged(APawn* /*Pawn*/, AController* /*OldController*/, AController* /*NewController*/)
{
	RefreshOwnerControlState();
}

void UEGInteractionComponent::RefreshOwnerControlState()
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const bool bLocallyControlled = OwnerPawn && OwnerPawn->IsLocallyControlled();
	const bool bAuthority = GetOwner() && GetOwner()->HasAuthority();

	// The local owner traces and draws prompts; authority pumps holds for every
	// pawn it owns. A remote proxy on a client is neither and stays idle.
	SetComponentTickEnabled(bLocallyControlled || bAuthority);

	if (bLocallyControlled)
	{
		// The input component may not exist yet when possession lands on a client
		// (it is created in PawnClientRestart); the tick retries the bind until it does.
		EnsureInteractionInputBindings();
		if (bAutoCreatePromptWidget)
		{
			ShowPromptWidget();
		}
		return;
	}

	// Local control ended, or never began: anything that belongs to a local
	// player goes with it. Each of these is a no-op when there is nothing to drop.
	ClearInteractionInputBindings();
	ClearFocus();
	HidePromptWidget();
}

void UEGInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Authority pumps the running hold for every pawn it owns, local or not.
	InteractionTick(DeltaTime);

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}
	EnsureInteractionInputBindings();

	// Whether the interaction key can currently fire is the mapping context's
	// business: a modal tier replaces the gameplay context and the action simply
	// stops arriving. The component does not second-guess that here.
	if (!IsInteractionAllowed())
	{
		if (FocusedActor.IsValid())
		{
			ClearFocus();
		}
		else if (ActiveInteractionActor.IsValid())
		{
			InteractionEnd();
		}
		return;
	}

	UpdateHoldProgressFeedback();

	TraceAccumulator += DeltaTime;
	if (TraceTickInterval > 0.0f && TraceAccumulator < TraceTickInterval)
	{
		return;
	}
	TraceAccumulator = 0.0f;

	RefreshFocus();
}

// =====================================================================
// Input
// =====================================================================

void UEGInteractionComponent::RefreshInteractionInputBindings()
{
	ClearInteractionInputBindings();
	EnsureInteractionInputBindings();
}

void UEGInteractionComponent::ClearInteractionInputBindings()
{
	if (UEnhancedInputComponent* EnhancedInput = BoundInputComponent.Get())
	{
		if (InteractionStartedBindingHandle != 0)
		{
			EnhancedInput->RemoveBindingByHandle(InteractionStartedBindingHandle);
		}
		if (InteractionCompletedBindingHandle != 0)
		{
			EnhancedInput->RemoveBindingByHandle(InteractionCompletedBindingHandle);
		}
		if (InteractionCanceledBindingHandle != 0)
		{
			EnhancedInput->RemoveBindingByHandle(InteractionCanceledBindingHandle);
		}
	}

	BoundInputComponent.Reset();
	BoundInteractionInputAction.Reset();
	InteractionStartedBindingHandle = 0;
	InteractionCompletedBindingHandle = 0;
	InteractionCanceledBindingHandle = 0;
}

void UEGInteractionComponent::EnsureInteractionInputBindings()
{
	if (!bAutoBindInteractionInput || InteractionInputAction.IsNull())
	{
		if (BoundInputComponent.IsValid())
		{
			ClearInteractionInputBindings();
		}
		return;
	}

	// Still loading from BeginPlay: the tick retries, as it does while the input component is missing.
	if (InteractionInputActionLoadHandle.IsValid() && InteractionInputActionLoadHandle->IsLoadingInProgress() && !InteractionInputAction.Get())
	{
		return;
	}

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	UEnhancedInputComponent* EnhancedInput = OwnerPawn
		? Cast<UEnhancedInputComponent>(OwnerPawn->InputComponent)
		: nullptr;
	UInputAction* InputAction = EFAssets::Resolve(InteractionInputAction, TEXT("UEGInteractionComponent"));
	if (!EnhancedInput || !InputAction)
	{
		if (BoundInputComponent.IsValid())
		{
			ClearInteractionInputBindings();
		}
		return;
	}

	if (BoundInputComponent.Get() == EnhancedInput
		&& BoundInteractionInputAction.Get() == InputAction)
	{
		return;
	}

	ClearInteractionInputBindings();
	BoundInputComponent = EnhancedInput;
	BoundInteractionInputAction = InputAction;
	InteractionStartedBindingHandle = EnhancedInput->BindAction(
		InputAction,
		ETriggerEvent::Started,
		this,
		&UEGInteractionComponent::InteractionStart).GetHandle();
	InteractionCompletedBindingHandle = EnhancedInput->BindAction(
		InputAction,
		ETriggerEvent::Completed,
		this,
		&UEGInteractionComponent::InteractionEnd).GetHandle();
	InteractionCanceledBindingHandle = EnhancedInput->BindAction(
		InputAction,
		ETriggerEvent::Canceled,
		this,
		&UEGInteractionComponent::InteractionEnd).GetHandle();
}

// =====================================================================
// Actions
// =====================================================================

void UEGInteractionComponent::RefreshFocus()
{
	if (!IsInteractionAllowed())
	{
		ClearFocus();
		return;
	}

	if (bUseMouseTrace)
	{
		PerformMouseTrace();
	}
	else
	{
		PerformTrace();
	}
}

bool UEGInteractionComponent::TryInteract()
{
	if (!FocusedActor.IsValid())
	{
		NotifyInteractionFailed(nullptr, EGInteraction::NoTargetReason(), TEXT("nothing is focused"));
		return false;
	}

	InteractionStart();
	InteractionEnd();
	return true;
}

void UEGInteractionComponent::InteractionStart()
{
	AActor* TargetActor = FocusedActor.Get();
	if (!TargetActor || ActiveInteractionActor.IsValid())
	{
		return;
	}

	if (!IsInteractionAllowed())
	{
		// Refused here, before anything is sent: the server never sees this request.
		NotifyInteractionFailed(TargetActor, EGInteraction::NotAllowedReason(), DescribeOwnerGate());
		return;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		// The authority's own request: BeginInteractionOnServer logs any refusal itself.
		FRejection Rejection;
		BeginInteractionOnServer(TargetActor, LastHitResult, Rejection);
		return;
	}

	// The client tracks the target locally so hold UI can run before the server confirms.
	// Close an earlier rejected request even if its optimistic actor was cleared.
	InteractionEnd();
	ActiveInteractionActor = TargetActor;
	RequestedInteractionActor = TargetActor;
	RequestedInteractionAttemptId = ++NextInteractionAttemptId;
	ServerInteractionStart(RequestedInteractionAttemptId, TargetActor, LastHitResult);
}

void UEGInteractionComponent::InteractionTick(float DeltaTime)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (AActor* ActiveActor = ActiveInteractionActor.Get())
	{
		IEGInteractableInterface::Execute_InteractionTick(ActiveActor, GetOwner(), DeltaTime);
	}
	else if (ActiveInteractionActor.IsStale())
	{
		ActiveInteractionActor.Reset();
	}
}

void UEGInteractionComponent::InteractionEnd()
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		EndInteractionOnServer(ActiveInteractionActor.Get());
		ServerInteractionAttemptId = 0;
		return;
	}

	const int64 AttemptId = RequestedInteractionAttemptId;
	RequestedInteractionAttemptId = 0;
	RequestedInteractionActor.Reset();
	ActiveInteractionActor.Reset();
	if (AttemptId > 0)
	{
		// Send even when the target was destroyed or a rejection already cleared local UI.
		ServerInteractionEnd(AttemptId);
	}
}

void UEGInteractionComponent::EnableMouseInteraction()
{
	bUseMouseTrace = true;
	if (PromptWidgetInstance)
	{
		PromptWidgetInstance->SetCursorFollowMode(true);
	}
	RefreshFocus();
}

void UEGInteractionComponent::DisableMouseInteraction()
{
	bUseMouseTrace = false;
	ClearFocus();
	if (PromptWidgetInstance)
	{
		PromptWidgetInstance->SetCursorFollowMode(false);
	}
}

// =====================================================================
// Gating
// =====================================================================

void UEGInteractionComponent::SetInteractionBlocked(bool bBlocked)
{
	if (bInteractionBlocked == bBlocked)
	{
		return;
	}

	bInteractionBlocked = bBlocked;
	if (!IsInteractionAllowed())
	{
		ClearFocus();
		UE_CLOG(bDebugLogTraceResults, LogEGInteraction, Log, TEXT("[%s] Interaction BLOCKED by game code"), *GetNameSafe(GetOwner()));
	}
	else
	{
		UE_CLOG(bDebugLogTraceResults, LogEGInteraction, Log, TEXT("[%s] Interaction UNBLOCKED"), *GetNameSafe(GetOwner()));
	}
}

bool UEGInteractionComponent::IsInteractionAllowed() const
{
	// Mouse tracing is the cursor-visible case: a dialogue screen blocks the world but not clicks.
	if (bInteractionBlocked && !(bUseMouseTrace && bMouseTraceIgnoresBlocking))
	{
		return false;
	}

	return CanOwnerInteract();
}

FString UEGInteractionComponent::DescribeOwnerGate() const
{
	// Mirrors IsInteractionAllowed, for the logs.
	if (bInteractionBlocked && !(bUseMouseTrace && bMouseTraceIgnoresBlocking))
	{
		return TEXT("blocked by game code (SetInteractionBlocked)");
	}
	return CanOwnerInteract() ? TEXT("allowed") : TEXT("CanOwnerInteract returned false");
}

FHitResult UEGInteractionComponent::GetInteractionHit(const AActor* Interactor)
{
	const UEGInteractionComponent* Component = Interactor ? Interactor->FindComponentByClass<UEGInteractionComponent>() : nullptr;
	return Component ? Component->LastHitResult : FHitResult();
}

// =====================================================================
// Authority
// =====================================================================

void UEGInteractionComponent::ServerInteractionStart_Implementation(int64 AttemptId, AActor* TargetActor, FHitResult ClientHitResult)
{
	if (AttemptId <= LastServerInteractionAttemptId)
	{
		return;
	}
	LastServerInteractionAttemptId = AttemptId;
	// Newer input supersedes an abandoned hold; a delayed old end cannot end the new one.
	EndInteractionOnServer(ActiveInteractionActor.Get());
	ServerInteractionAttemptId = 0;
	FRejection Rejection;
	const bool bSuccess = BeginInteractionOnServer(TargetActor, ClientHitResult, Rejection);
	if (bSuccess)
	{
		ServerInteractionAttemptId = AttemptId;
	}
	ClientInteractionResult(AttemptId, bSuccess, !bSuccess, TargetActor, Rejection.Reason, Rejection.Detail);
}

void UEGInteractionComponent::ServerInteractionEnd_Implementation(int64 AttemptId)
{
	if (AttemptId != ServerInteractionAttemptId || AttemptId <= 0)
	{
		return;
	}
	AActor* TargetActor = ActiveInteractionActor.Get();
	EndInteractionOnServer(TargetActor);
	ServerInteractionAttemptId = 0;
	ClientInteractionResult(AttemptId, true, true, TargetActor, FText::GetEmpty(), FString());
}

void UEGInteractionComponent::ClientInteractionResult_Implementation(int64 AttemptId, bool bSuccess, bool bFinished, AActor* TargetActor, const FText& FailureReason, const FString& FailureDetail)
{
	if (AttemptId != NextInteractionAttemptId)
	{
		// A newer press superseded this attempt, so nothing is shown for it, but the reason is kept.
		UE_CLOG(!bSuccess, LogEGInteraction, Log, TEXT("[%s] Server rejected superseded attempt %lld on %s: %s (%s)"),
			*GetNameSafe(GetOwner()), AttemptId, *GetNameSafe(TargetActor), *FailureReason.ToString(), *FailureDetail);
		return;
	}
	if ((!bSuccess || bFinished) && RequestedInteractionAttemptId == AttemptId)
	{
		ActiveInteractionActor.Reset();
	}
	if (!bSuccess)
	{
		NotifyInteractionFailed(TargetActor, FailureReason,
			FString::Printf(TEXT("rejected by the server, attempt %lld: %s"), AttemptId, *FailureDetail));
	}
}

void UEGInteractionComponent::NotifyInteractionFailed(AActor* TargetActor, const FText& Reason, const FString& Detail)
{
	// Logged regardless of the debug flags: refusals are rare, and the on-screen notice is gone in seconds.
	UE_LOG(LogEGInteraction, Warning, TEXT("[%s] Interaction with %s failed: %s (%s)"),
		*GetNameSafe(GetOwner()), *GetNameSafe(TargetActor), *Reason.ToString(), *Detail);
	OnInteractionFailed.Broadcast(TargetActor, Reason);
}

bool UEGInteractionComponent::BeginInteractionOnServer(AActor* TargetActor, const FHitResult& HitResult, FRejection& OutRejection)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		OutRejection = FRejection{ EGInteraction::RejectedReason(), TEXT("no authority over the interactor") };
	}
	else if (ActiveInteractionActor.IsValid())
	{
		OutRejection = FRejection{ EGInteraction::BusyReason(),
			FString::Printf(TEXT("already interacting with %s"), *GetNameSafe(ActiveInteractionActor.Get())) };
	}
	else if (IsServerInteractionValid(TargetActor, HitResult, OutRejection))
	{
		// Publish the validated hit so the interactable can read it back with GetInteractionHit().
		LastHitResult = HitResult;
		ActiveInteractionActor = TargetActor;
		IEGInteractableInterface::Execute_InteractionStart(TargetActor, GetOwner());
		return true;
	}

	// Logged regardless of the debug flags: this is the only place the server's reason is written down.
	UE_LOG(LogEGInteraction, Warning, TEXT("[%s] Server rejected interaction with %s: %s (%s)"),
		*GetNameSafe(GetOwner()), *GetNameSafe(TargetActor), *OutRejection.Reason.ToString(), *OutRejection.Detail);
	return false;
}

void UEGInteractionComponent::EndInteractionOnServer(AActor* TargetActor)
{
	AActor* ActiveActor = ActiveInteractionActor.Get();
	if (ActiveActor != TargetActor)
	{
		return;
	}

	// Clear first: target callbacks can themselves begin/end interaction.
	ActiveInteractionActor.Reset();
	if (IsValid(ActiveActor))
	{
		IEGInteractableInterface::Execute_InteractionEnd(ActiveActor, GetOwner());
	}
}

bool UEGInteractionComponent::IsServerInteractionValid(AActor* TargetActor, const FHitResult& HitResult, FRejection& OutRejection) const
{
	if (!TargetActor)
	{
		// The client names its target by network reference, so an actor the server does not know
		// (destroyed meanwhile, or one that only ever existed on the client) arrives as null.
		OutRejection = FRejection{ EGInteraction::NotInteractableReason(),
			TEXT("the target did not resolve on the server (destroyed, or never known to it)") };
		return false;
	}

	if (!TargetActor->GetClass()->ImplementsInterface(UEGInteractableInterface::StaticClass()))
	{
		OutRejection = FRejection{ EGInteraction::NotInteractableReason(),
			FString::Printf(TEXT("%s does not implement the interactable interface"), *TargetActor->GetClass()->GetName()) };
		return false;
	}

	if (!IsInteractionAllowed())
	{
		OutRejection = FRejection{ EGInteraction::NotAllowedReason(), DescribeOwnerGate() };
		return false;
	}

	if (HitResult.GetActor() && HitResult.GetActor() != TargetActor)
	{
		OutRejection = FRejection{ EGInteraction::TargetMismatchReason(),
			FString::Printf(TEXT("the client's hit is on %s, not on the requested target"), *GetNameSafe(HitResult.GetActor())) };
		return false;
	}

	const float ValidationDistance = InteractionRange + TraceRadius + ValidationSlack;
	const FVector OwnerLocation = GetOwner()->GetActorLocation();
	FVector ClosestPoint = TargetActor->GetActorLocation();
	const UPrimitiveComponent* HitComponent = HitResult.GetComponent();
	if (HitComponent && HitComponent->GetOwner() != TargetActor)
	{
		OutRejection = FRejection{ EGInteraction::TargetMismatchReason(),
			FString::Printf(TEXT("the client's hit component %s belongs to %s, not to the requested target"),
				*HitComponent->GetName(), *GetNameSafe(HitComponent->GetOwner())) };
		return false;
	}
	float DistanceToTarget = HitComponent ? HitComponent->GetClosestPointOnCollision(OwnerLocation, ClosestPoint) : -1.0f;
	if (DistanceToTarget < 0.0f)
	{
		DistanceToTarget = TargetActor->ActorGetDistanceToCollision(OwnerLocation, TraceChannel, ClosestPoint);
	}
	if (DistanceToTarget < 0.0f)
	{
		const FBox Bounds = TargetActor->GetComponentsBoundingBox(true);
		DistanceToTarget = Bounds.IsValid ? FVector::Dist(OwnerLocation, Bounds.GetClosestPointTo(OwnerLocation))
			: FVector::Dist(OwnerLocation, TargetActor->GetActorLocation());
	}
	FVector ViewLocation;
	FRotator ViewRotation;
	GetViewPoint(ViewLocation, ViewRotation);
	if (DistanceToTarget > ValidationDistance)
	{
		// TraceStart and ImpactPoint replicate with the hit, so the client's own view of the request is
		// available here; a large gap between its camera and the server's is movement the server has
		// not caught up with yet.
		OutRejection = FRejection{ EGInteraction::OutOfRangeReason(), FString::Printf(
			TEXT("%.0f cm from the interactor at %s, allowed %.0f (range %.0f + trace radius %.0f + slack %.0f), measured to %s. ")
			TEXT("The client's camera at %s hit at %.0f cm; the server places that camera %.0f cm away"),
			DistanceToTarget, *OwnerLocation.ToCompactString(), ValidationDistance, InteractionRange, TraceRadius, ValidationSlack,
			HitComponent ? *HitComponent->GetName() : TEXT("the actor's collision (no hit component)"),
			*HitResult.TraceStart.ToCompactString(), FVector::Dist(HitResult.TraceStart, HitResult.ImpactPoint),
			FVector::Dist(ViewLocation, HitResult.TraceStart)) };
		return false;
	}

	if (!IsCandidateValid(TargetActor, HitResult, ViewLocation, true, &OutRejection))
	{
		return false;
	}

	if (!IEGInteractableInterface::Execute_CanInteract(TargetActor, GetOwner(), HitResult))
	{
		OutRejection = FRejection{ EGInteraction::RefusedReason(),
			FString::Printf(TEXT("%s refused via CanInteract"), *TargetActor->GetName()) };
		return false;
	}

	return true;
}

// =====================================================================
// Trace
// =====================================================================

void UEGInteractionComponent::PerformTrace()
{
	FVector ViewLocation;
	FRotator ViewRotation;
	GetViewPoint(ViewLocation, ViewRotation);
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * InteractionRange;

	TArray<FHitResult> Hits;
	const bool bAnyHit = GatherTraceHits(ViewLocation, TraceEnd, Hits);

	FHitResult BestHit;
	AActor* BestActor = SelectBestCandidate(Hits, ViewLocation, ViewRotation.Vector(), BestHit);
	if (BestActor)
	{
		LostFocusRemaining = LostFocusGracePeriod;
		SetFocusedActor(BestActor, BestHit);
	}
	else if (FocusedActor.IsValid() && LostFocusRemaining > 0.0f)
	{
		LostFocusRemaining -= GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;
	}
	else
	{
		ClearFocus();
	}

	DrawDebugInfo(ViewLocation, TraceEnd, BestActor != nullptr, bAnyHit && BestActor == nullptr);

	if (bDebugLogTraceResults && DebugLogVerbosity >= EEGInteractionDebugVerbosity::Detailed)
	{
		if (BestActor)
		{
			UE_LOG(LogEGInteraction, Log, TEXT("[%s] Trace hit %s at %.1f"), *GetNameSafe(GetOwner()), *BestActor->GetName(), BestHit.Distance);
		}
		else if (bAnyHit)
		{
			UE_LOG(LogEGInteraction, Log, TEXT("[%s] Trace hit a non-interactable or filtered actor"), *GetNameSafe(GetOwner()));
		}
		else
		{
			UE_LOG(LogEGInteraction, Verbose, TEXT("[%s] Trace miss"), *GetNameSafe(GetOwner()));
		}
	}
}

void UEGInteractionComponent::PerformMouseTrace()
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	APlayerController* PlayerController = OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
	if (!PlayerController)
	{
		ClearFocus();
		return;
	}

	FHitResult HitResult;
	if (PlayerController->GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(TraceChannel), bTraceComplex, HitResult))
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		GetViewPoint(ViewLocation, ViewRotation);
		if (IsCandidateValid(HitResult.GetActor(), HitResult, ViewLocation, false))
		{
			SetFocusedActor(HitResult.GetActor(), HitResult);
			return;
		}
	}
	ClearFocus();
}

bool UEGInteractionComponent::GatherTraceHits(const FVector& Start, const FVector& End, TArray<FHitResult>& OutHits) const
{
	if (!GetWorld())
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EGInteractionTrace), bTraceComplex, GetOwner());
	if (bIgnoreOwnerAndInstigator && GetOwner() && GetOwner()->GetInstigator())
	{
		QueryParams.AddIgnoredActor(GetOwner()->GetInstigator());
	}
	for (const TSoftObjectPtr<AActor>& SoftActor : AdditionalIgnoredActors)
	{
		if (const AActor* Ignored = SoftActor.Get())
		{
			QueryParams.AddIgnoredActor(Ignored);
		}
	}

	FCollisionObjectQueryParams ObjectParams;
	for (const TEnumAsByte<EObjectTypeQuery>& ObjectType : TraceObjectTypes)
	{
		ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(ObjectType));
	}
	const bool bByObjectType = bUseObjectTypeTrace && TraceObjectTypes.Num() > 0;

	// The precise line goes first: a sphere sweep alone cannot pick out small components
	// like a button on a panel.
	FHitResult LineHit;
	const bool bLineHit = bByObjectType
		? GetWorld()->LineTraceSingleByObjectType(LineHit, Start, End, ObjectParams, QueryParams)
		: GetWorld()->LineTraceSingleByChannel(LineHit, Start, End, TraceChannel, QueryParams);
	if (bLineHit)
	{
		OutHits.Add(LineHit);
		return true;
	}

	if (TraceMode == EEGInteractionTraceMode::CameraCenter)
	{
		return false;
	}

	return bByObjectType
		? GetWorld()->SweepMultiByObjectType(OutHits, Start, End, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(TraceRadius), QueryParams)
		: GetWorld()->SweepMultiByChannel(OutHits, Start, End, FQuat::Identity, TraceChannel, FCollisionShape::MakeSphere(TraceRadius), QueryParams);
}

bool UEGInteractionComponent::PassesTagFilter(const AActor* Candidate) const
{
	if (BlockedTags.IsEmpty())
	{
		return true;
	}

	const IGameplayTagAssetInterface* TagInterface = Cast<const IGameplayTagAssetInterface>(Candidate);
	if (!TagInterface)
	{
		return true;
	}

	FGameplayTagContainer ActorTags;
	TagInterface->GetOwnedGameplayTags(ActorTags);
	return !ActorTags.HasAny(BlockedTags);
}

bool UEGInteractionComponent::IsCandidateValid(AActor* Candidate, const FHitResult& HitResult, const FVector& ViewLocation, bool bServerValidation, FRejection* OutRejection) const
{
	if (!Candidate || !Candidate->GetClass()->ImplementsInterface(UEGInteractableInterface::StaticClass()))
	{
		if (OutRejection)
		{
			*OutRejection = FRejection{ EGInteraction::NotInteractableReason(), TEXT("the target is not interactable") };
		}
		return false;
	}

	if (!PassesTagFilter(Candidate))
	{
		if (OutRejection)
		{
			*OutRejection = FRejection{ EGInteraction::RefusedReason(), TEXT("the target carries one of BlockedTags") };
		}
		return false;
	}

	if (!CanOwnerInteractWith(Candidate))
	{
		if (OutRejection)
		{
			*OutRejection = FRejection{ EGInteraction::RefusedReason(), TEXT("CanOwnerInteractWith refused this target") };
		}
		return false;
	}

	if (bMustBeFacing)
	{
		const FVector Direction = (Candidate->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal();
		const float FacingDot = FVector::DotProduct(GetOwner()->GetActorForwardVector(), Direction);
		if (FacingDot < FMath::Cos(FMath::DegreesToRadians(FacingHalfAngle)))
		{
			if (OutRejection)
			{
				*OutRejection = FRejection{ EGInteraction::NotFacingReason(),
					FString::Printf(TEXT("the interactor faces %.0f degrees away from the target, allowed %.0f"),
						FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FacingDot, -1.0f, 1.0f))), FacingHalfAngle) };
			}
			return false;
		}
	}

	// Line of sight decides focus only. The server does not trace it again: a target the player sees a
	// prompt for must not be refused afterwards by geometry that exists only on the server (a sensing
	// volume whose collision is authority-only).
	if (bRequiresLineOfSight && !bServerValidation && GetWorld())
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(EGInteractionLineOfSight), bTraceComplex, GetOwner());
		Params.AddIgnoredActor(Candidate);
		// The impact point, not the actor origin: an origin inside the mesh blocks its own trace.
		const FVector TargetPoint = HitResult.ImpactPoint.IsNearlyZero() ? Candidate->GetActorLocation() : FVector(HitResult.ImpactPoint);
		if (GetWorld()->LineTraceTestByChannel(ViewLocation, TargetPoint, TraceChannel, Params))
		{
			return false;
		}
	}

	// The server asks CanInteract separately, after the cheaper checks have passed.
	return bServerValidation || IEGInteractableInterface::Execute_CanInteract(Candidate, GetOwner(), HitResult);
}

AActor* UEGInteractionComponent::SelectBestCandidate(const TArray<FHitResult>& Hits, const FVector& Start, const FVector& Direction, FHitResult& OutHit) const
{
	AActor* BestActor = nullptr;
	float BestScore = -MAX_flt;
	for (const FHitResult& Hit : Hits)
	{
		AActor* Candidate = Hit.GetActor();
		if (!IsCandidateValid(Candidate, Hit, Start, false))
		{
			continue;
		}

		const float Score = InteractionPriority == EEGInteractionPriority::MostCentered
			? FVector::DotProduct(Direction, (Candidate->GetActorLocation() - Start).GetSafeNormal())
			: -Hit.Distance;
		if (Score > BestScore)
		{
			BestScore = Score;
			BestActor = Candidate;
			OutHit = Hit;
		}
	}
	return BestActor;
}

void UEGInteractionComponent::GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const
{
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (const APlayerController* PlayerController = Cast<APlayerController>(OwnerPawn->GetController()))
		{
			PlayerController->GetPlayerViewPoint(OutLocation, OutRotation);
			return;
		}
		OwnerPawn->GetActorEyesViewPoint(OutLocation, OutRotation);
		return;
	}

	OutLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	OutRotation = GetOwner() ? GetOwner()->GetActorRotation() : FRotator::ZeroRotator;
}

// =====================================================================
// Focus
// =====================================================================

void UEGInteractionComponent::SetFocusedActor(AActor* NewFocusedActor, const FHitResult& HitResult)
{
	if (!NewFocusedActor)
	{
		ClearFocus();
		return;
	}

	const FEGInteractionPresentation NewPresentation =
		IEGInteractableInterface::Execute_GetInteractionPresentation(NewFocusedActor, GetOwner(), HitResult);
	const bool bActorChanged = FocusedActor.Get() != NewFocusedActor;
	const bool bContextChanged = LastHitResult.GetComponent() != HitResult.GetComponent() || LastHitResult.BoneName != HitResult.BoneName;
	// The hint counts too: it can come and go on a target whose line stays the same (a wardrobe's
	// hiding line appears when its doors finish opening, long after "Open" became "Close").
	const bool bTextChanged = !FocusedPresentation.Text.EqualTo(NewPresentation.Text)
		|| !FocusedPresentation.HintText.EqualTo(NewPresentation.HintText);
	const bool bIconChanged = FocusedPresentation.Icon != NewPresentation.Icon
		|| FocusedPresentation.StaticIcon != NewPresentation.StaticIcon
		|| FocusedPresentation.bOverrideStaticIcon != NewPresentation.bOverrideStaticIcon;

	if (bActorChanged && FocusedActor.IsValid())
	{
		InteractionEnd();
		IEGInteractableInterface::Execute_FocusStateChanged(FocusedActor.Get(), false, LastHitResult);
	}

	FocusedActor = NewFocusedActor;
	LastHitResult = HitResult;
	FocusedPresentation = NewPresentation;

	if (bActorChanged)
	{
		IEGInteractableInterface::Execute_FocusStateChanged(NewFocusedActor, true, HitResult);
	}
	if (bActorChanged || bContextChanged || bTextChanged || bIconChanged)
	{
		NotifyFocusUpdated();
	}
}

void UEGInteractionComponent::ClearFocus()
{
	AActor* PreviousActor = FocusedActor.Get();
	if (!PreviousActor && !FocusedActor.IsStale())
	{
		return;
	}

	if (PreviousActor)
	{
		InteractionEnd();
		IEGInteractableInterface::Execute_FocusStateChanged(PreviousActor, false, LastHitResult);
	}

	FocusedActor = nullptr;
	FocusedPresentation = FEGInteractionPresentation();
	LastHitResult = FHitResult();
	LastBroadcastHoldProgress = 0.0f;

	OnFocusLost.Broadcast();
	if (PromptWidgetInstance)
	{
		PromptWidgetInstance->HidePrompt();
	}
}

void UEGInteractionComponent::NotifyFocusUpdated()
{
	AActor* FocusActor = FocusedActor.Get();
	if (!FocusActor)
	{
		return;
	}

	OnFocusChanged.Broadcast(FocusActor, FocusedPresentation.Text, FocusedPresentation.Icon);
	if (PromptWidgetInstance && !bPromptSuppressed)
	{
		PromptWidgetInstance->ShowPresentation(FocusedPresentation);
	}
}

float UEGInteractionComponent::GetHoldProgress() const
{
	const AActor* HoldActor = ActiveInteractionActor.IsValid() ? ActiveInteractionActor.Get() : FocusedActor.Get();
	const AEGInteractableActor* Interactable = Cast<AEGInteractableActor>(HoldActor);
	return Interactable ? Interactable->GetHoldProgress() : 0.0f;
}

void UEGInteractionComponent::UpdateHoldProgressFeedback()
{
	const float Progress = GetHoldProgress();
	if (FMath::IsNearlyEqual(Progress, LastBroadcastHoldProgress, KINDA_SMALL_NUMBER))
	{
		return;
	}

	LastBroadcastHoldProgress = Progress;
	OnHoldProgressChanged.Broadcast(Progress);
	if (PromptWidgetInstance && !bPromptSuppressed)
	{
		PromptWidgetInstance->SetHoldProgress(Progress);
	}
}

// =====================================================================
// Prompt widget
// =====================================================================

void UEGInteractionComponent::ShowPromptWidget()
{
	if (PromptWidgetInstance || bPromptSuppressed)
	{
		return;
	}

	CreatePromptWidget();
	if (PromptWidgetInstance)
	{
		PromptWidgetInstance->SetCursorFollowMode(bUseMouseTrace);
		PromptWidgetInstance->HidePrompt();
	}
}

void UEGInteractionComponent::HidePromptWidget()
{
	if (PromptWidgetInstance)
	{
		DestroyPromptWidget();
		PromptWidgetInstance = nullptr;
	}
}

void UEGInteractionComponent::CreatePromptWidget()
{
	if (!PromptWidgetClass)
	{
		return;
	}

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	APlayerController* PlayerController = OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;

	// Local-player UI. On a listen server this component also exists for remote pawns, and
	// creating their prompt here would put it on the host's screen.
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	PromptWidgetInstance = CreateWidget<UEGInteractionPromptWidget>(PlayerController, PromptWidgetClass);
	if (PromptWidgetInstance)
	{
		PromptWidgetInstance->AddToViewport();
	}
}

void UEGInteractionComponent::DestroyPromptWidget()
{
	if (PromptWidgetInstance)
	{
		PromptWidgetInstance->RemoveFromParent();
	}
}

void UEGInteractionComponent::SetPromptSuppressed(bool bSuppress)
{
	if (bPromptSuppressed == bSuppress)
	{
		return;
	}

	bPromptSuppressed = bSuppress;
	if (bPromptSuppressed)
	{
		HidePromptWidget();
	}
	else
	{
		ShowPromptWidget();
		NotifyFocusUpdated();
	}
}

// =====================================================================
// Debug
// =====================================================================

void UEGInteractionComponent::DrawDebugInfo(const FVector& Start, const FVector& End, bool bHitValid, bool bHitBlocked) const
{
#if ENABLE_DRAW_DEBUG
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float Lifetime = FMath::Max(TraceTickInterval, World->GetDeltaSeconds());

	if (bDebugDraw)
	{
		const FColor TraceColor = bHitValid ? DebugTraceHitColor : (bHitBlocked ? DebugTraceBlockedColor : DebugTraceMissColor);
		DrawDebugLine(World, Start, End, TraceColor, false, Lifetime, 0, 1.0f);
		if (TraceMode == EEGInteractionTraceMode::SphereSweep)
		{
			DrawDebugSphere(World, End, TraceRadius, 12, TraceColor, false, Lifetime);
		}
		if (bDebugShowInteractionRadius && GetOwner())
		{
			DrawDebugSphere(World, GetOwner()->GetActorLocation(), InteractionRange, 24, FColor::Cyan, false, Lifetime, 0, 0.5f);
		}
	}

	if (!bDebugDrawFocusInfo)
	{
		return;
	}

	const AActor* Target = FocusedActor.Get();
	const bool bIsServer = GetOwner() && GetOwner()->HasAuthority();
	FString ScreenText = Target
		? FString::Printf(TEXT("%s Interaction: %s (%.0fcm)"), bIsServer ? TEXT("[SERVER]") : TEXT("[CLIENT]"), *Target->GetName(), FVector::Dist(Start, Target->GetActorLocation()))
		: FString::Printf(TEXT("%s Interaction: no focus"), bIsServer ? TEXT("[SERVER]") : TEXT("[CLIENT]"));

	if (!IsInteractionAllowed())
	{
		ScreenText += TEXT(" [BLOCKED]");
	}
	const float Progress = GetHoldProgress();
	if (Progress > 0.0f)
	{
		ScreenText += FString::Printf(TEXT(" [HOLD %.0f%%]"), Progress * 100.0f);
	}

	if (Target)
	{
		DrawDebugString(World, Target->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f),
			FString::Printf(TEXT("[FOCUS] %s\nPrompt: %s"), *Target->GetName(), *FocusedPresentation.Text.ToString()),
			nullptr, FColor::White, Lifetime, true, 1.0f);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), Lifetime, FColor::White, ScreenText);
	}
#endif // ENABLE_DRAW_DEBUG
}
