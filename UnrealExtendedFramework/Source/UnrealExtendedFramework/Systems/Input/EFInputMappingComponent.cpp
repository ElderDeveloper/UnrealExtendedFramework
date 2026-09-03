// Fill out your copyright notice in the Description page of Project Settings.

#include "Systems/Input/EFInputMappingComponent.h"

#include "EnhancedInputSubsystems.h"
#include "Systems/Input/EFPlayerMappableKeySettings.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "TimerManager.h"

UEFInputMappingComponent::UEFInputMappingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = false;
}

void UEFInputMappingComponent::BeginPlay()
{
	Super::BeginPlay();

	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		OwnerPawn->ReceiveControllerChangedDelegate.AddUniqueDynamic(this, &UEFInputMappingComponent::HandleOwnerControllerChanged);
	}

	for (const FEFInputMappingSpec& MappingSpec : DefaultInputMappings)
	{
		AddManagedInputMapping(MappingSpec);
	}

	RefreshInputMappings();

	// The owning pawn is often possessed in the same frame this component begins
	// play, and on clients the controller arrives later still. One coalesced
	// retry covers the ordering without polling.
	ScheduleRefreshInputMappings();
}

void UEFInputMappingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		OwnerPawn->ReceiveControllerChangedDelegate.RemoveDynamic(this, &UEFInputMappingComponent::HandleOwnerControllerChanged);
	}

	InputMappingStack.Reset();
	RemoveAppliedInputMapping();
	ActiveInputMappingIndex = INDEX_NONE;

	Super::EndPlay(EndPlayReason);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Persistent entries
// ─────────────────────────────────────────────────────────────────────────────

bool UEFInputMappingComponent::AddManagedInputMapping(const FEFInputMappingSpec& MappingSpec)
{
	if (!MappingSpec.IsValid())
	{
		return false;
	}

	FEFInputMappingEntry* ExistingEntry = InputMappingStack.FindByPredicate(
		[&MappingSpec](const FEFInputMappingEntry& Entry)
		{
			return Entry.bManaged && Entry.Spec.MappingContext == MappingSpec.MappingContext;
		});

	if (ExistingEntry)
	{
		if (ExistingEntry->Spec == MappingSpec)
		{
			return true;
		}

		// Re-prioritizing also refreshes recency so it wins a tie against
		// whatever was pushed before it.
		ExistingEntry->Spec = MappingSpec;
		ExistingEntry->Handle.Id = NextInputMappingHandleId++;
	}
	else
	{
		FEFInputMappingEntry Entry;
		Entry.Spec = MappingSpec;
		Entry.Handle.Id = NextInputMappingHandleId++;
		Entry.bManaged = true;
		InputMappingStack.Add(MoveTemp(Entry));
	}

	RefreshInputMappings();
	return true;
}

bool UEFInputMappingComponent::RemoveManagedInputMapping(UInputMappingContext* MappingContext)
{
	if (!MappingContext)
	{
		return false;
	}

	const int32 RemovedCount = InputMappingStack.RemoveAll(
		[MappingContext](const FEFInputMappingEntry& Entry)
		{
			return Entry.bManaged && Entry.Spec.MappingContext == MappingContext;
		});

	if (RemovedCount == 0)
	{
		return false;
	}

	RefreshInputMappings();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Temporary entries
// ─────────────────────────────────────────────────────────────────────────────

bool UEFInputMappingComponent::ApplyInputMappingOverride(UObject* Source, const TArray<FEFInputMappingSpec>& MappingSpecs, FEFInputMappingHandle& OutHandle)
{
	OutHandle.Reset();

	if (!Source)
	{
		return false;
	}

	// One handle covers every spec in the set, so releasing it drops them
	// together even though at most one of them was ever applied.
	FEFInputMappingHandle Handle;
	Handle.Id = NextInputMappingHandleId++;

	int32 AddedCount = 0;
	for (const FEFInputMappingSpec& MappingSpec : MappingSpecs)
	{
		if (!MappingSpec.IsValid())
		{
			continue;
		}

		FEFInputMappingEntry Entry;
		Entry.Spec = MappingSpec;
		Entry.Handle = Handle;
		Entry.Source = Source;
		Entry.bManaged = false;
		InputMappingStack.Add(MoveTemp(Entry));
		++AddedCount;
	}

	if (AddedCount == 0)
	{
		return false;
	}

	OutHandle = Handle;
	RefreshInputMappings();
	return true;
}

bool UEFInputMappingComponent::ReleaseInputMappingOverride(FEFInputMappingHandle& Handle)
{
	if (!Handle.IsValid())
	{
		return false;
	}

	const int32 RemovedCount = InputMappingStack.RemoveAll(
		[&Handle](const FEFInputMappingEntry& Entry)
		{
			return !Entry.bManaged && Entry.Handle == Handle;
		});

	Handle.Reset();

	if (RemovedCount == 0)
	{
		return false;
	}

	RefreshInputMappings();
	return true;
}

int32 UEFInputMappingComponent::ReleaseInputMappingOverridesFromSource(UObject* Source)
{
	if (!Source)
	{
		return 0;
	}

	const int32 RemovedCount = InputMappingStack.RemoveAll(
		[Source](const FEFInputMappingEntry& Entry)
		{
			return !Entry.bManaged && Entry.Source.Get() == Source;
		});

	if (RemovedCount > 0)
	{
		RefreshInputMappings();
	}

	return RemovedCount;
}

void UEFInputMappingComponent::ClearInputMappings()
{
	if (InputMappingStack.IsEmpty())
	{
		return;
	}

	InputMappingStack.Reset();
	RefreshInputMappings();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Resolution
// ─────────────────────────────────────────────────────────────────────────────

int32 UEFInputMappingComponent::FindWinningEntryIndex() const
{
	int32 WinningIndex = INDEX_NONE;

	for (int32 EntryIndex = 0; EntryIndex < InputMappingStack.Num(); ++EntryIndex)
	{
		const FEFInputMappingEntry& Entry = InputMappingStack[EntryIndex];
		if (!Entry.Spec.IsValid())
		{
			continue;
		}

		if (WinningIndex == INDEX_NONE)
		{
			WinningIndex = EntryIndex;
			continue;
		}

		const FEFInputMappingEntry& WinningEntry = InputMappingStack[WinningIndex];
		const bool bHigherPriority = Entry.Spec.Priority > WinningEntry.Spec.Priority;
		const bool bMoreRecentAtSamePriority = Entry.Spec.Priority == WinningEntry.Spec.Priority
			&& Entry.Handle.Id > WinningEntry.Handle.Id;

		if (bHigherPriority || bMoreRecentAtSamePriority)
		{
			WinningIndex = EntryIndex;
		}
	}

	return WinningIndex;
}

void UEFInputMappingComponent::PruneStaleEntries()
{
	InputMappingStack.RemoveAll([](const FEFInputMappingEntry& Entry)
	{
		if (!Entry.Spec.IsValid())
		{
			return true;
		}
		// A managed entry has no source to outlive; an override whose pusher is
		// gone would otherwise hold the winner slot forever.
		return !Entry.bManaged && !Entry.Source.IsValid();
	});
}

void UEFInputMappingComponent::RefreshInputMappings()
{
	PruneStaleEntries();

	const int32 WinningIndex = FindWinningEntryIndex();
	UInputMappingContext* DesiredMappingContext = InputMappingStack.IsValidIndex(WinningIndex)
		? InputMappingStack[WinningIndex].Spec.MappingContext
		: nullptr;
	const int32 DesiredPriority = InputMappingStack.IsValidIndex(WinningIndex)
		? InputMappingStack[WinningIndex].Spec.Priority
		: 0;

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = GetEnhancedInputSubsystem();
	if (!InputSubsystem)
	{
		// No local input surface. Give back anything still applied to the local
		// player we were last attached to, but keep the list — possession may
		// bring a subsystem back and everything re-applies from it.
		RemoveAppliedInputMapping();
		ActiveInputMappingIndex = INDEX_NONE;
		return;
	}

	const bool bSubsystemChanged = AppliedInputSubsystem.Get() != InputSubsystem;
	const bool bAlreadyApplied = !bSubsystemChanged
		&& AppliedMappingContext == DesiredMappingContext
		&& AppliedPriority == DesiredPriority;

	if (bAlreadyApplied)
	{
		ActiveInputMappingIndex = WinningIndex;
		return;
	}

	UInputMappingContext* PreviousMappingContext = AppliedMappingContext;

	// Remove before adopting the new subsystem — RemoveAppliedInputMapping()
	// resolves through AppliedInputSubsystem, so reassigning first would strand
	// the old context on the old local player.
	RemoveAppliedInputMapping();

	if (DesiredMappingContext)
	{
		InputSubsystem->AddMappingContext(DesiredMappingContext, DesiredPriority);
		AppliedInputSubsystem = InputSubsystem;
		AppliedMappingContext = DesiredMappingContext;
		AppliedPriority = DesiredPriority;
	}

	ActiveInputMappingIndex = WinningIndex;

	if (PreviousMappingContext != DesiredMappingContext)
	{
		NotifyActiveInputMappingChanged(DesiredMappingContext, PreviousMappingContext);
		OnActiveInputMappingChanged.Broadcast(DesiredMappingContext, PreviousMappingContext);
	}
}

void UEFInputMappingComponent::RemoveAppliedInputMapping()
{
	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = AppliedInputSubsystem.Get())
	{
		if (AppliedMappingContext)
		{
			InputSubsystem->RemoveMappingContext(AppliedMappingContext);
		}
	}

	AppliedInputSubsystem.Reset();
	AppliedMappingContext = nullptr;
	AppliedPriority = 0;
}

void UEFInputMappingComponent::ScheduleRefreshInputMappings()
{
	if (bRefreshScheduled)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	bRefreshScheduled = true;
	World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		bRefreshScheduled = false;
		RefreshInputMappings();
	}));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Possession
// ─────────────────────────────────────────────────────────────────────────────

void UEFInputMappingComponent::HandleOwnerControllerChanged(APawn* /*Pawn*/, AController* OldController, AController* NewController)
{
	// Hand the context back to the controller that is losing us, while its
	// local player is still reachable.
	RemoveAppliedInputMapping();

	OnOwnerControllerChanged(OldController, NewController);

	// GetLocalPlayer() is not reliably populated at the moment this fires on a
	// client, so resolve on the next tick.
	ScheduleRefreshInputMappings();
}

void UEFInputMappingComponent::OnOwnerControllerChanged(AController* /*OldController*/, AController* /*NewController*/)
{
}

void UEFInputMappingComponent::NotifyActiveInputMappingChanged(UInputMappingContext* /*NewMappingContext*/, UInputMappingContext* /*PreviousMappingContext*/)
{
}

UEnhancedInputLocalPlayerSubsystem* UEFInputMappingComponent::GetEnhancedInputSubsystem() const
{
	const AActor* OwnerActor = GetOwner();
	const APlayerController* PlayerController = Cast<APlayerController>(OwnerActor);

	if (!PlayerController)
	{
		const APawn* OwnerPawn = Cast<APawn>(OwnerActor);
		if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
		{
			return nullptr;
		}
		PlayerController = Cast<APlayerController>(OwnerPawn->GetController());
	}

	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return nullptr;
	}

	ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();
	return LocalPlayer ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer) : nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Queries
// ─────────────────────────────────────────────────────────────────────────────

UInputMappingContext* UEFInputMappingComponent::GetActiveInputMapping() const
{
	return AppliedMappingContext;
}

int32 UEFInputMappingComponent::GetActiveInputMappingPriority() const
{
	return InputMappingStack.IsValidIndex(ActiveInputMappingIndex)
		? InputMappingStack[ActiveInputMappingIndex].Spec.Priority
		: 0;
}

TArray<FEFInputPromptEntry> UEFInputMappingComponent::GetActiveInputPrompts() const
{
	TArray<FEFInputPromptEntry> Prompts;
	const UEnhancedInputLocalPlayerSubsystem* InputSubsystem = GetEnhancedInputSubsystem();
	UEFPlayerMappableKeySettings::CollectInputPrompts(
		AppliedMappingContext,
		InputSubsystem ? InputSubsystem->GetUserSettings() : nullptr,
		Prompts);
	return Prompts;
}

TArray<FEFInputMappingSpec> UEFInputMappingComponent::GetInputMappingStack() const
{
	TArray<FEFInputMappingSpec> Specs;
	Specs.Reserve(InputMappingStack.Num());
	for (const FEFInputMappingEntry& Entry : InputMappingStack)
	{
		Specs.Add(Entry.Spec);
	}
	return Specs;
}

bool UEFInputMappingComponent::HasInputMapping(UInputMappingContext* MappingContext) const
{
	return MappingContext && InputMappingStack.ContainsByPredicate(
		[MappingContext](const FEFInputMappingEntry& Entry)
		{
			return Entry.Spec.MappingContext == MappingContext;
		});
}

bool UEFInputMappingComponent::IsInputMappingHandleActive(const FEFInputMappingHandle& Handle) const
{
	return Handle.IsValid() && InputMappingStack.ContainsByPredicate(
		[&Handle](const FEFInputMappingEntry& Entry)
		{
			return !Entry.bManaged && Entry.Handle == Handle;
		});
}
