// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGDamageTrace_Notify.h"

#include "Components/SkeletalMeshComponent.h"
#include "EGDamageTrace_DamageObject.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UnrealExtendedFramework/Libraries/Math/EFMathLibrary.h"
#include "UnrealExtendedFramework/Libraries/Trace/EFTraceLibrary.h"
#include "UnrealExtendedGameplay/Animation/EGAnimNotifyHelpers.h"

UEGDamageTrace_Notify::UEGDamageTrace_Notify()
{
	PSTemplate = nullptr;
}

bool UEGDamageTrace_Notify::ShouldTrace(const USkeletalMeshComponent* MeshComp) const
{
	if (!MeshComp || !MeshComp->GetOwner())
	{
		return false;
	}
	return !bAuthorityOnly || EGAnimNotify::HasAuthority(MeshComp);
}

void UEGDamageTrace_Notify::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!ShouldTrace(MeshComp))
	{
		return;
	}

	// Drop entries for meshes destroyed mid-window.
	for (auto It = ActiveTraces.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			ClearTraceTimer(It->Value);
			It.RemoveCurrent();
		}
	}

	// A window restarting on the same mesh starts with a clean hit list.
	const TWeakObjectPtr<USkeletalMeshComponent> MeshKey(MeshComp);
	StopTrace(MeshKey);
	ActiveTraces.Add(MeshKey).World = MeshComp->GetWorld();

	if (!UseEngineTickForCollisionCalculation)
	{
		TraceAndApply(MeshComp);

		// Re-find: damage applied by the first trace may already have ended the window.
		FActiveTrace* Trace = ActiveTraces.Find(MeshKey);
		UWorld* World = Trace ? Trace->World.Get() : nullptr;
		if (World)
		{
			const FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &UEGDamageTrace_Notify::HandleTraceTimer, MeshKey);
			World->GetTimerManager().SetTimer(Trace->TimerHandle, Delegate, FMath::Max(CollisionCalculationTickSpeed, 0.001f), true);
		}
	}
}

void UEGDamageTrace_Notify::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (UseEngineTickForCollisionCalculation)
	{
		TraceAndApply(MeshComp);
	}
}

void UEGDamageTrace_Notify::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	StopTrace(TWeakObjectPtr<USkeletalMeshComponent>(MeshComp));
}

void UEGDamageTrace_Notify::ClearTraceTimer(FActiveTrace& Trace)
{
	if (UWorld* World = Trace.World.Get())
	{
		World->GetTimerManager().ClearTimer(Trace.TimerHandle);
	}
	Trace.TimerHandle.Invalidate();
}

void UEGDamageTrace_Notify::StopTrace(const TWeakObjectPtr<USkeletalMeshComponent>& MeshKey)
{
	if (FActiveTrace* Trace = ActiveTraces.Find(MeshKey))
	{
		ClearTraceTimer(*Trace);
		ActiveTraces.Remove(MeshKey);
	}
}

void UEGDamageTrace_Notify::HandleTraceTimer(TWeakObjectPtr<USkeletalMeshComponent> WeakMesh)
{
	USkeletalMeshComponent* MeshComp = WeakMesh.Get();
	if (!IsValid(MeshComp))
	{
		// The mesh went away without ending the window; stop the timer instead of looping forever.
		// A stale weak key still hashes and compares equal, so the entry is found.
		StopTrace(WeakMesh);
		return;
	}

	TraceAndApply(MeshComp);
}

void UEGDamageTrace_Notify::TraceAndApply(USkeletalMeshComponent* MeshComp)
{
	if (!IsValid(MeshComp))
	{
		return;
	}

	AActor* const Owner = MeshComp->GetOwner();
	const TWeakObjectPtr<USkeletalMeshComponent> MeshKey(MeshComp);
	if (!IsValid(Owner) || !ActiveTraces.Contains(MeshKey))
	{
		// No entry: NotifyBegin skipped this mesh (no authority), so do not trace for nothing.
		return;
	}

	TArray<FHitResult> Hits;
	if (!RunTrace(MeshComp, Hits))
	{
		return;
	}

	// Record the new hits first and apply damage afterwards: damage can end this window (the
	// attacker dies, a montage is stopped), which removes the map entry under us.
	TArray<FHitResult, TInlineAllocator<4>> NewHits;
	{
		FActiveTrace* Trace = ActiveTraces.Find(MeshKey);
		if (!Trace)
		{
			return;
		}

		for (const FHitResult& Hit : Hits)
		{
			AActor* const HitActor = Hit.GetActor();
			const TWeakObjectPtr<AActor> HitKey(HitActor);
			if (!IsValid(HitActor) || HitActor == Owner || Trace->HitActors.Contains(HitKey))
			{
				continue;
			}

			Trace->HitActors.Add(HitKey);
			NewHits.Add(Hit);
		}
	}

	for (const FHitResult& Hit : NewHits)
	{
		if (!IsValid(MeshComp) || !IsValid(Hit.GetActor()))
		{
			continue;
		}

		HandleDamage(MeshComp, Hit);
		if (bShouldPush)
		{
			ApplyPush(MeshComp, Hit.GetActor());
		}
	}
}

bool UEGDamageTrace_Notify::RunTrace(USkeletalMeshComponent* MeshComp, TArray<FHitResult>& OutHits) const
{
	const FVector Start = MeshComp->GetSocketLocation(FirstSocketName);
	const FVector End = MeshComp->GetSocketLocation(SecondSocketName);

	// Trace on a copy: the settings belong to the animation asset and are shared by every mesh.
	// The mesh is the world context so bIgnoreSelf ignores the attacker.
	switch (NotifyTraceStruct.TraceShape)
	{
	case ETraceShapes::Sphere:
	{
		FSphereTraceStruct Settings = NotifyTraceStruct.SphereTraceSettings;
		Settings.Start = Start;
		Settings.End = End;
		if (UEFTraceLibrary::ExtendedSphereTraceMulti(MeshComp, Settings))
		{
			OutHits = MoveTemp(Settings.HitResults);
			return true;
		}
		return false;
	}
	case ETraceShapes::Box:
	{
		FBoxTraceStruct Settings = NotifyTraceStruct.BoxTraceSettings;
		Settings.Start = Start;
		Settings.End = End;
		if (UEFTraceLibrary::ExtendedBoxTraceMulti(MeshComp, Settings))
		{
			OutHits = MoveTemp(Settings.HitResults);
			return true;
		}
		return false;
	}
	case ETraceShapes::Line:
	{
		FLineTraceStruct Settings = NotifyTraceStruct.LineTraceSettings;
		Settings.Start = Start;
		Settings.End = End;
		if (UEFTraceLibrary::ExtendedLineTraceMulti(MeshComp, Settings))
		{
			OutHits = MoveTemp(Settings.HitResults);
			return true;
		}
		return false;
	}
	default:
		return false;
	}
}

void UEGDamageTrace_Notify::ApplyPush(USkeletalMeshComponent* MeshComp, AActor* HitActor) const
{
	const ACharacter* const Character = Cast<ACharacter>(HitActor);
	UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (Movement && MeshComp->GetOwner())
	{
		Movement->AddForce(UEFMathLibrary::GetDirectionBetweenActors(MeshComp->GetOwner(), Character, PushStrength));
	}
}

void UEGDamageTrace_Notify::HandleDamage(USkeletalMeshComponent* MeshComp, const FHitResult& HitResult)
{
	AActor* const Owner = MeshComp ? MeshComp->GetOwner() : nullptr;
	AActor* const HitActor = HitResult.GetActor();
	UWorld* const World = MeshComp ? MeshComp->GetWorld() : nullptr;
	if (!Owner || !HitActor || !World)
	{
		return;
	}

	if (NotifyDamageStruct.bApplyAnimNotifyDamage)
	{
		AController* const Instigator = Owner->GetInstigatorController();
		const TArray<AActor*> IgnoreActors = { Owner };

		switch (NotifyDamageStruct.ApplyDamageType)
		{
		case UEF_ApplyDamage:
			UGameplayStatics::ApplyDamage(HitActor, NotifyDamageStruct.AnimNotifyDamage, Instigator, Owner, NotifyDamageStruct.DamageType);
			break;

		case UEF_ApplyPointDamage:
		{
			// HitFromDirection is a direction, not the hit location that was passed here before.
			const FVector HitFromDirection = (HitResult.ImpactPoint - Owner->GetActorLocation()).GetSafeNormal();
			UGameplayStatics::ApplyPointDamage(HitActor, NotifyDamageStruct.AnimNotifyDamage, HitFromDirection, HitResult, Instigator, Owner, NotifyDamageStruct.DamageType);
			break;
		}

		case UEF_ApplyRadialDamage:
			UGameplayStatics::ApplyRadialDamage(World,
				NotifyDamageStruct.AnimNotifyDamage,
				HitResult.Location,
				NotifyDamageStruct.RadialDamageStruct.AreaDamageRadius,
				NotifyDamageStruct.DamageType,
				IgnoreActors,
				Owner,
				Instigator,
				true,
				NotifyDamageStruct.AreaDamageBlockChannel);
			break;

		case UEF_ApplyRadialDamageFalloff:
			UGameplayStatics::ApplyRadialDamageWithFalloff(World,
				NotifyDamageStruct.RadialDamageStruct.AreaDamageMaximum,
				NotifyDamageStruct.RadialDamageStruct.AreaDamageMinimum,
				HitResult.Location,
				NotifyDamageStruct.RadialDamageStruct.AreaDamageInnerRadius,
				NotifyDamageStruct.RadialDamageStruct.AreaDamageOuterRadius,
				NotifyDamageStruct.RadialDamageStruct.AreaDamageFalloff,
				NotifyDamageStruct.DamageType,
				IgnoreActors,
				Owner,
				Instigator,
				NotifyDamageStruct.AreaDamageBlockChannel);
			break;

		default:
			break;
		}
	}

	if (NotifyDamageStruct.CustomApplyDamageObject)
	{
		// Instantiate the chosen subclass; passing the class as the outer (as before) built the
		// base class, so Blueprint overrides never ran.
		UEGDamageTrace_DamageObject* const DamageObject = NewObject<UEGDamageTrace_DamageObject>(Owner, NotifyDamageStruct.CustomApplyDamageObject.Get(), NAME_None, RF_Transient);
		if (DamageObject)
		{
			DamageObject->DamageTag = NotifyDamageStruct.DamageCustomTag;
			DamageObject->ReceiveApplyDamageInformation(MeshComp, HitResult);
		}
	}
}
