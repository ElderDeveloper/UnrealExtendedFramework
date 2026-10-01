// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGChessSeatComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EGChessTableActor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Data/EGChessAnimationSet.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Player/EGChessCharacterInterface.h"
#include "UnrealExtendedGameplay/Systems/Minigames/Chess/Presentation/EGChessEngineAssets.h"

namespace EGChessSeatPrivate
{
	FQuat YawOnly(const FQuat& Rotation)
	{
		return FRotator(0.0f, Rotation.Rotator().Yaw, 0.0f).Quaternion();
	}

	/** Composition used throughout: Base then a delta expressed in Base's frame. */
	FTransform Compose(const FTransform& Base, const FTransform& Delta)
	{
		return FTransform(Base.GetRotation() * Delta.GetRotation(), Base.GetLocation() + Base.GetRotation().RotateVector(Delta.GetLocation()));
	}
}

UEGChessSeatComponent::UEGChessSeatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(true);
}

void UEGChessSeatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UEGChessSeatComponent, Occupant);
	DOREPLIFETIME(UEGChessSeatComponent, AnimState);
}

void UEGChessSeatComponent::OnUnregister()
{
	UnbindPawnAnimation();
	Super::OnUnregister();
}

AEGChessTableActor* UEGChessSeatComponent::GetTable() const
{
	return OwningTable.IsValid() ? OwningTable.Get() : Cast<AEGChessTableActor>(GetOwner());
}

double UEGChessSeatComponent::GetServerTime() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return 0.0;
	}
	const AGameStateBase* GameState = World->GetGameState();
	return GameState ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

UEGChessAnimationSet* UEGChessSeatComponent::GetAnimationSet(const APawn* Pawn) const
{
	const AEGChessTableActor* Table = GetTable();
	return Table ? Table->GetAnimationSetFor(Pawn) : nullptr;
}

USkeletalMeshComponent* UEGChessSeatComponent::GetPawnMesh(const APawn* Pawn) const
{
	if (!Pawn)
	{
		return nullptr;
	}
	if (Pawn->Implements<UEGChessCharacterInterface>())
	{
		if (USkeletalMeshComponent* Mesh = IEGChessCharacterInterface::Execute_GetChessMesh(const_cast<APawn*>(Pawn)))
		{
			return Mesh;
		}
	}
	if (const ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		return Character->GetMesh();
	}
	return Pawn->FindComponentByClass<USkeletalMeshComponent>();
}

UAnimInstance* UEGChessSeatComponent::GetPawnAnimInstance(const APawn* Pawn) const
{
	const USkeletalMeshComponent* Mesh = GetPawnMesh(Pawn);
	return Mesh ? Mesh->GetAnimInstance() : nullptr;
}

// ---------------------------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------------------------

FTransform UEGChessSeatComponent::GetSeatedActorTransform(const APawn* Pawn) const
{
	float HalfHeight = 88.0f;
	if (const ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		HalfHeight = Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	}
	const FVector Location = GetComponentLocation() + FVector(0.0f, 0.0f, HalfHeight);
	return FTransform(EGChessSeatPrivate::YawOnly(GetComponentQuat()), Location);
}

FVector UEGChessSeatComponent::GetSideDirection(EEGChessEntrySide Side) const
{
	switch (Side)
	{
	case EEGChessEntrySide::Left: return -GetRightVector().GetSafeNormal2D();
	case EEGChessEntrySide::Right: return GetRightVector().GetSafeNormal2D();
	default: return -GetForwardVector().GetSafeNormal2D();
	}
}

FVector UEGChessSeatComponent::GetEntryStartLocation(const APawn* Pawn, EEGChessEntrySide Side) const
{
	const FTransform Seated = GetSeatedActorTransform(Pawn);
	const UEGChessAnimationSet* Set = GetAnimationSet(Pawn);
	UAnimMontage* Montage = Set ? EGChessEngineAssets::Resolve(Set->GetEnterMontage(Side)) : nullptr;
	if (Montage && Set->bScriptedRootMotion)
	{
		const FTransform Motion = ExtractActorRootMotion(Montage, Montage->GetPlayLength());
		const FQuat DesiredRot = Seated.GetRotation() * Motion.GetRotation().Inverse();
		return Seated.GetLocation() - DesiredRot.RotateVector(Motion.GetLocation());
	}
	return Seated.GetLocation() + GetSideDirection(Side) * FallbackEntryDistance;
}

bool UEGChessSeatComponent::IsSpotClear(const APawn* Pawn, const FVector& Location) const
{
	const ACharacter* Character = Cast<ACharacter>(Pawn);
	const UWorld* World = GetWorld();
	if (!Character || !World)
	{
		return true;
	}
	const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(EGChessSeatSpot), false);
	Params.AddIgnoredActor(Pawn);
	Params.AddIgnoredActor(GetOwner());
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius() * 0.9f, Capsule->GetScaledCapsuleHalfHeight() * 0.9f);
	return !World->OverlapBlockingTestByChannel(Location, FQuat::Identity, Capsule->GetCollisionObjectType(), Shape, Params);
}

EEGChessEntrySide UEGChessSeatComponent::ChooseEntrySide(const APawn* Pawn) const
{
	TArray<EEGChessEntrySide> Order;
	if (Pawn)
	{
		const FVector Local = GetComponentTransform().InverseTransformPosition(Pawn->GetActorLocation());
		if (Local.X < -FMath::Abs(Local.Y))
		{
			Order = { EEGChessEntrySide::Back, Local.Y < 0.0f ? EEGChessEntrySide::Left : EEGChessEntrySide::Right, Local.Y < 0.0f ? EEGChessEntrySide::Right : EEGChessEntrySide::Left };
		}
		else if (Local.Y < 0.0f)
		{
			Order = { EEGChessEntrySide::Left, EEGChessEntrySide::Back, EEGChessEntrySide::Right };
		}
		else
		{
			Order = { EEGChessEntrySide::Right, EEGChessEntrySide::Back, EEGChessEntrySide::Left };
		}
	}
	else
	{
		Order = { EEGChessEntrySide::Left, EEGChessEntrySide::Right, EEGChessEntrySide::Back };
	}

	const UEGChessAnimationSet* Set = GetAnimationSet(Pawn);
	const bool bSetHasEntries = Set && (!Set->EnterLeft.IsNull() || !Set->EnterRight.IsNull() || !Set->EnterBack.IsNull());

	for (const EEGChessEntrySide Side : Order)
	{
		if (bSetHasEntries && Set->GetEnterMontage(Side).IsNull())
		{
			continue;
		}
		if (IsSpotClear(Pawn, GetEntryStartLocation(Pawn, Side)))
		{
			return Side;
		}
	}
	// Nothing is clear: take the first side that has a clip, else the preferred one.
	for (const EEGChessEntrySide Side : Order)
	{
		if (!bSetHasEntries || !Set->GetEnterMontage(Side).IsNull())
		{
			return Side;
		}
	}
	return Order[0];
}

EEGChessEntrySide UEGChessSeatComponent::ChooseExitSide(const APawn* Pawn) const
{
	const UEGChessAnimationSet* Set = GetAnimationSet(Pawn);
	const EEGChessEntrySide Order[3] = { EEGChessEntrySide::Left, EEGChessEntrySide::Right, EEGChessEntrySide::Back };
	const bool bSetHasExits = Set && (!Set->ExitLeft.IsNull() || !Set->ExitRight.IsNull() || !Set->ExitBack.IsNull());
	const FTransform Seated = GetSeatedActorTransform(Pawn);

	for (const EEGChessEntrySide Side : Order)
	{
		if (bSetHasExits && Set->GetExitMontage(Side).IsNull())
		{
			continue;
		}
		FVector End = Seated.GetLocation() + GetSideDirection(Side) * FallbackEntryDistance;
		if (UAnimMontage* Montage = Set ? EGChessEngineAssets::Resolve(Set->GetExitMontage(Side)) : nullptr)
		{
			if (Set->bScriptedRootMotion)
			{
				End = EGChessSeatPrivate::Compose(Seated, ExtractActorRootMotion(Montage, Montage->GetPlayLength())).GetLocation();
			}
		}
		if (IsSpotClear(Pawn, End))
		{
			return Side;
		}
	}
	for (const EEGChessEntrySide Side : Order)
	{
		if (!bSetHasExits || !Set->GetExitMontage(Side).IsNull())
		{
			return Side;
		}
	}
	return EEGChessEntrySide::Left;
}

float UEGChessSeatComponent::GetEnterDuration(const APawn* Pawn, EEGChessEntrySide Side) const
{
	const UEGChessAnimationSet* Set = GetAnimationSet(Pawn);
	if (UAnimMontage* Montage = Set ? EGChessEngineAssets::Resolve(Set->GetEnterMontage(Side)) : nullptr)
	{
		return Montage->GetPlayLength();
	}
	return Set ? Set->FallbackSlideTime : 0.35f;
}

FTransform UEGChessSeatComponent::ExtractActorRootMotion(UAnimMontage* Montage, float Time) const
{
	if (!Montage || Time <= 0.0f)
	{
		return FTransform::Identity;
	}
	const FTransform MeshSpace = Montage->ExtractRootMotionFromTrackRange(0.0f, FMath::Min(Time, Montage->GetPlayLength()), FAnimExtractContext());

	// Root motion is in mesh space; the mesh is usually yawed relative to the actor.
	FQuat MeshRelative = Sequence.MeshRelativeRotation;
	if (!Sequence.bActive)
	{
		const APawn* Pawn = Occupant.Pawn;
		const USkeletalMeshComponent* Mesh = GetPawnMesh(Pawn);
		MeshRelative = Mesh ? Mesh->GetRelativeRotation().Quaternion() : FQuat::Identity;
	}
	const FQuat Rotation = EGChessSeatPrivate::YawOnly(MeshRelative * MeshSpace.GetRotation() * MeshRelative.Inverse());
	const FVector Translation = MeshRelative.RotateVector(MeshSpace.GetTranslation());
	return FTransform(Rotation, Translation);
}

// ---------------------------------------------------------------------------------------------
// Occupant (server)
// ---------------------------------------------------------------------------------------------

void UEGChessSeatComponent::SetOccupant(const FEGChessSeatOccupant& NewOccupant)
{
	Occupant = NewOccupant;
	OnRep_Occupant();
}

void UEGChessSeatComponent::ClearOccupant()
{
	APawn* Pawn = Occupant.Pawn;
	if (IsValid(Pawn))
	{
		ReleaseSeatedPawn(Pawn);
	}
	PreparedPawn.Reset();
	Occupant = FEGChessSeatOccupant();
	AnimState.Action = EEGChessSeatAction::None;
	++AnimState.Sequence;
	OnRep_Occupant();
	OnRep_AnimState();
}

void UEGChessSeatComponent::MarkDisconnected(double ForfeitServerTime)
{
	Occupant.bDisconnected = true;
	Occupant.ForfeitServerTime = ForfeitServerTime;
	Occupant.PlayerState = nullptr;
	OnRep_Occupant();
}

void UEGChessSeatComponent::OnRep_Occupant()
{
	if (AEGChessTableActor* Table = GetTable())
	{
		Table->HandleSeatOccupantChanged(SeatId);
	}
	// A body can arrive after its animation state on a client.
	if (Occupant.Pawn && AnimState.Action != EEGChessSeatAction::None && PreparedPawn.Get() != Occupant.Pawn)
	{
		AppliedSequence = 255;
		ApplyAnimState();
	}
}

// ---------------------------------------------------------------------------------------------
// Seat sequence (server entry points)
// ---------------------------------------------------------------------------------------------

void UEGChessSeatComponent::BeginEnter(EEGChessEntrySide Side, const FTransform& StartTransform)
{
	AnimState.Action = EEGChessSeatAction::Entering;
	AnimState.Side = Side;
	AnimState.StartServerTime = GetServerTime();
	AnimState.StartTransform = StartTransform;
	AnimState.bSnap = false;
	++AnimState.Sequence;
	OnRep_AnimState();
}

void UEGChessSeatComponent::SnapSeated()
{
	AnimState.Action = EEGChessSeatAction::Seated;
	AnimState.StartServerTime = GetServerTime();
	AnimState.bSnap = true;
	++AnimState.Sequence;
	OnRep_AnimState();
}

void UEGChessSeatComponent::BeginExit(EEGChessEntrySide Side, bool bImmediate)
{
	APawn* Pawn = Occupant.Pawn;
	AnimState.Action = EEGChessSeatAction::Exiting;
	AnimState.Side = Side;
	AnimState.StartServerTime = GetServerTime();
	AnimState.StartTransform = GetSeatedActorTransform(Pawn);
	AnimState.bSnap = bImmediate;
	++AnimState.Sequence;
	OnRep_AnimState();
}

void UEGChessSeatComponent::OnRep_AnimState()
{
	ApplyAnimState();
}

// ---------------------------------------------------------------------------------------------
// Playing the state (every machine)
// ---------------------------------------------------------------------------------------------

void UEGChessSeatComponent::ApplyAnimState()
{
	if (AppliedSequence == AnimState.Sequence && PreparedPawn.Get() == Occupant.Pawn)
	{
		return;
	}
	AppliedSequence = AnimState.Sequence;

	APawn* Pawn = Occupant.Pawn;
	AEGChessTableActor* Table = GetTable();
	const float Elapsed = FMath::Max(0.0f, static_cast<float>(GetServerTime() - AnimState.StartServerTime));
	UEGChessAnimationSet* Set = GetAnimationSet(Pawn);

	switch (AnimState.Action)
	{
	case EEGChessSeatAction::Entering:
	{
		if (!Pawn)
		{
			return;
		}
		PrepareSeatedPawn(Pawn);
		UAnimMontage* Montage = Set ? EGChessEngineAssets::Resolve(Set->GetEnterMontage(AnimState.Side)) : nullptr;
		BeginSequence(false, Montage, AnimState.StartTransform, Elapsed);
		if (Table)
		{
			Table->HandleSeatActionStarted(SeatId, EEGChessSeatAction::Entering);
		}
		break;
	}
	case EEGChessSeatAction::Seated:
	{
		if (!Pawn)
		{
			return;
		}
		PrepareSeatedPawn(Pawn);
		Sequence.bActive = false;
		SetComponentTickEnabled(false);
		SetPawnTransform(Pawn, GetSeatedActorTransform(Pawn));
		PlaySeatedIdle();
		break;
	}
	case EEGChessSeatAction::Exiting:
	{
		if (!Pawn)
		{
			if (GetOwnerRole() == ROLE_Authority && Table)
			{
				Table->HandleSeatExitFinished(SeatId);
			}
			return;
		}
		UAnimMontage* Montage = (!AnimState.bSnap && Set) ? EGChessEngineAssets::Resolve(Set->GetExitMontage(AnimState.Side)) : nullptr;
		if (AnimState.bSnap)
		{
			Sequence = FSequenceState();
			Sequence.bExit = true;
			Sequence.Final = FTransform(AnimState.StartTransform.GetRotation(), AnimState.StartTransform.GetLocation() + GetSideDirection(AnimState.Side) * FallbackEntryDistance);
			Sequence.bActive = true;
			FinishSequence();
		}
		else
		{
			BeginSequence(true, Montage, AnimState.StartTransform, Elapsed);
		}
		if (Table)
		{
			Table->HandleSeatActionStarted(SeatId, EEGChessSeatAction::Exiting);
		}
		break;
	}
	default:
	{
		Sequence.bActive = false;
		SetComponentTickEnabled(false);
		if (PreparedPawn.IsValid())
		{
			ReleaseSeatedPawn(PreparedPawn.Get());
		}
		break;
	}
	}
}


void UEGChessSeatComponent::BeginSequence(bool bExit, UAnimMontage* Montage, const FTransform& Start, float Elapsed)
{
	using namespace EGChessSeatPrivate;

	APawn* Pawn = Occupant.Pawn;
	const UEGChessAnimationSet* Set = GetAnimationSet(Pawn);
	const USkeletalMeshComponent* Mesh = GetPawnMesh(Pawn);
	UWorld* World = GetWorld();
	if (!Pawn || !World)
	{
		return;
	}

	Sequence = FSequenceState();
	Sequence.bActive = true;
	Sequence.bExit = bExit;
	Sequence.Montage = Montage;
	Sequence.MeshRelativeRotation = Mesh ? Mesh->GetRelativeRotation().Quaternion() : FQuat::Identity;
	Sequence.Start = FTransform(YawOnly(Start.GetRotation()), Start.GetLocation());
	Sequence.bScripted = Montage != nullptr && Set != nullptr && Set->bScriptedRootMotion;
	Sequence.bFallbackSlide = Montage == nullptr;
	Sequence.Duration = Montage ? Montage->GetPlayLength() : (Set ? Set->FallbackSlideTime : 0.35f);

	const FTransform Seated = GetSeatedActorTransform(Pawn);
	const FVector SideOffset = GetSideDirection(AnimState.Side) * FallbackEntryDistance;

	if (bExit)
	{
		if (Sequence.bScripted)
		{
			Sequence.Final = Compose(Sequence.Start, ExtractActorRootMotion(Montage, Sequence.Duration));
		}
		else
		{
			Sequence.Final = FTransform(Sequence.Start.GetRotation(), Sequence.Start.GetLocation() + SideOffset);
		}
	}
	else
	{
		Sequence.Final = Seated;
		if (Sequence.bScripted)
		{
			// Where the clip would have to start to end exactly on the seat. The actual start is
			// blended into it over the first part of the clip, so a mis-placed player still lands.
			const FTransform Motion = ExtractActorRootMotion(Montage, Sequence.Duration);
			const FQuat DesiredRotation = Seated.GetRotation() * Motion.GetRotation().Inverse();
			const FVector DesiredLocation = Seated.GetLocation() - DesiredRotation.RotateVector(Motion.GetLocation());
			Sequence.Desired = FTransform(DesiredRotation, DesiredLocation);
			Sequence.WarpDuration = Sequence.Duration * (Set ? Set->WarpFraction : 0.4f);
		}
	}

	Sequence.LocalStartTime = World->GetTimeSeconds() - Elapsed;

	if (Elapsed >= Sequence.Duration)
	{
		FinishSequence();
		return;
	}

	if (Montage)
	{
		PlayMontage(Montage, 1.0f, Elapsed, false);
	}
	SetPawnTransform(Pawn, EvaluateSequence(Elapsed));
	SetComponentTickEnabled(true);
}

FTransform UEGChessSeatComponent::EvaluateSequence(float Time) const
{
	using namespace EGChessSeatPrivate;

	if (Sequence.bScripted)
	{
		const FTransform Motion = ExtractActorRootMotion(Sequence.Montage.Get(), Time);
		if (Sequence.bExit)
		{
			return Compose(Sequence.Start, Motion);
		}
		const float Alpha = Sequence.WarpDuration > KINDA_SMALL_NUMBER ? FMath::SmoothStep(0.0f, 1.0f, Time / Sequence.WarpDuration) : 1.0f;
		const FTransform Base(
			FQuat::Slerp(Sequence.Start.GetRotation(), Sequence.Desired.GetRotation(), Alpha),
			FMath::Lerp(Sequence.Start.GetLocation(), Sequence.Desired.GetLocation(), Alpha));
		return Compose(Base, Motion);
	}

	if (Sequence.bFallbackSlide)
	{
		const float Alpha = Sequence.Duration > KINDA_SMALL_NUMBER ? FMath::SmoothStep(0.0f, 1.0f, Time / Sequence.Duration) : 1.0f;
		return FTransform(
			FQuat::Slerp(Sequence.Start.GetRotation(), Sequence.Final.GetRotation(), Alpha),
			FMath::Lerp(Sequence.Start.GetLocation(), Sequence.Final.GetLocation(), Alpha));
	}

	// A clip without scripted root motion carries the body itself: the capsule sits on the seat
	// while entering, and stays on the seat until the exit clip is over.
	return Sequence.bExit ? Sequence.Start : Sequence.Final;
}

void UEGChessSeatComponent::FinishSequence()
{
	const bool bExit = Sequence.bExit;
	const FTransform Final = Sequence.Final;
	Sequence.bActive = false;
	SetComponentTickEnabled(false);

	APawn* Pawn = Occupant.Pawn;
	if (Pawn)
	{
		SetPawnTransform(Pawn, Final);
	}

	if (!bExit)
	{
		PlaySeatedIdle();
		return;
	}

	if (Pawn)
	{
		ReleaseSeatedPawn(Pawn);
	}
	if (GetOwnerRole() == ROLE_Authority)
	{
		if (AEGChessTableActor* Table = GetTable())
		{
			Table->HandleSeatExitFinished(SeatId);
		}
	}
}

void UEGChessSeatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const UWorld* World = GetWorld();
	APawn* Pawn = Occupant.Pawn;
	if (!Sequence.bActive || !World || !Pawn)
	{
		SetComponentTickEnabled(false);
		return;
	}

	const float Time = static_cast<float>(World->GetTimeSeconds() - Sequence.LocalStartTime);

	// Start the seated idle where the enter clip would begin its auto blend-out, so the two seated
	// poses crossfade. Started only once the clip has finished, the blend-out ran towards the
	// animation blueprint's standing pose first: the body rose and sank back just after landing.
	if (!Sequence.bExit && !Sequence.bIdleStarted)
	{
		const UAnimMontage* EnterMontage = Sequence.Montage.Get();
		const float LeadIn = EnterMontage ? EnterMontage->BlendOut.GetBlendTime() : 0.0f;
		if (EnterMontage && Time >= Sequence.Duration - LeadIn)
		{
			Sequence.bIdleStarted = true;
			PlaySeatedIdle();
		}
	}

	if (Time >= Sequence.Duration)
	{
		FinishSequence();
		return;
	}
	SetPawnTransform(Pawn, EvaluateSequence(Time));
}

void UEGChessSeatComponent::SetPawnTransform(APawn* Pawn, const FTransform& Transform)
{
	if (!Pawn)
	{
		return;
	}
	Pawn->SetActorLocationAndRotation(Transform.GetLocation(), Transform.GetRotation(), false, nullptr, ETeleportType::TeleportPhysics);

	// A pawn that follows its controller's yaw would be turned straight back; keep them aligned.
	AController* Controller = Pawn->GetController();
	if (Controller && (Pawn->HasAuthority() || Pawn->IsLocallyControlled()))
	{
		FRotator ControlRotation = Controller->GetControlRotation();
		ControlRotation.Yaw = Transform.Rotator().Yaw;
		Controller->SetControlRotation(ControlRotation);
	}
}

// ---------------------------------------------------------------------------------------------
// Pawn preparation
// ---------------------------------------------------------------------------------------------

void UEGChessSeatComponent::PrepareSeatedPawn(APawn* Pawn)
{
	if (!Pawn || PreparedPawn.Get() == Pawn)
	{
		return;
	}
	if (PreparedPawn.IsValid())
	{
		ReleaseSeatedPawn(PreparedPawn.Get());
	}
	PreparedPawn = Pawn;

	if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
			Movement->DisableMovement();
			if (Pawn->HasAuthority())
			{
				Movement->bIgnoreClientMovementErrorChecksAndCorrection = true;
			}
		}
		Character->GetCapsuleComponent()->IgnoreActorWhenMoving(GetOwner(), true);
	}
	if (Pawn->HasAuthority())
	{
		Pawn->SetReplicateMovement(false);
	}

	BindPawnAnimation(Pawn);

	if (Pawn->Implements<UEGChessCharacterInterface>())
	{
		IEGChessCharacterInterface::Execute_OnChessSeatChanged(Pawn, SeatId, true);
	}
}

void UEGChessSeatComponent::ReleaseSeatedPawn(APawn* Pawn)
{
	if (!Pawn)
	{
		return;
	}

	if (const UEGChessAnimationSet* Set = GetAnimationSet(Pawn))
	{
		if (UAnimMontage* Idle = Set->SeatedIdle.Get())
		{
			StopMontage(Idle);
		}
	}
	if (CurrentMoveMontage.IsValid())
	{
		StopMontage(CurrentMoveMontage.Get());
	}

	if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->SetMovementMode(MOVE_Walking);
			if (Pawn->HasAuthority())
			{
				Movement->bIgnoreClientMovementErrorChecksAndCorrection = false;
			}
		}
		Character->GetCapsuleComponent()->IgnoreActorWhenMoving(GetOwner(), false);
	}
	if (Pawn->HasAuthority())
	{
		Pawn->SetReplicateMovement(true);
	}

	UnbindPawnAnimation();

	if (Pawn->Implements<UEGChessCharacterInterface>())
	{
		IEGChessCharacterInterface::Execute_OnChessSeatChanged(Pawn, SeatId, false);
	}
	if (PreparedPawn.Get() == Pawn)
	{
		PreparedPawn.Reset();
	}
}

void UEGChessSeatComponent::BindPawnAnimation(APawn* Pawn)
{
	UnbindPawnAnimation();
	if (UAnimInstance* AnimInstance = GetPawnAnimInstance(Pawn))
	{
		AnimInstance->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &UEGChessSeatComponent::HandleMontageNotifyBegin);
		BoundAnimInstance = AnimInstance;
	}
}

void UEGChessSeatComponent::UnbindPawnAnimation()
{
	if (UAnimInstance* AnimInstance = BoundAnimInstance.Get())
	{
		AnimInstance->OnPlayMontageNotifyBegin.RemoveDynamic(this, &UEGChessSeatComponent::HandleMontageNotifyBegin);
	}
	BoundAnimInstance.Reset();
}

// ---------------------------------------------------------------------------------------------
// Montages
// ---------------------------------------------------------------------------------------------

bool UEGChessSeatComponent::PlayMontage(UAnimMontage* Montage, float PlayRate, float StartTime, bool bReturnToIdle)
{
	APawn* Pawn = Occupant.Pawn;
	if (!Montage || !Pawn)
	{
		return false;
	}
	if (Pawn->Implements<UEGChessCharacterInterface>() && IEGChessCharacterInterface::Execute_PlayChessMontage(Pawn, Montage, StartTime, PlayRate))
	{
		return true;
	}
	UAnimInstance* AnimInstance = GetPawnAnimInstance(Pawn);
	if (!AnimInstance)
	{
		return false;
	}
	if (BoundAnimInstance.Get() != AnimInstance)
	{
		BindPawnAnimation(Pawn);
	}
	const float Length = AnimInstance->Montage_Play(Montage, PlayRate, EMontagePlayReturnType::MontageLength, StartTime);
	if (Length <= 0.0f)
	{
		return false;
	}
	if (bReturnToIdle)
	{
		FOnMontageBlendingOutStarted BlendingOut;
		BlendingOut.BindUObject(this, &UEGChessSeatComponent::HandleOneShotBlendingOut);
		AnimInstance->Montage_SetBlendingOutDelegate(BlendingOut, Montage);
	}
	return true;
}

void UEGChessSeatComponent::StopMontage(UAnimMontage* Montage)
{
	APawn* Pawn = PreparedPawn.IsValid() ? PreparedPawn.Get() : Occupant.Pawn.Get();
	if (!Montage || !Pawn)
	{
		return;
	}
	if (Pawn->Implements<UEGChessCharacterInterface>() && IEGChessCharacterInterface::Execute_StopChessMontage(Pawn, Montage))
	{
		return;
	}
	if (UAnimInstance* AnimInstance = GetPawnAnimInstance(Pawn))
	{
		AnimInstance->Montage_Stop(0.25f, Montage);
	}
}

void UEGChessSeatComponent::PlaySeatedIdle()
{
	APawn* Pawn = Occupant.Pawn;
	const UEGChessAnimationSet* Set = GetAnimationSet(Pawn);
	UAnimMontage* Idle = Set ? EGChessEngineAssets::Resolve(Set->SeatedIdle) : nullptr;
	if (!Idle)
	{
		return;
	}
	if (const UAnimInstance* AnimInstance = GetPawnAnimInstance(Pawn))
	{
		if (AnimInstance->Montage_IsPlaying(Idle))
		{
			return;
		}
	}
	PlayMontage(Idle, 1.0f, 0.0f, false);
}

void UEGChessSeatComponent::HandleOneShotBlendingOut(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage == CurrentMoveMontage.Get())
	{
		CurrentMoveMontage.Reset();
	}
	if (!bInterrupted && !Sequence.bActive && (AnimState.Action == EEGChessSeatAction::Seated || AnimState.Action == EEGChessSeatAction::Entering))
	{
		PlaySeatedIdle();
	}
}

bool UEGChessSeatComponent::PlayMoveClip(float PlayRate, bool bClockOnRight, FEGChessMoveTiming& OutTiming)
{
	APawn* Pawn = Occupant.Pawn;
	const UEGChessAnimationSet* Set = GetAnimationSet(Pawn);
	if (!Pawn || !Set || Sequence.bActive || AnimState.Action == EEGChessSeatAction::Exiting || AnimState.Action == EEGChessSeatAction::None)
	{
		return false;
	}

	UAnimMontage* Montage = EGChessEngineAssets::Resolve(bClockOnRight ? Set->MoveRightHand : Set->MoveLeftHand);
	if (!Montage)
	{
		Montage = EGChessEngineAssets::Resolve(bClockOnRight ? Set->MoveLeftHand : Set->MoveRightHand);
	}
	if (!Montage || !PlayMontage(Montage, PlayRate, 0.0f, true))
	{
		return false;
	}

	CurrentMoveMontage = Montage;
	const float Rate = FMath::Max(PlayRate, KINDA_SMALL_NUMBER);
	OutTiming.bCharacterDriven = true;
	OutTiming.PickTime = Set->FallbackPickTime / Rate;
	OutTiming.PlaceTime = FMath::Max(OutTiming.PickTime + 0.05f, Set->FallbackPlaceTime / Rate);
	OutTiming.ClockPressTime = FMath::Max(OutTiming.PlaceTime, Set->FallbackClockPressTime / Rate);
	return true;
}

float UEGChessSeatComponent::GetMoveClipDuration(float PlayRate, bool bClockOnRight) const
{
	const APawn* Pawn = Occupant.Pawn;
	const UEGChessAnimationSet* Set = GetAnimationSet(Pawn);
	if (!Pawn || !Set)
	{
		return 0.0f;
	}
	UAnimMontage* Montage = EGChessEngineAssets::Resolve(bClockOnRight ? Set->MoveRightHand : Set->MoveLeftHand);
	if (!Montage)
	{
		Montage = EGChessEngineAssets::Resolve(bClockOnRight ? Set->MoveLeftHand : Set->MoveRightHand);
	}
	return Montage ? Montage->GetPlayLength() / FMath::Max(PlayRate, KINDA_SMALL_NUMBER) : 0.0f;
}

void UEGChessSeatComponent::PlayReaction(EEGChessReaction Reaction, int32 Variant)
{
	APawn* Pawn = Occupant.Pawn;
	const UEGChessAnimationSet* Set = GetAnimationSet(Pawn);
	if (!Pawn || !Set || Sequence.bActive || AnimState.Action == EEGChessSeatAction::Exiting || AnimState.Action == EEGChessSeatAction::None)
	{
		return;
	}

	UAnimMontage* Montage = nullptr;
	if (Reaction == EEGChessReaction::Thinking)
	{
		if (Set->ThinkingIdles.Num() > 0)
		{
			Montage = EGChessEngineAssets::Resolve(Set->ThinkingIdles[FMath::Abs(Variant) % Set->ThinkingIdles.Num()]);
		}
	}
	else
	{
		Montage = EGChessEngineAssets::Resolve(Set->GetReactionMontage(Reaction));
	}
	if (Montage)
	{
		PlayMontage(Montage, 1.0f, 0.0f, true);
	}
}

void UEGChessSeatComponent::HandleMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& Payload)
{
	if (!CurrentMoveMontage.IsValid() || Payload.SequenceAsset != CurrentMoveMontage.Get())
	{
		return;
	}
	if (AEGChessTableActor* Table = GetTable())
	{
		Table->HandleCharacterNotify(SeatId, NotifyName);
	}
}
