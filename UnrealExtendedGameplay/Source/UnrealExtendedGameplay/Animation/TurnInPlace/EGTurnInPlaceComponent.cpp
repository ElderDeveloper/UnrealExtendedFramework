// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGTurnInPlaceComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "EGTurnInPlaceAnimSource.h"
#include "EGTurnInPlaceSet.h"
#include "EGTurnInPlaceSolver.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "HAL/IConsoleManager.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "UnrealExtendedGameplay/Animation/Debug/EGAnimDebug.h"
#include "UnrealExtendedGameplay/Animation/EGAnimationTags.h"
#include "UnrealExtendedGameplay/Animation/Events/EGAnimEventComponent.h"
#include "UnrealExtendedGameplay/Movement/EGMovementModifierComponent.h"
#include "UnrealExtendedGameplay/SoftCollision/EGRootMotionSource_SoftPush.h"

namespace EGTurnInPlaceCVars
{
	static TAutoConsoleVariable<int32> Override(
		TEXT("eg.TurnInPlace.Override"), 0,
		TEXT("Force the turn in place state. 0 default, 1 enabled, 2 locked, 3 paused."),
		ECVF_Cheat);

	static TAutoConsoleVariable<int32> Debug(
		TEXT("eg.TurnInPlace.Debug"), 0,
		TEXT("1: show each character's turn offset and draw actor (pink), control (black) and turn target (green) arrows."),
		ECVF_Cheat);

	static TAutoConsoleVariable<int32> DebugState(
		TEXT("eg.TurnInPlace.Debug.State"), 0,
		TEXT("1: show each character's resolved turn in place state and why."),
		ECVF_Cheat);
}

namespace
{
	/** Frames a hook may go missing (while the engine would call it) before it is reported. */
	constexpr int32 MissingHookFrames = 120;
}

UEGTurnInPlaceComponent::UEGTurnInPlaceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetIsReplicatedByDefault(true);
}

UEGTurnInPlaceComponent* UEGTurnInPlaceComponent::FindFor(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UEGTurnInPlaceComponent>() : nullptr;
}

void UEGTurnInPlaceComponent::SetRotationMode(ACharacter* Character, EEGCharacterRotationMode Mode)
{
	UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return;
	}

	Character->bUseControllerRotationYaw = Mode == EEGCharacterRotationMode::StrafeDirect;
	Movement->bOrientRotationToMovement = Mode == EEGCharacterRotationMode::OrientToMovement;
	Movement->bUseControllerDesiredRotation = Mode == EEGCharacterRotationMode::StrafeDesired;
}

// ---------------------------------------------------------------------------------------------
// Lifetime
// ---------------------------------------------------------------------------------------------

void UEGTurnInPlaceComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.Condition = COND_SimulatedOnly;
	DOREPLIFETIME_WITH_PARAMS_FAST(UEGTurnInPlaceComponent, SimulatedOffset, Params);
}

void UEGTurnInPlaceComponent::OnRegister()
{
	Super::OnRegister();
	CachedCharacter = Cast<ACharacter>(GetOwner());
}

void UEGTurnInPlaceComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedCharacter = Cast<ACharacter>(GetOwner());
	if (USkeletalMeshComponent* const Mesh = GetMesh())
	{
		// Tick after the mesh, so the curve copy is taken once its animation work is done.
		AddTickPrerequisiteComponent(Mesh);
		Mesh->OnAnimInitialized.AddUniqueDynamic(this, &UEGTurnInPlaceComponent::HandleAnimInitialized);
		HandleAnimInitialized();

		// Without refreshed bones the server never runs the turn states, and its offset diverges.
		if (GetNetMode() == NM_DedicatedServer && DedicatedServerAnimMode == EEGTurnServerAnimMode::Animation
			&& Mesh->VisibilityBasedAnimTickOption != EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones)
		{
			ReportProblem(TEXT("Dedicated server in Animation mode needs the mesh's VisibilityBasedAnimTickOption set to AlwaysTickPoseAndRefreshBones, or set DedicatedServerAnimMode to Pseudo."));
		}
	}
	else
	{
		ReportProblem(TEXT("Turn in place needs an ACharacter with a skeletal mesh."));
	}

	if (!TurnSet)
	{
		ReportProblem(TEXT("No TurnSet assigned; turn in place is off."));
	}

	RefreshStateProviders();
}

void UEGTurnInPlaceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (USkeletalMeshComponent* const Mesh = GetMesh())
	{
		Mesh->OnAnimInitialized.RemoveDynamic(this, &UEGTurnInPlaceComponent::HandleAnimInitialized);
	}
	Requests.Reset();
	Super::EndPlay(EndPlayReason);
}

void UEGTurnInPlaceComponent::HandleAnimInitialized()
{
	USkeletalMeshComponent* const Mesh = GetMesh();
	UAnimInstance* const AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	CachedAnimInstance = AnimInstance;
	bHasAnimSource = AnimInstance && AnimInstance->Implements<UEGTurnInPlaceAnimSource>();
	CachedCurves = FEGTurnInPlaceCurves();

	if (AnimInstance && !bHasAnimSource && !WantsPseudoAnimState())
	{
		ReportProblem(FString::Printf(TEXT("Anim instance %s does not implement IEGTurnInPlaceAnimSource. Parent the AnimBP to UEGAnimInstance or implement the interface."), *GetNameSafe(AnimInstance->GetClass())));
	}

	// Blueprint components exist by now on spawned characters.
	RefreshStateProviders();
}

void UEGTurnInPlaceComponent::RefreshStateProviders()
{
	StateProviders.Reset();
	AActor* const Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (Owner->Implements<UEGAnimStateProvider>())
	{
		StateProviders.Add(Owner);
	}
	TInlineComponentArray<UActorComponent*> Components(Owner);
	for (UActorComponent* Component : Components)
	{
		// Not this component: its own state tags depend on the stance it is resolving.
		if (Component && Component != this && Component->Implements<UEGAnimStateProvider>())
		{
			StateProviders.Add(Component);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------------------------

ACharacter* UEGTurnInPlaceComponent::GetCharacter() const
{
	return CachedCharacter.Get();
}

UCharacterMovementComponent* UEGTurnInPlaceComponent::GetMovement() const
{
	const ACharacter* const Character = GetCharacter();
	return Character ? Character->GetCharacterMovement() : nullptr;
}

USkeletalMeshComponent* UEGTurnInPlaceComponent::GetMesh() const
{
	const ACharacter* const Character = GetCharacter();
	return Character ? Character->GetMesh() : nullptr;
}

bool UEGTurnInPlaceComponent::WantsPseudoAnimState() const
{
	return GetNetMode() == NM_DedicatedServer && DedicatedServerAnimMode == EEGTurnServerAnimMode::Pseudo;
}

bool UEGTurnInPlaceComponent::HasValidData() const
{
	const ACharacter* const Character = GetCharacter();
	return TurnSet && Character && !Character->IsPendingKillPending() && Character->GetCharacterMovement()
		&& (bHasAnimSource || WantsPseudoAnimState());
}

bool UEGTurnInPlaceComponent::IsStationary() const
{
	const AActor* const Owner = GetOwner();
	return Owner && Owner->GetVelocity().IsNearlyZero();
}

EEGTurnMethod UEGTurnInPlaceComponent::GetTurnMethod() const
{
	const ACharacter* const Character = GetCharacter();
	const UCharacterMovementComponent* const Movement = GetMovement();
	if (!Character || !Movement)
	{
		return EEGTurnMethod::None;
	}

	if (!Movement->bOrientRotationToMovement && (Character->bUseControllerRotationPitch || Character->bUseControllerRotationYaw || Character->bUseControllerRotationRoll))
	{
		return EEGTurnMethod::FaceRotation;
	}
	if (Movement->bOrientRotationToMovement || Movement->bUseControllerDesiredRotation)
	{
		return EEGTurnMethod::PhysicsRotation;
	}
	return EEGTurnMethod::None;
}

void UEGTurnInPlaceComponent::GetGravity(FVector& OutUp, FQuat& OutWorldToGravity, FQuat& OutGravityToWorld) const
{
	OutUp = FVector::UpVector;
	OutWorldToGravity = FQuat::Identity;
	OutGravityToWorld = FQuat::Identity;

	const UCharacterMovementComponent* const Movement = GetMovement();
	if (Movement && Movement->HasCustomGravity())
	{
		const FVector Up = (-Movement->GetGravityDirection()).GetSafeNormal();
		if (!Up.IsNearlyZero())
		{
			OutUp = Up;
			OutWorldToGravity = Movement->GetWorldToGravityTransform();
			OutGravityToWorld = Movement->GetGravityToWorldTransform();
		}
	}
}

UAnimMontage* UEGTurnInPlaceComponent::GetRootMotionMontage() const
{
	const ACharacter* const Character = GetCharacter();
	const UAnimInstance* const AnimInstance = CachedAnimInstance.Get();
	if (!Character || !AnimInstance || !Character->IsPlayingNetworkedRootMotionMontage())
	{
		return nullptr;
	}
	const FAnimMontageInstance* const Instance = AnimInstance->GetRootMotionMontageInstance();
	return Instance ? Instance->Montage.Get() : nullptr;
}

FEGTurnInPlaceCurves UEGTurnInPlaceComponent::GetCurves() const
{
	if (WantsPseudoAnimState())
	{
		FEGTurnInPlaceCurves Curves;
		if (TurnSet && PseudoState != EEGTurnPseudoState::Idle)
		{
			const UAnimSequence* const Sequence = FEGTurnInPlaceSolver::GetTurnAnimation(GetEntry(), PseudoNode, PseudoState == EEGTurnPseudoState::Recovery);
			if (Sequence)
			{
				const float Time = static_cast<float>(PseudoNode.AnimTime);
				Curves.RemainingTurnYaw = Sequence->EvaluateCurveData(TurnSet->Curves.RemainingTurnYaw, Time);
				Curves.TurnYawWeight = Sequence->EvaluateCurveData(TurnSet->Curves.TurnYawWeight, Time);
				Curves.PauseTurnInPlace = Sequence->EvaluateCurveData(TurnSet->Curves.PauseTurnInPlace, Time);
				Curves.LockTurnInPlace = Sequence->EvaluateCurveData(TurnSet->Curves.LockTurnInPlace, Time);
			}
		}
		return Curves;
	}

	return CachedCurves;
}

void UEGTurnInPlaceComponent::RefreshCurveCache()
{
	UAnimInstance* const AnimInstance = CachedAnimInstance.Get();
	CachedCurves = (AnimInstance && bHasAnimSource)
		? IEGTurnInPlaceAnimSource::Execute_GetTurnInPlaceCurves(AnimInstance)
		: FEGTurnInPlaceCurves();
}

bool UEGTurnInPlaceComponent::IsTurningInPlace() const
{
	return HasValidData() && GetCurves().IsTurning();
}

// ---------------------------------------------------------------------------------------------
// Stance and state
// ---------------------------------------------------------------------------------------------

FGameplayTag UEGTurnInPlaceComponent::ResolveStanceKey() const
{
	if (!TurnSet)
	{
		return FGameplayTag();
	}

	const FGameplayTag Explicit = GetStanceTag();
	if (Explicit.IsValid())
	{
		return TurnSet->Stances.Contains(Explicit) ? Explicit : FGameplayTag();
	}
	if (TurnSet->Stances.Num() == 0)
	{
		return FGameplayTag();
	}

	FGameplayTagContainer StateTags;
	for (const TWeakObjectPtr<UObject>& Provider : StateProviders)
	{
		if (UObject* const Object = Provider.Get())
		{
			StateTags.AppendTags(IEGAnimStateProvider::Execute_GetAnimStateTags(Object));
		}
	}
	return TurnSet->FindStanceKey(StateTags);
}

FGameplayTag UEGTurnInPlaceComponent::GetStanceKey() const
{
	// Once per frame, so the movement hook and the anim graph agree on the stance. Marked resolved
	// (as Default) before resolving: a provider or override that asks this component for its state
	// would otherwise recurse forever.
	if (CachedStanceFrame != GFrameCounter)
	{
		CachedStanceFrame = GFrameCounter;
		CachedStanceKey = FGameplayTag();
		CachedStanceKey = ResolveStanceKey();
	}
	return CachedStanceKey;
}

const FEGTurnInPlaceEntry& UEGTurnInPlaceComponent::GetEntry() const
{
	return TurnSet ? TurnSet->GetEntry(GetStanceKey()) : UEGTurnInPlaceSet::GetEmptyEntry();
}

EEGTurnInPlaceState UEGTurnInPlaceComponent::GetEnabledState(EEGTurnStateReason& OutReason) const
{
	if (!HasValidData())
	{
		OutReason = EEGTurnStateReason::NoData;
		return EEGTurnInPlaceState::Locked;
	}

	const FEGTurnInPlaceEntry& Entry = GetEntry();
	const AActor* const Owner = GetOwner();

	FEGTurnInPlaceSolver::FStateInputs In;
	In.EntryState = Entry.State;
#if !UE_BUILD_SHIPPING
	In.ConsoleOverride = EGTurnInPlaceCVars::Override.GetValueOnGameThread();
#endif
	for (const FRequest& Request : Requests)
	{
		(Request.bLock ? In.bLockRequested : In.bPauseRequested) = true;
	}
	In.Curves = GetCurves();

	if (const UEGAnimEventComponent* const Events = Owner->FindComponentByClass<UEGAnimEventComponent>())
	{
		In.bPauseWindowOpen = Events->IsWindowActive(TAG_EGTurnInPlace_Pause);
		In.bLockWindowOpen = Events->IsWindowActive(TAG_EGTurnInPlace_Lock);
	}

	if (const UAnimMontage* const Montage = GetRootMotionMontage())
	{
		In.MontageOverride = ShouldIgnoreMontage(Montage)
			? EEGTurnInPlaceOverride::Default
			: FEGTurnInPlaceSolver::EvaluateRootMotionMontage(Entry.Montages, Montage);
	}

	// Soft collision pushes are root motion sources too, but being jostled must not pause turning.
	In.bHasRootMotionSources = EGSoftCollision::HasGameplayRootMotionSources(*GetMovement());

	if (const UEGMovementModifierComponent* const Modifiers = Owner->FindComponentByClass<UEGMovementModifierComponent>())
	{
		In.bRotationLockedByModifier = Modifiers->HasModifiers() && FMath::IsNearlyZero(Modifiers->GetCombinedModifier().RotationRateScale);
	}

	return FEGTurnInPlaceSolver::ResolveState(In, OutReason);
}

FGameplayTag UEGTurnInPlaceComponent::GetStanceTag_Implementation() const
{
	return FGameplayTag();
}

FGameplayTag UEGTurnInPlaceComponent::GetTurnModeTag_Implementation() const
{
	const UCharacterMovementComponent* const Movement = GetMovement();
	return Movement && Movement->bOrientRotationToMovement ? FGameplayTag(TAG_EGTurnInPlace_Mode_Movement) : FGameplayTag(TAG_EGTurnInPlace_Mode_Strafe);
}

bool UEGTurnInPlaceComponent::ShouldIgnoreMontage_Implementation(const UAnimMontage* Montage) const
{
	return false;
}

bool UEGTurnInPlaceComponent::CanAbortTurn_Implementation() const
{
	return true;
}

// ---------------------------------------------------------------------------------------------
// Requests
// ---------------------------------------------------------------------------------------------

FEGTurnInPlaceRequestHandle UEGTurnInPlaceComponent::PushRequest(bool bLock, FName Reason)
{
	NextRequestId = (NextRequestId == MAX_int32) ? 1 : NextRequestId + 1;
	FRequest& Request = Requests.AddDefaulted_GetRef();
	Request.Id = NextRequestId;
	Request.bLock = bLock;
	Request.Reason = Reason;

	FEGTurnInPlaceRequestHandle Handle;
	Handle.Id = NextRequestId;
	return Handle;
}

FEGTurnInPlaceRequestHandle UEGTurnInPlaceComponent::PushPause(FName Reason)
{
	return PushRequest(false, Reason);
}

FEGTurnInPlaceRequestHandle UEGTurnInPlaceComponent::PushLock(FName Reason)
{
	return PushRequest(true, Reason);
}

void UEGTurnInPlaceComponent::RemoveRequest(FEGTurnInPlaceRequestHandle& Handle)
{
	const int32 Id = Handle.Id;
	if (Id != 0)
	{
		Requests.RemoveAll([Id](const FRequest& Request) { return Request.Id == Id; });
	}
	Handle.Invalidate();
}

// ---------------------------------------------------------------------------------------------
// Rotation
// ---------------------------------------------------------------------------------------------

void UEGTurnInPlaceComponent::SetOwnerRotation(const FRotator& NewRotation)
{
	AActor* const Owner = GetOwner();
	USceneComponent* const Root = Owner ? Owner->GetRootComponent() : nullptr;
	if (!Root)
	{
		return;
	}

	const FRotator Current = Root->GetComponentRotation();
	if (Current.Equals(NewRotation, 1.e-4f))
	{
		return;
	}

	// A yaw-only rotation of an upright capsule cannot collide: skip the sweep SetActorRotation does.
	if (FMath::IsNearlyEqual(Current.Pitch, NewRotation.Pitch, 1.e-3) && FMath::IsNearlyEqual(Current.Roll, NewRotation.Roll, 1.e-3))
	{
		Root->SetWorldRotation(NewRotation, false, nullptr, ETeleportType::None);
	}
	else
	{
		Owner->SetActorRotation(NewRotation);
	}
}

void UEGTurnInPlaceComponent::ApplyYaw(const FRotator& Base, float AddYaw, const FQuat& WorldToGravity, const FQuat& GravityToWorld)
{
	// Upright to gravity at Base's heading plus AddYaw. Under normal gravity this also clears any
	// pitch or roll left from a tilted base.
	const double GravityYaw = (GravityToWorld * Base.Quaternion()).Rotator().Yaw + AddYaw;
	SetOwnerRotation((WorldToGravity * FRotator(0.0, GravityYaw, 0.0).Quaternion()).Rotator());
}

void UEGTurnInPlaceComponent::RunTurnInPlace(const FRotator& Current, const FRotator& Desired, EEGTurnInPlaceState State, bool bSimulatedProxy)
{
	FVector Up;
	FQuat WorldToGravity;
	FQuat GravityToWorld;
	GetGravity(Up, WorldToGravity, GravityToWorld);

	const FEGTurnInPlaceEntry& Entry = GetEntry();
	const FGameplayTag ModeTag = GetTurnModeTag();
	const FEGTurnAngles* const Angles = Entry.GetAngles(ModeTag);
	if (!Angles)
	{
		ReportProblem(FString::Printf(TEXT("The turn set has no angles for mode %s."), *ModeTag.ToString()));
	}

	FEGTurnInPlaceSolver::FOffsetInputs Inputs;
	Inputs.Current = Current;
	Inputs.Desired = Desired;
	Inputs.Up = Up;
	Inputs.State = State;
	Inputs.Curves = GetCurves();
	Inputs.MaxTurnAngle = Angles ? Angles->MaxTurnAngle : 0.0f;
	Inputs.bSimulatedProxy = bSimulatedProxy;

	const float Turn = FEGTurnInPlaceSolver::UpdateOffset(Inputs, MoveState);
	if (!bSimulatedProxy)
	{
		ApplyYaw(Current, Turn, WorldToGravity, GravityToWorld);
	}
}

void UEGTurnInPlaceComponent::UpdateReplicatedOffset()
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || GetNetMode() == NM_Standalone)
	{
		return;
	}

	// Only when it changed beyond what 16 bits can tell apart.
	if (!FMath::IsNearlyEqual(SimulatedOffset.Get(), static_cast<float>(FRotator::NormalizeAxis(MoveState.TurnOffset)), 0.01f))
	{
		SimulatedOffset.Set(MoveState.TurnOffset);
		MARK_PROPERTY_DIRTY_FROM_NAME(UEGTurnInPlaceComponent, SimulatedOffset, this);
	}
}

void UEGTurnInPlaceComponent::OnRep_SimulatedOffset()
{
	if (GetOwner() && GetOwner()->GetLocalRole() == ROLE_SimulatedProxy)
	{
		// The curve value stays: local simulation carries on from the new offset.
		MoveState.TurnOffset = SimulatedOffset.Get();
	}
}

void UEGTurnInPlaceComponent::UpdateLastInputDirection(const UCharacterMovementComponent* Movement)
{
	const ACharacter* const Character = GetCharacter();
	const UWorld* const World = GetWorld();
	if (!Character || !Movement || !Movement->UpdatedComponent || !World)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	FEGTurnInPlaceSolver::FLastInputInputs In;
	In.bOrientToMovement = Movement->bOrientRotationToMovement;
	In.bRootMotion = EGSoftCollision::HasGameplayRootMotion(*Character);
	if (In.bRootMotion)
	{
		LastRootMotionTime = Now;
	}
	In.Forward = Movement->UpdatedComponent->GetForwardVector();
	In.Velocity = Movement->Velocity;
	In.Acceleration = Movement->GetCurrentAcceleration();
	In.bMovingOnGround = Movement->IsMovingOnGround();
	FQuat WorldToGravity;
	FQuat GravityToWorld;
	GetGravity(In.Up, WorldToGravity, GravityToWorld);
	const float MaxAcceleration = Movement->GetMaxAcceleration();
	In.AnalogInput = MaxAcceleration > UE_SMALL_NUMBER ? FMath::Clamp(static_cast<float>(In.Acceleration.Size() / MaxAcceleration), 0.0f, 1.0f) : 0.0f;
	In.MaxSpeed = Movement->GetMaxSpeed();
	In.TimeSinceRootMotion = Now - LastRootMotionTime;
	In.bBotControlled = Character->IsBotControlled();
	In.ControlForward = Character->GetControlRotation().Vector();

	FEGTurnInPlaceSolver::UpdateLastInputDirection(LastInputDirection, In);
}

// ---------------------------------------------------------------------------------------------
// Hooks
// ---------------------------------------------------------------------------------------------

bool UEGTurnInPlaceComponent::HandleFaceRotation(FRotator NewControlRotation, float DeltaTime)
{
	bFaceHookSeen = true;
	FramesWithoutFaceHook = 0;

	if (GetTurnMethod() != EEGTurnMethod::FaceRotation)
	{
		return false;
	}
	if (!HasValidData())
	{
		MoveState = FEGTurnInPlaceMoveState();
		return false;
	}

	// DeltaTime 0 is movement following a rotating base, not the controller asking to turn. Treating
	// it as a turn request fights the base and feeds back into the camera.
	if (DeltaTime == 0.0f)
	{
		return false;
	}

	ACharacter* const Character = GetCharacter();
	EEGTurnStateReason Reason;
	const EEGTurnInPlaceState State = GetEnabledState(Reason);

	if (State == EEGTurnInPlaceState::Paused)
	{
		MoveState = FEGTurnInPlaceMoveState();
		UpdateReplicatedOffset();
		return false;
	}

	const FRotator Current = Character->GetActorRotation();
	if (State == EEGTurnInPlaceState::Locked || IsStationary())
	{
		RunTurnInPlace(Current, NewControlRotation, State, false);
		UpdateReplicatedOffset();
		return true;
	}

	// Moving: clear the standing state (so a later stop starts clean) and ease the offset away
	// instead of snapping to the camera.
	MoveState.TurnOffset = 0.0f;
	MoveState.CurveValue = 0.0f;
	MoveState.bLastCurveValid = false;

	if (!Character->bUseControllerRotationPitch)
	{
		NewControlRotation.Pitch = Current.Pitch;
	}
	if (!Character->bUseControllerRotationYaw)
	{
		NewControlRotation.Yaw = Current.Yaw;
	}
	else
	{
		MoveState.InterpOutAlpha = FMath::FInterpConstantTo(MoveState.InterpOutAlpha, 1.0f, DeltaTime, GetEntry().MovingInterpOutRate);
		NewControlRotation.Yaw = FQuat::Slerp(Current.Quaternion(), NewControlRotation.Quaternion(), MoveState.InterpOutAlpha).GetNormalized().Rotator().Yaw;
	}
	if (!Character->bUseControllerRotationRoll)
	{
		NewControlRotation.Roll = Current.Roll;
	}

	Character->SetActorRotation(NewControlRotation);
	UpdateReplicatedOffset();
	return true;
}

bool UEGTurnInPlaceComponent::HandlePhysicsRotation(UCharacterMovementComponent* Movement, float DeltaTime)
{
	bPhysicsHookSeen = true;
	FramesWithoutPhysicsHook = 0;

	// Tracked on every movement step, root motion included, so no CalcVelocity override is needed.
	UpdateLastInputDirection(Movement);

	ACharacter* const Character = GetCharacter();
	if (!Movement || !Character || GetTurnMethod() != EEGTurnMethod::PhysicsRotation)
	{
		return false;
	}

	// The engine's own early-outs.
	if (!Movement->UpdatedComponent || (!Character->Controller && !Movement->bRunPhysicsWithNoController))
	{
		return false;
	}

	if (!HasValidData())
	{
		MoveState = FEGTurnInPlaceMoveState();
		return false;
	}

	EEGTurnStateReason Reason;
	const EEGTurnInPlaceState State = GetEnabledState(Reason);
	if (State == EEGTurnInPlaceState::Paused)
	{
		MoveState = FEGTurnInPlaceMoveState();
		UpdateReplicatedOffset();
		return false;
	}

	const FRotator Current = Movement->UpdatedComponent->GetComponentRotation();
	if (State == EEGTurnInPlaceState::Locked)
	{
		RunTurnInPlace(Current, Current, State, false);
		UpdateReplicatedOffset();
		return true;
	}

	if (!IsStationary())
	{
		// Moving: the movement component rotates; the offset is measured again once stopped.
		MoveState = FEGTurnInPlaceMoveState();
		UpdateReplicatedOffset();
		return false;
	}

	if (Movement->bOrientRotationToMovement)
	{
		if (bRotateToLastInputDirection && !LastInputDirection.IsNearlyZero())
		{
			RunTurnInPlace(Current, LastInputDirection.Rotation(), State, false);
		}
	}
	else if (Movement->bUseControllerDesiredRotation)
	{
		const AController* Controller = Character->Controller;
		if (!Controller && Movement->bRunPhysicsWithNoController)
		{
			Controller = Cast<AController>(Character->GetOwner());
		}
		if (Controller)
		{
			RunTurnInPlace(Current, Controller->GetDesiredRotation(), State, false);
		}
	}

	UpdateReplicatedOffset();
	return true;
}

bool UEGTurnInPlaceComponent::ComputeOrientToMovementRotation(const UCharacterMovementComponent* Movement, const FRotator& CurrentRotation, FRotator& OutRotation) const
{
	const ACharacter* const Character = GetCharacter();
	if (!bRotateToLastInputDirection || !Movement || !Character || Character->IsBotControlled())
	{
		// Bots keep the engine's path-following orientation.
		return false;
	}
	if (Movement->GetCurrentAcceleration().SizeSquared() >= UE_KINDA_SMALL_NUMBER || LastInputDirection.IsNearlyZero())
	{
		return false;
	}
	OutRotation = LastInputDirection.Rotation();
	return true;
}

// ---------------------------------------------------------------------------------------------
// Anim graph
// ---------------------------------------------------------------------------------------------

FEGTurnInPlaceGraphInput UEGTurnInPlaceComponent::MakeGraphInput() const
{
	FEGTurnInPlaceGraphInput In;
	if (!HasValidData())
	{
		return In;
	}

	In.bValid = true;
	In.Set = TurnSet;
	In.StanceKey = GetStanceKey();
	In.CurveNames = TurnSet->Curves;

	const FEGTurnInPlaceEntry& Entry = GetEntry();
	EEGTurnStateReason Reason;
	In.State = GetEnabledState(Reason);
	In.TurnOffset = MoveState.TurnOffset;
	In.bIsTurning = GetCurves().IsTurning();
	In.StepIndex = FEGTurnInPlaceSolver::SelectStep(Entry, In.TurnOffset, In.bTurnRight);
	In.ModeTag = GetTurnModeTag();
	In.bIsStrafing = !GetMovement()->bOrientRotationToMovement;
	In.bAbortTurn = In.State != EEGTurnInPlaceState::Enabled && CanAbortTurn();
	In.bPseudo = WantsPseudoAnimState();

	if (const FEGTurnAngles* const Angles = Entry.GetAngles(In.ModeTag))
	{
		In.bHasAngles = true;
		In.Angles = *Angles;
		// Only while Enabled: a proxy keeps its replicated offset while paused, and wanting to turn
		// while aborting would flip the graph between Turn and Abort.
		In.bWantsToTurn = In.State == EEGTurnInPlaceState::Enabled && Entry.StepSizes.Num() > 0 && FMath::Abs(In.TurnOffset) >= Angles->MinTurnAngle;
	}

	if (In.bWantsToTurn && !Entry.GetTurnSequence(In.StepIndex, In.bTurnRight))
	{
		ReportProblem(FString::Printf(TEXT("The turn set has no %s sequence for step %d."), In.bTurnRight ? TEXT("right") : TEXT("left"), In.StepIndex));
	}
	return In;
}

FGameplayTagContainer UEGTurnInPlaceComponent::GetAnimStateTags_Implementation() const
{
	FGameplayTagContainer Tags;
	if (!HasValidData())
	{
		return Tags;
	}

	EEGTurnStateReason Reason;
	switch (GetEnabledState(Reason))
	{
	case EEGTurnInPlaceState::Locked: Tags.AddTag(TAG_EGTurnInPlace_State_Locked); break;
	case EEGTurnInPlaceState::Paused: Tags.AddTag(TAG_EGTurnInPlace_State_Paused); break;
	default: break;
	}

	if (GetCurves().IsTurning())
	{
		Tags.AddTag(TAG_EGTurnInPlace_State_Turning);
		Tags.AddTag(MoveState.TurnOffset > 0.0f ? TAG_EGTurnInPlace_State_Right : TAG_EGTurnInPlace_State_Left);
	}
	return Tags;
}

// ---------------------------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------------------------

void UEGTurnInPlaceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	RefreshCurveCache();

#if !UE_BUILD_SHIPPING
	TickSelfCheck();
#endif

	if (!HasValidData())
	{
		return;
	}

	// Anim root motion skips PhysicsRotation: record the facing it leaves (and the time, for the
	// grace period) here, or Orient To Movement would turn back to where it faced before.
	if (EGSoftCollision::HasGameplayRootMotion(*GetCharacter()))
	{
		LastRootMotionTime = GetWorld()->GetTimeSeconds();
		if (const UCharacterMovementComponent* const Movement = GetMovement(); Movement && Movement->UpdatedComponent && Movement->bOrientRotationToMovement)
		{
			LastInputDirection = Movement->UpdatedComponent->GetForwardVector();
		}
	}

	if (WantsPseudoAnimState())
	{
		TickPseudoStates(DeltaTime);
	}
	TickSimulatedProxy();

#if !UE_BUILD_SHIPPING
	DrawDebug();
#endif
}

void UEGTurnInPlaceComponent::TickPseudoStates(float DeltaTime)
{
	const FEGTurnInPlaceGraphInput Input = MakeGraphInput();
	if (PseudoState == EEGTurnPseudoState::Turn && Input.bIsTurning)
	{
		bPseudoTurnedThisEntry = true;
	}

	FEGTurnInPlaceGraphOutput Output;
	FEGTurnInPlaceSolver::ComputeGraphOutput(Input, bPseudoTurnedThisEntry, PseudoNode.bReachedAnimEnd, Output);

	const EEGTurnPseudoState Before = PseudoState;
	FEGTurnInPlaceSolver::AdvancePseudoState(PseudoState, PseudoNode, Input, Output, DeltaTime);
	if (PseudoState != Before)
	{
		// As the graph's Setup functions do on every state entry.
		bPseudoTurnedThisEntry = false;
	}
}

void UEGTurnInPlaceComponent::TickSimulatedProxy()
{
	const AActor* const Owner = GetOwner();
	if (!bSimulateCurvesOnProxies || !Owner || Owner->GetLocalRole() != ROLE_SimulatedProxy || !IsStationary())
	{
		return;
	}

	// Consume the replicated offset with the local turn animation between server updates.
	EEGTurnStateReason Reason;
	RunTurnInPlace(FRotator::ZeroRotator, FRotator::ZeroRotator, GetEnabledState(Reason), true);
}

void UEGTurnInPlaceComponent::ReportProblem(const FString& Message) const
{
#if !UE_BUILD_SHIPPING
	const UWorld* const World = GetWorld();
	if (bReportSetupProblems && World && World->IsGameWorld())
	{
		// Once per owner class and message.
		const AActor* const Owner = GetOwner();
		EGAnimDebug::WarnOnce(Owner ? static_cast<const UObject*>(Owner->GetClass()) : this, FString::Printf(TEXT("Turn in place: %s"), *Message));
	}
#endif
}

void UEGTurnInPlaceComponent::TickSelfCheck()
{
	const ACharacter* const Character = GetCharacter();
	const UCharacterMovementComponent* const Movement = GetMovement();
	if (!bReportSetupProblems || !Character || !Movement || !TurnSet || Character->GetLocalRole() == ROLE_SimulatedProxy)
	{
		return;
	}

	// Only count frames where the engine would have called the hook.
	const bool bMovementActive = Movement->MovementMode != MOVE_None && !EGSoftCollision::HasGameplayRootMotion(*Character);
	switch (GetTurnMethod())
	{
	case EEGTurnMethod::FaceRotation:
		// Player controllers call FaceRotation every frame; AI only when its facing differs, so an
		// idle, aligned AI would be reported falsely.
		if (!bFaceHookSeen && Character->IsPlayerControlled() && ++FramesWithoutFaceHook > MissingHookFrames)
		{
			ReportProblem(TEXT("ACharacter::FaceRotation is not forwarded. Override it and call EGTurnInPlace::HandleFaceRotation first (see Docs/TurnInPlace.md)."));
		}
		break;
	case EEGTurnMethod::PhysicsRotation:
		if (!bPhysicsHookSeen && bMovementActive && (Character->Controller || Movement->bRunPhysicsWithNoController) && ++FramesWithoutPhysicsHook > MissingHookFrames)
		{
			ReportProblem(TEXT("UCharacterMovementComponent::PhysicsRotation is not forwarded. Use a movement component that calls EGTurnInPlace::HandlePhysicsRotation first (see Docs/TurnInPlace.md)."));
		}
		break;
	default:
		break;
	}

	// An owning client predicts moves; without the saved-move mixin combined moves lose part of the turn.
	if (Character->GetLocalRole() == ROLE_AutonomousProxy && GetNetMode() == NM_Client && (bFaceHookSeen || bPhysicsHookSeen)
		&& !bSavedMoveHookSeen && ++ClientFramesWithoutSavedMove > MissingHookFrames)
	{
		ReportProblem(TEXT("The movement component's saved move does not use TEGTurnInPlaceSavedMove; combined moves will lose part of the turn. Allocate FEGTurnInPlacePredictionData (or your own TEGTurnInPlacePredictionData) in GetPredictionData_Client."));
	}
}

void UEGTurnInPlaceComponent::DrawDebug() const
{
#if ENABLE_DRAW_DEBUG
	const bool bShowOffset = EGTurnInPlaceCVars::Debug.GetValueOnGameThread() > 0;
	const bool bShowState = EGTurnInPlaceCVars::DebugState.GetValueOnGameThread() > 0;
	const ACharacter* const Character = GetCharacter();
	if ((!bShowOffset && !bShowState) || !Character || !GEngine)
	{
		return;
	}

	const TCHAR* const Role = Character->HasAuthority() ? TEXT("Server") : (Character->GetLocalRole() == ROLE_AutonomousProxy ? TEXT("Client") : TEXT("Simulated"));
	const uint64 Key = Character->GetUniqueID() + 0x7E1F;
	const FRandomStream ColorStream(static_cast<int32>(Key));
	const FColor Color(ColorStream.RandRange(64, 255), ColorStream.RandRange(64, 255), ColorStream.RandRange(64, 255));

	FString Text = FString::Printf(TEXT("[%s] %s"), Role, *GetNameSafe(Character));
	if (bShowOffset)
	{
		Text += FString::Printf(TEXT("  offset %.1f"), MoveState.TurnOffset);
	}
	if (bShowState)
	{
		EEGTurnStateReason Reason;
		const EEGTurnInPlaceState State = GetEnabledState(Reason);
		Text += FString::Printf(TEXT("  %s (%s)"),
			*StaticEnum<EEGTurnInPlaceState>()->GetNameStringByValue(static_cast<int64>(State)),
			*StaticEnum<EEGTurnStateReason>()->GetNameStringByValue(static_cast<int64>(Reason)));
	}
	GEngine->AddOnScreenDebugMessage(static_cast<int32>(Key), 0.0f, Color, Text);

	// Arrows once per character: the server skips characters a client owns and draws itself.
	if (!bShowOffset || (Character->HasAuthority() && Character->GetRemoteRole() == ROLE_AutonomousProxy))
	{
		return;
	}

	UWorld* const World = GetWorld();
	const UCapsuleComponent* const Capsule = Character->GetCapsuleComponent();
	const FVector Feet = Character->GetActorLocation() - Character->GetActorUpVector() * (Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 0.0f);
	DrawDebugDirectionalArrow(World, Feet, Feet + Character->GetActorForwardVector() * 200.0f, 40.0f, FColor(199, 10, 143), false, -1.0f, 0, 2.0f);
	if (const AController* const Controller = Character->GetController())
	{
		DrawDebugDirectionalArrow(World, Feet, Feet + FRotator(0.0, Controller->GetControlRotation().Yaw, 0.0).Vector() * 200.0f, 40.0f, FColor::Black, false, -1.0f, 0, 2.0f);
	}
	const FVector Target = (Character->GetActorRotation() + FRotator(0.0, MoveState.TurnOffset, 0.0)).GetNormalized().Vector();
	DrawDebugDirectionalArrow(World, Feet, Feet + Target * 200.0f, 40.0f, FColor(38, 199, 0), false, -1.0f, 0, 2.0f);
#endif
}
