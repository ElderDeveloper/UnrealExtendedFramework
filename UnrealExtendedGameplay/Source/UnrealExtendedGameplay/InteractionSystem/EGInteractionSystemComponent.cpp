// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGInteractionSystemComponent.h"

#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "EGInteraction.h"
#include "EGInteractionProviderComponent.h"
#include "EGInteractionSystemInterface.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"

DEFINE_LOG_CATEGORY_STATIC(LogEGInteractionSystem, Log, All);

UEGInteractionSystemComponent::UEGInteractionSystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(true);
}

UEGInteractionSystemComponent* UEGInteractionSystemComponent::FindInteractionSystem(const AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	if (const IEGInteractionSystemInterface* Interface = Cast<IEGInteractionSystemInterface>(Actor))
	{
		return Interface->GetInteractionSystemComponent();
	}

	return Actor->FindComponentByClass<UEGInteractionSystemComponent>();
}

// =====================================================================
// Component lifetime
// =====================================================================

void UEGInteractionSystemComponent::BeginPlay()
{
	Super::BeginPlay();

	// Who controls the pawn decides whether this component traces, and on a client that
	// answer arrives after BeginPlay through OnRep_Controller. Re-run it on every change.
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		OwnerPawn->ReceiveControllerChangedDelegate.AddUniqueDynamic(this, &UEGInteractionSystemComponent::HandleOwnerControllerChanged);
	}

	RefreshOwnerControlState();
}

void UEGInteractionSystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		OwnerPawn->ReceiveControllerChangedDelegate.RemoveDynamic(this, &UEGInteractionSystemComponent::HandleOwnerControllerChanged);
	}

	CancelAllInteractions();
	ClearFocus();
	ClearInputBindings();
	Super::EndPlay(EndPlayReason);
}

void UEGInteractionSystemComponent::HandleOwnerControllerChanged(APawn* /*Pawn*/, AController* /*OldController*/, AController* /*NewController*/)
{
	RefreshOwnerControlState();
}

void UEGInteractionSystemComponent::RefreshOwnerControlState()
{
	const bool bLocal = IsLocallyControlled();
	bWantsLocalTick = bLocal;
	UpdateShouldTick();

	if (bLocal)
	{
		// The input component may not exist yet when possession lands on a client; the tick
		// retries the bind until it does.
		EnsureInputBindings();
		return;
	}

	ClearFocus();
	ClearInputBindings();
}

bool UEGInteractionSystemComponent::HasAuthority() const
{
	return GetOwner() && GetOwner()->HasAuthority();
}

bool UEGInteractionSystemComponent::IsLocallyControlled() const
{
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		return OwnerPawn->IsLocallyControlled();
	}
	return GetOwner() && GetOwner()->HasLocalNetOwner();
}

bool UEGInteractionSystemComponent::GetShouldTick() const
{
	// The base ticks only while a task is ticking. The local owner also traces.
	return bWantsLocalTick || Super::GetShouldTick();
}

void UEGInteractionSystemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// Ticks every running task on both sides.
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bWantsLocalTick)
	{
		return;
	}

	EnsureInputBindings();

	if (!IsInteractionAllowed())
	{
		CancelAllInteractions();
		ClearFocus();
		return;
	}

	TraceAccumulator += DeltaTime;
	if (TraceTickInterval > 0.0f && TraceAccumulator < TraceTickInterval)
	{
		return;
	}
	TraceAccumulator = 0.0f;

	PerformTrace();
	RebuildPrompts();
}

// =====================================================================
// Input
// =====================================================================

void UEGInteractionSystemComponent::RefreshInputBindings()
{
	ClearInputBindings();
	EnsureInputBindings();
}

void UEGInteractionSystemComponent::ClearInputBindings()
{
	if (UEnhancedInputComponent* EnhancedInput = BoundInputComponent.Get())
	{
		for (const uint32 Handle : InputBindingHandles)
		{
			EnhancedInput->RemoveBindingByHandle(Handle);
		}
	}

	BoundInputComponent.Reset();
	InputBindingHandles.Reset();
	BoundSlots.Reset();
}

void UEGInteractionSystemComponent::EnsureInputBindings()
{
	if (!bAutoBindInput || InputSlots.Num() == 0)
	{
		if (BoundInputComponent.IsValid())
		{
			ClearInputBindings();
		}
		return;
	}

	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	UEnhancedInputComponent* EnhancedInput = OwnerPawn ? Cast<UEnhancedInputComponent>(OwnerPawn->InputComponent) : nullptr;
	if (!EnhancedInput)
	{
		if (BoundInputComponent.IsValid())
		{
			ClearInputBindings();
		}
		return;
	}

	// Rebind when the component changed or any slot's action differs from what was bound.
	bool bUpToDate = BoundInputComponent.Get() == EnhancedInput && BoundSlots.Num() == InputSlots.Num();
	for (int32 Index = 0; bUpToDate && Index < BoundSlots.Num(); ++Index)
	{
		const TObjectPtr<UInputAction>* Current = InputSlots.Find(BoundSlots[Index].Key);
		bUpToDate = Current && Current->Get() == BoundSlots[Index].Value.Get();
	}
	if (bUpToDate)
	{
		return;
	}

	ClearInputBindings();
	BoundInputComponent = EnhancedInput;

	for (const TPair<FGameplayTag, TObjectPtr<UInputAction>>& Slot : InputSlots)
	{
		BoundSlots.Emplace(Slot.Key, Slot.Value.Get());
		if (!Slot.Key.IsValid() || !Slot.Value)
		{
			continue;
		}

		InputBindingHandles.Add(EnhancedInput->BindAction(Slot.Value, ETriggerEvent::Started, this, &UEGInteractionSystemComponent::HandleSlotStarted, Slot.Key).GetHandle());
		InputBindingHandles.Add(EnhancedInput->BindAction(Slot.Value, ETriggerEvent::Completed, this, &UEGInteractionSystemComponent::HandleSlotReleased, Slot.Key).GetHandle());
		InputBindingHandles.Add(EnhancedInput->BindAction(Slot.Value, ETriggerEvent::Canceled, this, &UEGInteractionSystemComponent::HandleSlotReleased, Slot.Key).GetHandle());
	}
}

void UEGInteractionSystemComponent::HandleSlotStarted(FGameplayTag InputTag)
{
	TryActivateSlot(InputTag);
}

void UEGInteractionSystemComponent::HandleSlotReleased(FGameplayTag InputTag)
{
	ReleaseSlot(InputTag);
}

// =====================================================================
// Gating
// =====================================================================

void UEGInteractionSystemComponent::SetInteractionBlocked(bool bBlocked)
{
	if (bInteractionBlocked == bBlocked)
	{
		return;
	}

	bInteractionBlocked = bBlocked;
	UE_CLOG(bDebugLog, LogEGInteractionSystem, Log, TEXT("[%s] Interaction %s"), *GetNameSafe(GetOwner()), bBlocked ? TEXT("BLOCKED") : TEXT("UNBLOCKED"));

	if (!IsInteractionAllowed())
	{
		CancelAllInteractions();
		ClearFocus();
	}
}

bool UEGInteractionSystemComponent::IsInteractionAllowed() const
{
	return !bInteractionBlocked && CanOwnerInteract();
}

// =====================================================================
// Actions
// =====================================================================

bool UEGInteractionSystemComponent::TryActivateSlot(FGameplayTag InputTag)
{
	return ActivateInstance(FindGrantedForSlot(InputTag));
}

void UEGInteractionSystemComponent::ReleaseSlot(FGameplayTag InputTag)
{
	const bool bAuthority = HasAuthority();

	// Active instances, granted or lingering after focus moved on.
	TArray<TObjectPtr<UEGInteraction>> ActiveCopy = Active;
	for (UEGInteraction* Interaction : ActiveCopy)
	{
		if (!Interaction || Interaction->GetActivationInputTag() != InputTag || !Interaction->IsActive())
		{
			continue;
		}

		Interaction->HandleInputReleased();
		if (!bAuthority && Interaction->NetPolicy != EEGInteractionNetPolicy::LocalOnly)
		{
			ServerInputReleased(Interaction->GetActivationId());
		}
	}

	// ServerOnly requests still waiting on the server.
	for (UEGInteraction* Interaction : Granted)
	{
		if (Interaction && Interaction->IsServerPending() && Interaction->GetActivationInputTag() == InputTag && !bAuthority)
		{
			ServerInputReleased(Interaction->GetActivationId());
		}
	}
}

bool UEGInteractionSystemComponent::TryActivateById(FGameplayTag Id)
{
	return ActivateInstance(FindGrantedInteraction(Id));
}

void UEGInteractionSystemComponent::CancelAllInteractions()
{
	TArray<TObjectPtr<UEGInteraction>> ActiveCopy = Active;
	for (UEGInteraction* Interaction : ActiveCopy)
	{
		if (Interaction)
		{
			Interaction->CancelInteraction();
		}
	}

	for (UEGInteraction* Interaction : Granted)
	{
		if (Interaction && Interaction->IsServerPending())
		{
			if (!HasAuthority())
			{
				ServerCancel(Interaction->GetActivationId());
			}
			Interaction->HandleRevoked();
		}
	}
}

void UEGInteractionSystemComponent::RefreshFocus()
{
	if (!bWantsLocalTick)
	{
		return;
	}

	if (!IsInteractionAllowed())
	{
		ClearFocus();
		return;
	}

	PerformTrace();
	RebuildPrompts();
}

void UEGInteractionSystemComponent::RefreshPrompts()
{
	RebuildPrompts();
}

bool UEGInteractionSystemComponent::ActivateInstance(UEGInteraction* Interaction)
{
	if (!Interaction || Interaction->IsActive() || Interaction->IsServerPending())
	{
		return false;
	}

	if (!IsInteractionAllowed() || !Interaction->CanActivate())
	{
		return false;
	}

	const bool bAuthority = HasAuthority();
	const int32 ActivationId = NextActivationId++;

	switch (Interaction->NetPolicy)
	{
	case EEGInteractionNetPolicy::LocalOnly:
		Interaction->BeginActivation(ActivationId, false);
		return true;

	case EEGInteractionNetPolicy::LocalPredicted:
		if (bAuthority)
		{
			// Listen host or standalone: the granted instance is the authoritative one.
			Interaction->BeginActivation(ActivationId, false);
			return true;
		}
		Interaction->BeginActivation(ActivationId, true);
		ServerActivate(Interaction->GetProvider(), Interaction->GetInteractionId(), Interaction->GetHitResult(), Interaction->GetHitComponent(), ActivationId);
		return true;

	case EEGInteractionNetPolicy::ServerOnly:
		if (bAuthority)
		{
			Interaction->BeginActivation(ActivationId, false);
			return true;
		}
		Interaction->BeginServerPending(ActivationId);
		ServerActivate(Interaction->GetProvider(), Interaction->GetInteractionId(), Interaction->GetHitResult(), Interaction->GetHitComponent(), ActivationId);
		return true;
	}

	return false;
}

UEGInteraction* UEGInteractionSystemComponent::FindGrantedForSlot(FGameplayTag InputTag) const
{
	UEGInteraction* Best = nullptr;
	int32 BestPriority = TNumericLimits<int32>::Lowest();

	for (UEGInteraction* Interaction : Granted)
	{
		if (!Interaction || Interaction->GetInputTag() != InputTag || Interaction->IsActive() || Interaction->IsServerPending())
		{
			continue;
		}

		const FEGInteractionDefinition* Definition = Interaction->GetSpec().GetBase();
		const int32 Priority = Definition ? Definition->Priority : 0;
		if (Priority <= BestPriority && Best)
		{
			continue;
		}

		if (!Interaction->CanActivate())
		{
			continue;
		}

		Best = Interaction;
		BestPriority = Priority;
	}

	return Best;
}

UEGInteraction* UEGInteractionSystemComponent::FindGrantedInteraction(FGameplayTag Id) const
{
	for (UEGInteraction* Interaction : Granted)
	{
		if (Interaction && Interaction->GetInteractionId() == Id)
		{
			return Interaction;
		}
	}
	return nullptr;
}

UEGInteraction* UEGInteractionSystemComponent::FindByActivationId(int32 ActivationId) const
{
	for (UEGInteraction* Interaction : Active)
	{
		if (Interaction && Interaction->GetActivationId() == ActivationId)
		{
			return Interaction;
		}
	}
	for (UEGInteraction* Interaction : Granted)
	{
		if (Interaction && Interaction->GetActivationId() == ActivationId)
		{
			return Interaction;
		}
	}
	return nullptr;
}

// =====================================================================
// Called by UEGInteraction
// =====================================================================

void UEGInteractionSystemComponent::NotifyInteractionStarted(UEGInteraction* Interaction)
{
	if (!Interaction)
	{
		return;
	}

	Active.AddUnique(Interaction);
	UE_CLOG(bDebugLog, LogEGInteractionSystem, Log, TEXT("[%s] %s started %s on %s"), *GetNameSafe(GetOwner()), HasAuthority() ? TEXT("[SERVER]") : TEXT("[CLIENT]"), *Interaction->GetInteractionId().ToString(), *GetNameSafe(Interaction->GetTargetActor()));
	OnInteractionStarted.Broadcast(Interaction);
}

void UEGInteractionSystemComponent::NotifyInteractionEnded(UEGInteraction* Interaction, bool bCancelled, bool bNotifyServer)
{
	if (!Interaction)
	{
		return;
	}

	Active.Remove(Interaction);
	if (PromptProgress.Contains(Interaction->GetSpec().Handle))
	{
		SetPromptProgress(Interaction->GetSpec().Handle, 0.0f);
	}

	UE_CLOG(bDebugLog, LogEGInteractionSystem, Log, TEXT("[%s] %s ended %s (%s)"), *GetNameSafe(GetOwner()), HasAuthority() ? TEXT("[SERVER]") : TEXT("[CLIENT]"), *Interaction->GetInteractionId().ToString(), bCancelled ? TEXT("cancelled") : TEXT("completed"));
	OnInteractionEnded.Broadcast(Interaction, bCancelled);

	if (HasAuthority())
	{
		if (Interaction->NetPolicy != EEGInteractionNetPolicy::LocalOnly && !IsLocallyControlled())
		{
			ClientInteractionEnded(Interaction->GetActivationId(), bCancelled);
		}
	}
	else if (bNotifyServer && bCancelled && Interaction->NetPolicy != EEGInteractionNetPolicy::LocalOnly)
	{
		ServerCancel(Interaction->GetActivationId());
	}

	if (bWantsLocalTick)
	{
		RebuildPrompts();
	}
}

void UEGInteractionSystemComponent::NotifyActivationRejected(UEGInteraction* Interaction)
{
	UE_CLOG(bDebugLog, LogEGInteractionSystem, Warning, TEXT("[%s] Server rejected %s"), *GetNameSafe(GetOwner()), Interaction ? *Interaction->GetInteractionId().ToString() : TEXT("?"));
	OnActivationRejected.Broadcast(Interaction);
}

void UEGInteractionSystemComponent::SetPromptProgress(int32 Handle, float Progress)
{
	if (Progress <= 0.0f)
	{
		if (PromptProgress.Remove(Handle) == 0)
		{
			return;
		}
	}
	else
	{
		float& Stored = PromptProgress.FindOrAdd(Handle);
		if (FMath::IsNearlyEqual(Stored, Progress, KINDA_SMALL_NUMBER))
		{
			return;
		}
		Stored = Progress;
	}

	for (FEGInteractionPrompt& Prompt : Prompts)
	{
		if (Prompt.Handle == Handle)
		{
			Prompt.Progress = Progress;
			break;
		}
	}

	OnPromptProgressChanged.Broadcast(Handle, Progress);
}

// =====================================================================
// Authority
// =====================================================================

void UEGInteractionSystemComponent::ServerActivate_Implementation(UEGInteractionProviderComponent* Provider, FGameplayTag Id, FHitResult Hit, UPrimitiveComponent* HitComponent, int32 ActivationId)
{
	UEGInteraction* Interaction = PrepareServerActivation(Provider, Id, Hit, HitComponent, ActivationId);

	// The result goes out before the instance runs: an instant interaction ends inside
	// BeginActivation, and its end notification must not overtake the acceptance.
	ClientActivationResult(ActivationId, Interaction != nullptr);

	if (Interaction)
	{
		Interaction->BeginActivation(ActivationId, false);
	}
}

void UEGInteractionSystemComponent::ServerInputReleased_Implementation(int32 ActivationId)
{
	if (UEGInteraction* Interaction = FindByActivationId(ActivationId))
	{
		Interaction->HandleInputReleased();
	}
}

void UEGInteractionSystemComponent::ServerCancel_Implementation(int32 ActivationId)
{
	if (UEGInteraction* Interaction = FindByActivationId(ActivationId))
	{
		Interaction->CancelInteraction();
	}
}

void UEGInteractionSystemComponent::ClientActivationResult_Implementation(int32 ActivationId, bool bAccepted)
{
	if (UEGInteraction* Interaction = FindByActivationId(ActivationId))
	{
		Interaction->HandleServerResult(bAccepted);
	}
}

void UEGInteractionSystemComponent::ClientInteractionEnded_Implementation(int32 ActivationId, bool bCancelled)
{
	if (UEGInteraction* Interaction = FindByActivationId(ActivationId))
	{
		Interaction->HandleServerEnded(bCancelled);
	}
}

UEGInteraction* UEGInteractionSystemComponent::PrepareServerActivation(UEGInteractionProviderComponent* Provider, FGameplayTag Id, const FHitResult& ClientHit, UPrimitiveComponent* ClientHitComponent, int32 ActivationId)
{
	if (!HasAuthority())
	{
		return nullptr;
	}

	if (FindByActivationId(ActivationId))
	{
		UE_CLOG(bDebugLog, LogEGInteractionSystem, Warning, TEXT("[%s] Rejected: activation id %d is already in use"), *GetNameSafe(GetOwner()), ActivationId);
		return nullptr;
	}

	FHitResult ServerHit;
	FInstancedStruct Definition;
	if (!ValidateServerRequest(Provider, Id, ClientHit, ClientHitComponent, ServerHit, Definition))
	{
		return nullptr;
	}

	// Server-side instances are transient: created per request, dropped when they end. They
	// carry the server's own hit, never the client's.
	UEGInteraction* Interaction = CreateInstance(Provider, Definition, ServerHit, ServerHit.GetComponent());
	if (!Interaction)
	{
		return nullptr;
	}

	if (!Interaction->CanActivate())
	{
		UE_CLOG(bDebugLog, LogEGInteractionSystem, Warning, TEXT("[%s] Rejected: %s refused via CanActivate"), *GetNameSafe(GetOwner()), *Id.ToString());
		return nullptr;
	}

	return Interaction;
}

bool UEGInteractionSystemComponent::ValidateServerRequest(UEGInteractionProviderComponent* Provider, FGameplayTag Id, const FHitResult& ClientHit, UPrimitiveComponent* ClientHitComponent, FHitResult& OutServerHit, FInstancedStruct& OutDefinition) const
{
	if (!IsValid(Provider) || !IsValid(Provider->GetOwner()) || !Id.IsValid())
	{
		UE_CLOG(bDebugLog, LogEGInteractionSystem, Warning, TEXT("[%s] Rejected: no provider"), *GetNameSafe(GetOwner()));
		return false;
	}

	if (!IsInteractionAllowed())
	{
		UE_CLOG(bDebugLog, LogEGInteractionSystem, Warning, TEXT("[%s] Rejected: interaction is gated"), *GetNameSafe(GetOwner()));
		return false;
	}

	AActor* Target = Provider->GetOwner();
	if (ClientHit.GetActor() && ClientHit.GetActor() != Target)
	{
		UE_CLOG(bDebugLog, LogEGInteractionSystem, Warning, TEXT("[%s] Rejected: hit actor is not the provider's owner"), *GetNameSafe(GetOwner()));
		return false;
	}

	if (ClientHitComponent && ClientHitComponent->GetOwner() != Target)
	{
		UE_CLOG(bDebugLog, LogEGInteractionSystem, Warning, TEXT("[%s] Rejected: hit component belongs to another actor"), *GetNameSafe(GetOwner()));
		return false;
	}

	// Range is measured against the target's own collision, never the point the client named.
	const FVector OwnerLocation = GetOwner()->GetActorLocation();
	FVector ClosestPoint = Target->GetActorLocation();
	float DistanceToTarget = ClientHitComponent ? ClientHitComponent->GetClosestPointOnCollision(OwnerLocation, ClosestPoint) : -1.0f;
	if (DistanceToTarget < 0.0f)
	{
		DistanceToTarget = Target->ActorGetDistanceToCollision(OwnerLocation, TraceChannel, ClosestPoint);
	}
	if (DistanceToTarget < 0.0f)
	{
		ClosestPoint = Target->GetComponentsBoundingBox(true).GetClosestPointTo(OwnerLocation);
		DistanceToTarget = FVector::Dist(OwnerLocation, ClosestPoint);
	}

	const float ValidationDistance = InteractionRange + TraceRadius + ValidationSlack;
	if (DistanceToTarget > ValidationDistance)
	{
		UE_CLOG(bDebugLog, LogEGInteractionSystem, Warning, TEXT("[%s] Rejected: %s is out of range (%.0f > %.0f)"), *GetNameSafe(GetOwner()), *GetNameSafe(Target), DistanceToTarget, ValidationDistance);
		return false;
	}

	// The client's point only says where to look. The server traces there itself, and the first
	// thing its line meets has to be the target: a fabricated point in open air hits nothing.
	FVector ViewLocation;
	FRotator ViewRotation;
	GetViewPoint(ViewLocation, ViewRotation);
	const FVector AimPoint = ClientHit.GetActor() ? FVector(ClientHit.ImpactPoint) : ClosestPoint;

	if (AimPoint.ContainsNaN() || ViewLocation.ContainsNaN())
	{
		return false;
	}
	const FVector Direction = (AimPoint - ViewLocation).GetSafeNormal();
	const FVector ViewDirection = ViewRotation.Vector();
	const double ForwardDistance = FVector::DotProduct(AimPoint - ViewLocation, ViewDirection);
	const float SweepRadius = TraceMode == EEGInteractionSystemTraceMode::SphereSweep ? FMath::Max(0.0f, TraceRadius) : 0.0f;
	// Allow the sweep width plus bounded camera replication delay.
	const double LateralAllowance = SweepRadius + FMath::Max(0.0, ForwardDistance)
		* FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(ValidationAimTolerance, 0.0f, 45.0f)));
	if (Direction.IsNearlyZero() || ForwardDistance <= 0.0
		|| (AimPoint - ViewLocation - ViewDirection * ForwardDistance).SizeSquared() > FMath::Square(LateralAllowance))
	{
		return false;
	}

	FCollisionQueryParams Params;
	BuildQueryParams(Params);
	const FVector TraceEnd = ViewLocation + Direction * ValidationDistance;
	OutServerHit = FHitResult();
	if (bRequiresLineOfSight)
	{
		const UWorld* World = GetWorld();
		if (!World)
		{
			return false;
		}

		FHitResult TraceHit;
		const bool bHit = World->LineTraceSingleByChannel(TraceHit, ViewLocation, TraceEnd, TraceChannel, Params);
		if (!bHit || TraceHit.GetActor() != Target)
		{
			UE_CLOG(bDebugLog, LogEGInteractionSystem, Warning, TEXT("[%s] Rejected: %s is occluded or not where the client said"), *GetNameSafe(GetOwner()), *GetNameSafe(Target));
			return false;
		}
		OutServerHit = TraceHit;
	}
	else
	{
		// Occlusion is optional; intersecting server-owned geometry is mandatory.
		// Resolve the nearest target part instead of trusting ClientHitComponent.
		TInlineComponentArray<UPrimitiveComponent*> Primitives;
		Target->GetComponents(Primitives);
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			if (!IsValid(Primitive) || !Primitive->IsQueryCollisionEnabled()
				|| Primitive->GetCollisionResponseToChannel(TraceChannel) == ECR_Ignore)
			{
				continue;
			}
			FHitResult PartHit;
			if (Primitive->LineTraceComponent(PartHit, ViewLocation, TraceEnd, Params)
				&& (!OutServerHit.GetComponent() || PartHit.Time < OutServerHit.Time))
			{
				OutServerHit = PartHit;
			}
		}
	}

	if (!OutServerHit.GetComponent() || OutServerHit.GetActor() != Target
		|| OutServerHit.ImpactPoint.ContainsNaN()
		|| FVector::DistSquared(OwnerLocation, OutServerHit.ImpactPoint) > FMath::Square(ValidationDistance))
	{
		return false;
	}

	FEGInteractionQuery Query;
	Query.Interactor = GetOwner();
	Query.System = const_cast<UEGInteractionSystemComponent*>(this);
	Query.Hit = OutServerHit;
	Query.HitComponent = OutServerHit.GetComponent();
	Query.bServer = true;

	TArray<FInstancedStruct> Definitions;
	Provider->GatherInteractions(Query, Definitions);
	for (const FInstancedStruct& Entry : Definitions)
	{
		const FEGInteractionDefinition* Definition = Entry.GetPtr<FEGInteractionDefinition>();
		if (!Definition || Definition->Id != Id || !Definition->InteractionClass)
		{
			continue;
		}

		// A LocalOnly class never runs on the server; a request for one is malformed.
		const UEGInteraction* DefaultInteraction = Definition->InteractionClass->GetDefaultObject<UEGInteraction>();
		if (!DefaultInteraction || DefaultInteraction->NetPolicy == EEGInteractionNetPolicy::LocalOnly)
		{
			UE_CLOG(bDebugLog, LogEGInteractionSystem, Warning, TEXT("[%s] Rejected: %s is local-only"), *GetNameSafe(GetOwner()), *Id.ToString());
			return false;
		}

		OutDefinition = Entry;
		return true;
	}

	UE_CLOG(bDebugLog, LogEGInteractionSystem, Warning, TEXT("[%s] Rejected: %s does not offer %s for this hit"), *GetNameSafe(GetOwner()), *GetNameSafe(Target), *Id.ToString());
	return false;
}

// =====================================================================
// Trace and focus
// =====================================================================

void UEGInteractionSystemComponent::GetViewPoint(FVector& OutLocation, FRotator& OutRotation) const
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

void UEGInteractionSystemComponent::BuildQueryParams(FCollisionQueryParams& OutParams) const
{
	OutParams = FCollisionQueryParams(SCENE_QUERY_STAT(EGInteractionSystemTrace), bTraceComplex, GetOwner());
	if (GetOwner() && GetOwner()->GetInstigator())
	{
		OutParams.AddIgnoredActor(GetOwner()->GetInstigator());
	}
	for (const AActor* Ignored : AdditionalIgnoredActors)
	{
		if (Ignored)
		{
			OutParams.AddIgnoredActor(Ignored);
		}
	}
}

bool UEGInteractionSystemComponent::HasLineOfSightToPoint(const FVector& From, const AActor* Target, const FVector& Point) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams Params;
	BuildQueryParams(Params);
	if (Target)
	{
		Params.AddIgnoredActor(Target);
	}
	return !World->LineTraceTestByChannel(From, Point, TraceChannel, Params);
}

bool UEGInteractionSystemComponent::HasLineOfSight(const FVector& ViewLocation, const FHitResult& Hit) const
{
	const AActor* Target = Hit.GetActor();
	if (!Target)
	{
		return false;
	}

	// The impact point, not the actor origin: an origin inside the mesh blocks its own trace.
	const FVector TargetPoint = Hit.ImpactPoint.IsNearlyZero() ? Target->GetActorLocation() : FVector(Hit.ImpactPoint);
	return HasLineOfSightToPoint(ViewLocation, Target, TargetPoint);
}

void UEGInteractionSystemComponent::PerformTrace()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	GetViewPoint(ViewLocation, ViewRotation);
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * InteractionRange;

	FCollisionQueryParams Params;
	BuildQueryParams(Params);

	UEGInteractionProviderComponent* Provider = nullptr;
	FHitResult BestHit;

	// The precise line goes first: a sphere sweep alone cannot pick out a button on a panel.
	FHitResult LineHit;
	if (World->LineTraceSingleByChannel(LineHit, ViewLocation, TraceEnd, TraceChannel, Params))
	{
		Provider = UEGInteractionProviderComponent::FindProvider(LineHit.GetActor());
		BestHit = LineHit;
	}

	if (!Provider && TraceMode == EEGInteractionSystemTraceMode::SphereSweep && TraceRadius > 0.0f)
	{
		TArray<FHitResult> Hits;
		if (World->SweepMultiByChannel(Hits, ViewLocation, TraceEnd, FQuat::Identity, TraceChannel, FCollisionShape::MakeSphere(TraceRadius), Params))
		{
			// Sweep results arrive sorted by distance. The first provider that is not occluded wins.
			for (const FHitResult& Hit : Hits)
			{
				UEGInteractionProviderComponent* Candidate = UEGInteractionProviderComponent::FindProvider(Hit.GetActor());
				if (!Candidate)
				{
					continue;
				}
				if (bRequiresLineOfSight && !HasLineOfSight(ViewLocation, Hit))
				{
					continue;
				}
				Provider = Candidate;
				BestHit = Hit;
				break;
			}
		}
	}

	if (Provider)
	{
		SetFocus(Provider, BestHit);
	}
	else
	{
		ClearFocus();
	}

	DrawDebugInfo(ViewLocation, TraceEnd, Provider != nullptr);
}

void UEGInteractionSystemComponent::SetFocus(UEGInteractionProviderComponent* Provider, const FHitResult& Hit)
{
	if (!Provider)
	{
		ClearFocus();
		return;
	}

	UPrimitiveComponent* HitComponent = Hit.GetComponent();
	const bool bProviderChanged = FocusedProvider.Get() != Provider;
	const bool bPartChanged = FocusedComponent.Get() != HitComponent;
	FocusHit = Hit;

	if (!bProviderChanged && !bPartChanged)
	{
		for (UEGInteraction* Interaction : Granted)
		{
			if (Interaction)
			{
				Interaction->UpdateSpecHit(Hit, HitComponent);
			}
		}

		// Same target, same part, but its definitions moved under us.
		if (Provider->GetDefinitionsRevision() != GrantedRevision)
		{
			Regrant();
		}
		return;
	}

	if (UEGInteractionProviderComponent* Previous = FocusedProvider.Get())
	{
		if (bProviderChanged)
		{
			Previous->NotifyFocus(false, FocusedComponent.Get());
		}
	}

	FocusedProvider = Provider;
	FocusedComponent = HitComponent;

	Regrant();

	// The provider hears about every part change; it applies its outline only once.
	Provider->NotifyFocus(true, HitComponent);
	OnFocusChanged.Broadcast(Provider->GetOwner(), Provider);
}

void UEGInteractionSystemComponent::ClearFocus()
{
	const bool bHadFocus = FocusedProvider.IsValid() || FocusedProvider.IsStale() || Granted.Num() > 0;
	if (!bHadFocus)
	{
		return;
	}

	if (UEGInteractionProviderComponent* Previous = FocusedProvider.Get())
	{
		Previous->NotifyFocus(false, FocusedComponent.Get());
	}

	FocusedProvider.Reset();
	FocusedComponent.Reset();
	FocusHit = FHitResult();

	RevokeAll();
	RebuildPrompts();
	OnFocusChanged.Broadcast(nullptr, nullptr);
}

AActor* UEGInteractionSystemComponent::GetFocusedActor() const
{
	const UEGInteractionProviderComponent* Provider = FocusedProvider.Get();
	return Provider ? Provider->GetOwner() : nullptr;
}

// =====================================================================
// Grants
// =====================================================================

UEGInteraction* UEGInteractionSystemComponent::CreateInstance(UEGInteractionProviderComponent* Provider, const FInstancedStruct& Definition, const FHitResult& Hit, UPrimitiveComponent* HitComponent)
{
	const FEGInteractionDefinition* Base = Definition.GetPtr<FEGInteractionDefinition>();
	if (!Base || !Base->InteractionClass || !Provider)
	{
		return nullptr;
	}

	UEGInteraction* Interaction = NewObject<UEGInteraction>(this, Base->InteractionClass);
	if (!Interaction)
	{
		return nullptr;
	}

	FEGInteractionSpec Spec;
	Spec.Handle = NextHandle++;
	Spec.Id = Base->Id;
	Spec.Provider = Provider;
	Spec.Definition = Definition;
	Spec.Hit = Hit;
	Spec.HitComponent = HitComponent;
	Interaction->InitInteraction(this, Spec);
	return Interaction;
}

void UEGInteractionSystemComponent::Regrant()
{
	UEGInteractionProviderComponent* Provider = FocusedProvider.Get();
	if (!Provider)
	{
		RevokeAll();
		return;
	}

	FEGInteractionQuery Query;
	Query.Interactor = GetOwner();
	Query.System = this;
	Query.Hit = FocusHit;
	Query.HitComponent = FocusedComponent.Get();
	Query.bServer = false;

	TArray<FInstancedStruct> Definitions;
	Provider->GatherInteractions(Query, Definitions);
	GrantedRevision = Provider->GetDefinitionsRevision();

	TArray<TObjectPtr<UEGInteraction>> Previous = MoveTemp(Granted);
	Granted.Reset();

	const auto MatchesDefinition = [Provider](const FEGInteractionDefinition& Base)
	{
		return [Provider, &Base](const TObjectPtr<UEGInteraction>& Candidate)
		{
			return Candidate && Candidate->GetProvider() == Provider && Candidate->GetInteractionId() == Base.Id
				&& Candidate->GetClass() == Base.InteractionClass;
		};
	};

	for (const FInstancedStruct& Definition : Definitions)
	{
		const FEGInteractionDefinition* Base = Definition.GetPtr<FEGInteractionDefinition>();
		if (!Base || !Base->Id.IsValid() || !Base->InteractionClass)
		{
			continue;
		}

		// A grant that survived the refocus keeps its instance, and with it any running hold.
		UEGInteraction* Kept = nullptr;
		const int32 Existing = Previous.IndexOfByPredicate(MatchesDefinition(*Base));
		if (Existing != INDEX_NONE)
		{
			Kept = Previous[Existing];
			Previous.RemoveAt(Existing);
		}
		else
		{
			// An instance that outlived a focus loss is still running: re-adopt it rather than
			// granting a twin that could be activated alongside it.
			const int32 Lingering = Active.IndexOfByPredicate(MatchesDefinition(*Base));
			if (Lingering != INDEX_NONE)
			{
				Kept = Active[Lingering];
			}
		}

		if (Kept)
		{
			Kept->UpdateSpecHit(FocusHit, FocusedComponent.Get());
			if (!Kept->GetDefinitionStruct().Identical(&Definition, PPF_None))
			{
				if (Kept->IsActive() || Kept->IsServerPending())
				{
					// Running: it keeps its behaviour, only the parameters it will read next change.
					Kept->UpdateSpecDefinition(Definition);
				}
				else
				{
					// Idle: a fresh instance is cheaper than reasoning about half-applied edits.
					Revoke(Kept);
					Kept = CreateInstance(Provider, Definition, FocusHit, FocusedComponent.Get());
				}
			}
			if (Kept)
			{
				Granted.Add(Kept);
			}
			continue;
		}

		if (UEGInteraction* Created = CreateInstance(Provider, Definition, FocusHit, FocusedComponent.Get()))
		{
			Granted.Add(Created);
		}
	}

	for (UEGInteraction* Dropped : Previous)
	{
		Revoke(Dropped);
	}
}

void UEGInteractionSystemComponent::RevokeAll()
{
	TArray<TObjectPtr<UEGInteraction>> Previous = MoveTemp(Granted);
	Granted.Reset();
	for (UEGInteraction* Interaction : Previous)
	{
		Revoke(Interaction);
	}
}

void UEGInteractionSystemComponent::Revoke(UEGInteraction* Interaction)
{
	if (!Interaction)
	{
		return;
	}

	PromptProgress.Remove(Interaction->GetSpec().Handle);

	if (Interaction->IsActive() && !Interaction->bCancelOnFocusLost)
	{
		// Lingers in Active until it ends on its own.
		return;
	}

	if (Interaction->IsServerPending() && !HasAuthority())
	{
		ServerCancel(Interaction->GetActivationId());
	}

	Interaction->HandleRevoked();
}

// =====================================================================
// Prompts
// =====================================================================

void UEGInteractionSystemComponent::RebuildPrompts()
{
	TArray<FEGInteractionPrompt> NewPrompts;
	NewPrompts.Reserve(Granted.Num());

	for (UEGInteraction* Interaction : Granted)
	{
		if (!Interaction)
		{
			continue;
		}

		FEGInteractionPrompt Prompt = Interaction->GetPresentation();
		if (!Prompt.bVisible)
		{
			continue;
		}

		const FEGInteractionSpec& Spec = Interaction->GetSpec();
		const FEGInteractionDefinition* Definition = Spec.GetBase();
		Prompt.Handle = Spec.Handle;
		Prompt.Id = Spec.Id;
		Prompt.InputTag = Definition ? Definition->InputTag : FGameplayTag();
		Prompt.Priority = Definition ? Definition->Priority : 0;
		Prompt.Target = Interaction->GetTargetActor();
		Prompt.Progress = PromptProgress.FindRef(Spec.Handle);
		NewPrompts.Add(MoveTemp(Prompt));
	}

	NewPrompts.StableSort([](const FEGInteractionPrompt& A, const FEGInteractionPrompt& B) { return A.Priority > B.Priority; });

	bool bChanged = NewPrompts.Num() != Prompts.Num();
	for (int32 Index = 0; !bChanged && Index < NewPrompts.Num(); ++Index)
	{
		bChanged = !NewPrompts[Index].IsSameAs(Prompts[Index]);
	}

	Prompts = MoveTemp(NewPrompts);
	if (bChanged)
	{
		OnPromptsChanged.Broadcast(Prompts);
	}
}

// =====================================================================
// Debug
// =====================================================================

void UEGInteractionSystemComponent::DrawDebugInfo(const FVector& Start, const FVector& End, bool bFocused) const
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
		const FColor Color = bFocused ? FColor::Green : FColor::Red;
		DrawDebugLine(World, Start, End, Color, false, Lifetime, 0, 1.0f);
		if (TraceMode == EEGInteractionSystemTraceMode::SphereSweep && TraceRadius > 0.0f)
		{
			DrawDebugSphere(World, End, TraceRadius, 12, Color, false, Lifetime);
		}
	}

	if (!bDebugDrawFocusInfo || !GEngine)
	{
		return;
	}

	const bool bIsServer = HasAuthority();
	FString Text = FString::Printf(TEXT("%s Interaction: %s"), bIsServer ? TEXT("[SERVER]") : TEXT("[CLIENT]"), *GetNameSafe(GetFocusedActor()));
	if (const UPrimitiveComponent* Component = FocusedComponent.Get())
	{
		Text += FString::Printf(TEXT(" / %s"), *Component->GetName());
	}
	if (!IsInteractionAllowed())
	{
		Text += TEXT(" [BLOCKED]");
	}
	for (const FEGInteractionPrompt& Prompt : Prompts)
	{
		Text += FString::Printf(TEXT("\n  [%s] %s%s"), *Prompt.InputTag.ToString(), *Prompt.Text.ToString(), Prompt.bEnabled ? TEXT("") : TEXT(" (disabled)"));
		if (Prompt.Progress > 0.0f)
		{
			Text += FString::Printf(TEXT(" %.0f%%"), Prompt.Progress * 100.0f);
		}
	}

	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), Lifetime, FColor::White, Text);
#endif // ENABLE_DRAW_DEBUG
}
