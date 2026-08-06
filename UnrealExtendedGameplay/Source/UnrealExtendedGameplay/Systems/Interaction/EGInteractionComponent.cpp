// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGInteractionComponent.h"

#include "Blueprint/UserWidget.h"
#include "DrawDebugHelpers.h"
#include "EGInteractableActor.h"
#include "EGInteractableInterface.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagAssetInterface.h"
#include "UI/EGInteractionPromptWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogEGInteraction, Log, All);

namespace EGInteraction
{
	const FText NoTargetReason = NSLOCTEXT("EGInteraction", "NoTarget", "No interactable target");
	const FText RejectedReason = NSLOCTEXT("EGInteraction", "Rejected", "Interaction was rejected by the server");
}

UEGInteractionComponent::UEGInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UEGInteractionComponent::BeginPlay()
{
	Super::BeginPlay();


	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	const bool bLocallyControlled = OwnerPawn && OwnerPawn->IsLocallyControlled();
	SetComponentTickEnabled(bLocallyControlled || (GetOwner() && GetOwner()->HasAuthority()));

	if (bLocallyControlled)
	{
		ShowPromptWidget();
	}
}

void UEGInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	InteractionEnd();
	ClearFocus();
	HidePromptWidget();
	Super::EndPlay(EndPlayReason);
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
		OnInteractionFailed.Broadcast(nullptr, EGInteraction::NoTargetReason);
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
		OnInteractionFailed.Broadcast(TargetActor, EGInteraction::RejectedReason);
		return;
	}

	if (GetOwner() && GetOwner()->HasAuthority())
	{
		BeginInteractionOnServer(TargetActor, LastHitResult);
		return;
	}

	// The client tracks the target locally so hold UI can run before the server confirms.
	ActiveInteractionActor = TargetActor;
	ServerInteractionStart(TargetActor, LastHitResult);
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
	AActor* TargetActor = ActiveInteractionActor.Get();
	if (!TargetActor && !ActiveInteractionActor.IsStale())
	{
		return;
	}

	if (TargetActor && GetOwner() && GetOwner()->HasAuthority())
	{
		EndInteractionOnServer(TargetActor);
		return;
	}

	ActiveInteractionActor.Reset();

	if (!TargetActor)
	{
		return;
	}

	ServerInteractionEnd(TargetActor);
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

FHitResult UEGInteractionComponent::GetInteractionHit(const AActor* Interactor)
{
	const UEGInteractionComponent* Component = Interactor ? Interactor->FindComponentByClass<UEGInteractionComponent>() : nullptr;
	return Component ? Component->LastHitResult : FHitResult();
}

// =====================================================================
// Authority
// =====================================================================

void UEGInteractionComponent::ServerInteractionStart_Implementation(AActor* TargetActor, FHitResult ClientHitResult)
{
	const bool bSuccess = BeginInteractionOnServer(TargetActor, ClientHitResult);
	if (!bSuccess)
	{
		ClientInteractionResult(false, TargetActor, ClientHitResult, EGInteraction::RejectedReason);
	}
}

void UEGInteractionComponent::ServerInteractionEnd_Implementation(AActor* TargetActor)
{
	EndInteractionOnServer(TargetActor);
}

void UEGInteractionComponent::ClientInteractionResult_Implementation(bool bSuccess, AActor* TargetActor, FHitResult ConfirmedHitResult, const FText& FailureReason)
{
	if (bSuccess)
	{
		return;
	}

	// The server refused, so drop the client's optimistic hold bookkeeping.
	if (ActiveInteractionActor.Get() == TargetActor)
	{
		ActiveInteractionActor.Reset();
	}
	OnInteractionFailed.Broadcast(TargetActor, FailureReason);
}

bool UEGInteractionComponent::BeginInteractionOnServer(AActor* TargetActor, const FHitResult& HitResult)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || ActiveInteractionActor.IsValid())
	{
		return false;
	}

	if (!IsServerInteractionValid(TargetActor, HitResult))
	{
		return false;
	}

	// Publish the validated hit so the interactable can read it back with GetInteractionHit().
	LastHitResult = HitResult;
	ActiveInteractionActor = TargetActor;
	IEGInteractableInterface::Execute_InteractionStart(TargetActor, GetOwner());
	return true;
}

void UEGInteractionComponent::EndInteractionOnServer(AActor* TargetActor)
{
	AActor* ActiveActor = ActiveInteractionActor.Get();
	if (!ActiveActor || ActiveActor != TargetActor)
	{
		return;
	}

	IEGInteractableInterface::Execute_InteractionEnd(ActiveActor, GetOwner());
	ActiveInteractionActor.Reset();
}

bool UEGInteractionComponent::IsServerInteractionValid(AActor* TargetActor, const FHitResult& HitResult) const
{
	if (!TargetActor || !TargetActor->GetClass()->ImplementsInterface(UEGInteractableInterface::StaticClass()))
	{
		UE_CLOG(bDebugLogTraceResults, LogEGInteraction, Warning, TEXT("[%s] Rejected: target is not interactable"), *GetNameSafe(GetOwner()));
		return false;
	}

	if (!IsInteractionAllowed())
	{
		UE_CLOG(bDebugLogTraceResults, LogEGInteraction, Warning, TEXT("[%s] Rejected: interaction is gated"), *GetNameSafe(GetOwner()));
		return false;
	}

	if (HitResult.GetActor() && HitResult.GetActor() != TargetActor)
	{
		UE_CLOG(bDebugLogTraceResults, LogEGInteraction, Warning, TEXT("[%s] Rejected: hit actor does not match the requested target"), *GetNameSafe(GetOwner()));
		return false;
	}

	const float ValidationDistance = InteractionRange + TraceRadius + ValidationSlack;
	if (FVector::DistSquared(GetOwner()->GetActorLocation(), TargetActor->GetActorLocation()) > FMath::Square(ValidationDistance))
	{
		UE_CLOG(bDebugLogTraceResults, LogEGInteraction, Warning, TEXT("[%s] Rejected: %s is out of range"), *GetNameSafe(GetOwner()), *GetNameSafe(TargetActor));
		return false;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	GetViewPoint(ViewLocation, ViewRotation);
	if (!IsCandidateValid(TargetActor, HitResult, ViewLocation, true))
	{
		UE_CLOG(bDebugLogTraceResults, LogEGInteraction, Warning, TEXT("[%s] Rejected: %s failed facing/line-of-sight/tag filtering"), *GetNameSafe(GetOwner()), *GetNameSafe(TargetActor));
		return false;
	}

	if (!IEGInteractableInterface::Execute_CanInteract(TargetActor, GetOwner(), HitResult))
	{
		UE_CLOG(bDebugLogTraceResults, LogEGInteraction, Warning, TEXT("[%s] Rejected: %s refused via CanInteract"), *GetNameSafe(GetOwner()), *GetNameSafe(TargetActor));
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

bool UEGInteractionComponent::IsCandidateValid(AActor* Candidate, const FHitResult& HitResult, const FVector& ViewLocation, bool bServerValidation) const
{
	if (!Candidate || !Candidate->GetClass()->ImplementsInterface(UEGInteractableInterface::StaticClass()))
	{
		return false;
	}

	if (!PassesTagFilter(Candidate))
	{
		return false;
	}

	if (bMustBeFacing)
	{
		const FVector Direction = (Candidate->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal();
		if (FVector::DotProduct(GetOwner()->GetActorForwardVector(), Direction) < FMath::Cos(FMath::DegreesToRadians(FacingHalfAngle)))
		{
			return false;
		}
	}

	if (bRequiresLineOfSight && GetWorld())
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
	const bool bTextChanged = !FocusedPresentation.Text.EqualTo(NewPresentation.Text);
	const bool bIconChanged = FocusedPresentation.Icon != NewPresentation.Icon;

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
		PromptWidgetInstance->ShowPrompt(FocusedPresentation.Text, FocusedPresentation.Icon);
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
