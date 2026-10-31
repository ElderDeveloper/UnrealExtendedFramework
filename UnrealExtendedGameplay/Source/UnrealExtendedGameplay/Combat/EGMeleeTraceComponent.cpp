// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGMeleeTraceComponent.h"

#include "CollisionQueryParams.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "EGMeleeDamagePolicy.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "UnrealExtendedGameplay/Animation/Debug/EGAnimDebug.h"
#include "UnrealExtendedGameplay/Time/EGTimeDilationSubsystem.h"

UEGMeleeTraceComponent::UEGMeleeTraceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// After animation has posed the sockets for this frame.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	ObjectTypes.Add(ObjectTypeQuery3);
	DamagePolicy = CreateDefaultSubobject<UEGMeleeDamagePolicy_EngineDamage>(TEXT("DamagePolicy"));
}

// ---------------------------------------------------------------------------------------------
// Windows
// ---------------------------------------------------------------------------------------------

bool UEGMeleeTraceComponent::ShouldDetect() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	switch (Detection)
	{
	case EEGMeleeDetection::AuthorityOnly:
		return Owner->HasAuthority();
	case EEGMeleeDetection::AuthorityAndLocalOwner:
	{
		const APawn* Pawn = Cast<APawn>(Owner);
		return Owner->HasAuthority() || (Pawn && Pawn->IsLocallyControlled());
	}
	case EEGMeleeDetection::Everywhere:
		return true;
	}
	return false;
}

USceneComponent* UEGMeleeTraceComponent::ResolveSource(const FEGMeleeHitShape& Shape, USkeletalMeshComponent* DefaultSource) const
{
	// Most specific override wins: an override for Window.Right.Blade beats one for Window.Right.
	USceneComponent* Best = nullptr;
	int32 BestDepth = -1;
	for (const TPair<FGameplayTag, TWeakObjectPtr<USceneComponent>>& Override : SourceOverrides)
	{
		if (Override.Value.IsValid() && Shape.Tag.MatchesTag(Override.Key))
		{
			const int32 Depth = Override.Key.GetGameplayTagParents().Num();
			if (Depth > BestDepth)
			{
				Best = Override.Value.Get();
				BestDepth = Depth;
			}
		}
	}
	if (Best)
	{
		return Best;
	}

	AActor* const Owner = GetOwner();
	if (Owner && !Shape.SourceComponentName.IsNone())
	{
		TArray<AActor*> Actors;
		Owner->GetAttachedActors(Actors, true, true);
		Actors.Insert(Owner, 0);
		for (const AActor* Actor : Actors)
		{
			TInlineComponentArray<USceneComponent*> Components(Actor);
			for (USceneComponent* Component : Components)
			{
				if (Component && Component->GetFName() == Shape.SourceComponentName)
				{
					return Component;
				}
			}
		}
	}

	if (DefaultSource)
	{
		return DefaultSource;
	}
	if (const ACharacter* Character = Cast<ACharacter>(Owner))
	{
		return Character->GetMesh();
	}
	return Owner ? Owner->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
}

void UEGMeleeTraceComponent::SamplePoints(const FEGMeleeHitShape& Shape, const USceneComponent* Source, TArray<FVector>& OutPoints) const
{
	const FVector Start = Source->GetSocketLocation(Shape.StartSocket);
	if (Shape.EndSocket.IsNone())
	{
		EGMelee::SampleSegment(Start, Start, 1, OutPoints);
		return;
	}
	EGMelee::SampleSegment(Start, Source->GetSocketLocation(Shape.EndSocket), Shape.PointCount, OutPoints);
}

int32 UEGMeleeTraceComponent::OpenWindow(FGameplayTag WindowTag, USkeletalMeshComponent* DefaultSource, const FGameplayTagContainer& AttackTags, float DamageMultiplier)
{
	return OpenWindowWithId(EGAnimEvents::NewWindowId(), WindowTag, DefaultSource, DamageMultiplier, AttackTags);
}

int32 UEGMeleeTraceComponent::OpenWindowWithId(int32 WindowId, FGameplayTag WindowTag, USkeletalMeshComponent* DefaultSource, float DamageMultiplier, const FGameplayTagContainer& AttackTags)
{
	if (!WindowTag.IsValid())
	{
		return INDEX_NONE;
	}

	// Called from inside a sweep (a Blueprint CanHit): adding a window now would move the window
	// the sweep is working on, so open it once the sweep is done.
	if (bSweeping)
	{
		const TWeakObjectPtr<USkeletalMeshComponent> WeakSource(DefaultSource);
		DeferredOperations.Add([this, WindowId, WindowTag, WeakSource, DamageMultiplier, AttackTags]()
		{
			OpenWindowWithId(WindowId, WindowTag, WeakSource.Get(), DamageMultiplier, AttackTags);
		});
		return WindowId;
	}

	FWindow Window;
	Window.Id = WindowId;
	Window.Tag = WindowTag;
	Window.DamageMultiplier = DamageMultiplier;
	Window.AttackTags = AttackTags;

	for (int32 ShapeIndex = 0; ShapeIndex < Shapes.Num(); ++ShapeIndex)
	{
		const FEGMeleeHitShape& Shape = Shapes[ShapeIndex];
		if (!Shape.Tag.MatchesTag(WindowTag))
		{
			continue;
		}

		USceneComponent* const Source = ResolveSource(Shape, DefaultSource);
		if (!Source)
		{
			EGAnimDebug::WarnOnce(this, FString::Printf(TEXT("Melee shape %s has no component to read sockets from."), *Shape.Tag.ToString()));
			continue;
		}

		FActiveShape& Active = Window.ActiveShapes.AddDefaulted_GetRef();
		Active.ShapeIndex = ShapeIndex;
		Active.Source = Source;
		SamplePoints(Shape, Source, Active.PreviousPoints);
	}

	if (Window.ActiveShapes.Num() == 0)
	{
		return INDEX_NONE;
	}

	Windows.Add(MoveTemp(Window));
	UpdateTickEnabled();

	// Opened before any of its hits are delivered, so listeners see Opened, hits, Closed in order.
	OnWindowOpened.Broadcast(WindowTag, WindowId);

	// Catch what the shapes overlap the moment the window opens (unless a listener closed it).
	if (ShouldDetect())
	{
		TArray<FEGMeleeHit> Hits;
		if (FWindow* Opened = FindWindow(WindowId))
		{
			TGuardValue<bool> SweepGuard(bSweeping, true);
			SweepWindow(*Opened, Hits);
		}
		DeliverHits(Hits);
		FlushDeferredOperations();
	}

	return FindWindow(WindowId) ? WindowId : INDEX_NONE;
}

UEGMeleeTraceComponent::FWindow* UEGMeleeTraceComponent::FindWindow(int32 WindowId)
{
	return Windows.FindByPredicate([WindowId](const FWindow& Window) { return Window.Id == WindowId; });
}

void UEGMeleeTraceComponent::FlushDeferredOperations()
{
	// Each pass may queue more (a deferred close sweeps, and its CanHit may ask again); bounded so a
	// policy that always asks cannot loop forever.
	for (int32 Pass = 0; Pass < 8 && DeferredOperations.Num() > 0; ++Pass)
	{
		TArray<TFunction<void()>> Operations = MoveTemp(DeferredOperations);
		DeferredOperations.Reset();
		for (TFunction<void()>& Operation : Operations)
		{
			Operation();
		}
	}
	DeferredOperations.Reset();
}

void UEGMeleeTraceComponent::CloseWindow(int32 WindowId)
{
	if (bSweeping)
	{
		DeferredOperations.Add([this, WindowId]() { CloseWindow(WindowId); });
		return;
	}

	const int32 Index = Windows.IndexOfByPredicate([WindowId](const FWindow& Window) { return Window.Id == WindowId; });
	if (Index == INDEX_NONE)
	{
		return;
	}

	// One last sweep so the frame the window ends on still counts.
	TArray<FEGMeleeHit> Hits;
	if (ShouldDetect())
	{
		TGuardValue<bool> SweepGuard(bSweeping, true);
		SweepWindow(Windows[Index], Hits);
	}

	// Nothing could change Windows during the sweep (changes were deferred), so Index still holds.
	const FGameplayTag Tag = Windows[Index].Tag;
	Windows.RemoveAt(Index);
	UpdateTickEnabled();

	DeliverHits(Hits);
	OnWindowClosed.Broadcast(Tag, WindowId);
	FlushDeferredOperations();
}

void UEGMeleeTraceComponent::CloseAllWindows()
{
	if (bSweeping)
	{
		DeferredOperations.Add([this]() { CloseAllWindows(); });
		return;
	}

	while (Windows.Num() > 0)
	{
		CloseWindow(Windows.Last().Id);
	}
}

void UEGMeleeTraceComponent::SetShapeSource(FGameplayTag ShapeTag, USceneComponent* Source)
{
	if (!ShapeTag.IsValid())
	{
		return;
	}
	if (Source)
	{
		SourceOverrides.Add(ShapeTag, Source);
	}
	else
	{
		SourceOverrides.Remove(ShapeTag);
	}
}

bool UEGMeleeTraceComponent::IsWindowOpen(FGameplayTag WindowTag) const
{
	return Windows.ContainsByPredicate([&WindowTag](const FWindow& Window) { return Window.Tag.MatchesTag(WindowTag); });
}

void UEGMeleeTraceComponent::UpdateTickEnabled()
{
	SetComponentTickEnabled(Windows.Num() > 0 && ShouldDetect());
}

// ---------------------------------------------------------------------------------------------
// Anim events
// ---------------------------------------------------------------------------------------------

void UEGMeleeTraceComponent::ReceiveAnimEvent_Implementation(const FEGAnimEvent& Event)
{
	const bool bAnyShape = Shapes.ContainsByPredicate([&Event](const FEGMeleeHitShape& Shape) { return Shape.Tag.MatchesTag(Event.Tag); });
	if (!bAnyShape)
	{
		return;
	}

	switch (Event.Phase)
	{
	case EEGAnimEventPhase::Begin:
		OpenWindowWithId(Event.WindowId, Event.Tag, Event.Mesh, Event.Payload.Magnitude, Event.Payload.ExtraTags);
		break;
	case EEGAnimEventPhase::End:
		CloseWindow(Event.WindowId);
		break;
	case EEGAnimEventPhase::Trigger:
	{
		// A single-frame notify: one overlap at the current pose.
		const int32 WindowId = OpenWindowWithId(EGAnimEvents::NewWindowId(), Event.Tag, Event.Mesh, Event.Payload.Magnitude, Event.Payload.ExtraTags);
		if (WindowId != INDEX_NONE)
		{
			CloseWindow(WindowId);
		}
		break;
	}
	default:
		break;
	}
}

// ---------------------------------------------------------------------------------------------
// Sweeping
// ---------------------------------------------------------------------------------------------

void UEGMeleeTraceComponent::SweepWindow(FWindow& Window, TArray<FEGMeleeHit>& OutHits) const
{
	AActor* const Owner = GetOwner();
	UWorld* const World = GetWorld();
	if (!Owner || !World || ObjectTypes.Num() == 0)
	{
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EGMeleeTrace), bTraceComplex, Owner);
	QueryParams.bReturnPhysicalMaterial = true;
	TArray<AActor*> AttachedActors;
	Owner->GetAttachedActors(AttachedActors, true, true);
	QueryParams.AddIgnoredActors(AttachedActors);

	const FCollisionObjectQueryParams ObjectParams(ObjectTypes);
	const bool bAuthoritative = Owner->HasAuthority();
	const bool bDebug = EGAnimDebug::IsEnabled();

	TArray<FVector> CurrentPoints;
	TArray<FHitResult> SweepHits;

	for (FActiveShape& Active : Window.ActiveShapes)
	{
		const USceneComponent* const Source = Active.Source.Get();
		if (!Source || !Shapes.IsValidIndex(Active.ShapeIndex))
		{
			continue;
		}

		const FEGMeleeHitShape& Shape = Shapes[Active.ShapeIndex];
		SamplePoints(Shape, Source, CurrentPoints);
		const FCollisionShape Sphere = FCollisionShape::MakeSphere(Shape.Radius);

		for (int32 PointIndex = 0; PointIndex < CurrentPoints.Num(); ++PointIndex)
		{
			const FVector End = CurrentPoints[PointIndex];
			FVector Start = Active.PreviousPoints.IsValidIndex(PointIndex) ? Active.PreviousPoints[PointIndex] : End;

			// A still point is an overlap; give the sweep a hair of length so it returns one.
			if (FVector::DistSquared(Start, End) < KINDA_SMALL_NUMBER)
			{
				Start = End - FVector(0.0, 0.0, 0.1);
			}

			SweepHits.Reset();
			World->SweepMultiByObjectType(SweepHits, Start, End, FQuat::Identity, ObjectParams, Sphere, QueryParams);

#if ENABLE_DRAW_DEBUG
			if (bDebug)
			{
				const FColor Color = SweepHits.Num() > 0 ? FColor::Red : FColor::Green;
				DrawDebugLine(World, Start, End, Color, false, 1.0f, 0, 1.0f);
				DrawDebugSphere(World, End, Shape.Radius, 8, Color, false, 1.0f);
			}
#endif

			for (const FHitResult& SweepHit : SweepHits)
			{
				AActor* const Target = SweepHit.GetActor();
				if (!IsValid(Target) || Window.HitActors.Contains(TWeakObjectPtr<AActor>(Target)))
				{
					continue;
				}
				if (DamagePolicy && !DamagePolicy->CanHit(Owner, Target))
				{
					continue;
				}

				Window.HitActors.Add(Target);

				FEGMeleeHit& Hit = OutHits.AddDefaulted_GetRef();
				Hit.Target = Target;
				Hit.HitResult = SweepHit;
				Hit.WindowTag = Window.Tag;
				Hit.WindowId = Window.Id;
				Hit.DamageMultiplier = Window.DamageMultiplier;
				Hit.AttackTags = Window.AttackTags;
				Hit.bAuthoritative = bAuthoritative;

				const FVector Motion = End - Start;
				Hit.SwingDirection = Motion.SizeSquared() > 1.0
					? Motion.GetSafeNormal()
					: (Target->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal();
			}
		}

		Active.PreviousPoints = CurrentPoints;
	}
}

void UEGMeleeTraceComponent::DeliverHits(const TArray<FEGMeleeHit>& Hits)
{
	AActor* const Owner = GetOwner();
	for (const FEGMeleeHit& Hit : Hits)
	{
		if (!IsValid(Owner) || !IsValid(Hit.Target))
		{
			continue;
		}

		if (Hit.bAuthoritative && DamagePolicy)
		{
			DamagePolicy->ApplyHit(Owner, Hit);
		}

		if (HitPauseDuration > 0.0f)
		{
			if (UEGTimeDilationSubsystem* const TimeDilation = UEGTimeDilationSubsystem::Get(this))
			{
				TArray<USkeletalMeshComponent*> Meshes;
				if (bHitPauseAttacker)
				{
					const ACharacter* const Character = Cast<ACharacter>(Owner);
					Meshes.Add(Character ? Character->GetMesh() : Owner->FindComponentByClass<USkeletalMeshComponent>());
				}
				if (bHitPauseVictim && IsValid(Hit.Target))
				{
					const ACharacter* const Character = Cast<ACharacter>(Hit.Target);
					Meshes.Add(Character ? Character->GetMesh() : Hit.Target->FindComponentByClass<USkeletalMeshComponent>());
				}
				Meshes.RemoveAll([](const USkeletalMeshComponent* Mesh) { return Mesh == nullptr; });
				TimeDilation->HitPause(Meshes, HitPauseDuration, HitPauseAnimRate);
			}
		}

		OnHit.Broadcast(Hit);
	}
}

void UEGMeleeTraceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Gather every window's hits first: delivering them can open or close windows, and anything the
	// damage policy asks for during the sweep itself waits until the sweep is over.
	TArray<FEGMeleeHit> Hits;
	{
		TGuardValue<bool> SweepGuard(bSweeping, true);
		for (FWindow& Window : Windows)
		{
			SweepWindow(Window, Hits);
		}
	}
	DeliverHits(Hits);
	FlushDeferredOperations();
}

void UEGMeleeTraceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Windows.Reset();
	SourceOverrides.Reset();
	DeferredOperations.Reset();
	Super::EndPlay(EndPlayReason);
}
