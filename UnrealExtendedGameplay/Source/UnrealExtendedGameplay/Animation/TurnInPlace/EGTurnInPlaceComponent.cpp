// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGTurnInPlaceComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "EGTurnInPlaceAnimSource.h"
#include "EGTurnInPlaceMovementSource.h"
#include "EGTurnInPlaceSet.h"
#include "EGTurnInPlaceSolver.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "UnrealExtendedGameplay/Animation/Debug/EGAnimDebug.h"
#include "UnrealExtendedGameplay/Animation/EGAnimationTags.h"

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
		TEXT("1: show each character's resolved turn in place state, why, and what drives the rotation."),
		ECVF_Cheat);
}

namespace
{
	/** Frames a hook may go missing (while the engine would call it) before it is reported. */
	constexpr int32 MissingHookFrames = 120;

	/** StrafeDirect's ease towards the camera once moving. A rate of 0 snaps: interpolating at 0 never arrives. */
	float AdvanceInterpOut(float Alpha, float DeltaTime, float Rate)
	{
		return Rate > 0.0f ? FMath::FInterpConstantTo(Alpha, 1.0f, DeltaTime, Rate) : 1.0f;
	}
}

// ---------------------------------------------------------------------------------------------
// Driver tick
// ---------------------------------------------------------------------------------------------

void FEGTurnInPlaceDriverTickFunction::ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	FActorComponentTickFunction::ExecuteTickHelper(Target, /*bTickInEditor=*/ false, DeltaTime, TickType, [this](float DilatedTime)
	{
		if (bSnapshot)
		{
			Target->TickSnapshot();
		}
		else
		{
			Target->TickDriver(DilatedTime);
		}
	});
}

FString FEGTurnInPlaceDriverTickFunction::DiagnosticMessage()
{
	const TCHAR* const Suffix = bSnapshot ? TEXT("[TurnInPlaceSnapshot]") : TEXT("[TurnInPlaceDriver]");
	return (Target ? Target->GetFullName() : FString(TEXT("<null>"))) + Suffix;
}

// ---------------------------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------------------------

UEGTurnInPlaceComponent::UEGTurnInPlaceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	DriverTick.bCanEverTick = true;
	DriverTick.bStartWithTickEnabled = true;
	DriverTick.TickGroup = TG_PrePhysics;

	SnapshotTick.bCanEverTick = true;
	SnapshotTick.bStartWithTickEnabled = true;
	SnapshotTick.TickGroup = TG_PrePhysics;
	SnapshotTick.bSnapshot = true;

	SetIsReplicatedByDefault(true);

	MovementSource = CreateDefaultSubobject<UEGTurnInPlaceCharacterSource>(TEXT("MovementSource"));
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
	CachedMesh = GetSource()->GetMesh(GetOwner());
}

void UEGTurnInPlaceComponent::RegisterComponentTickFunctions(bool bRegister)
{
	Super::RegisterComponentTickFunctions(bRegister);

	if (bRegister)
	{
		if (SetupActorComponentTickFunction(&DriverTick))
		{
			DriverTick.Target = this;
		}
		if (SetupActorComponentTickFunction(&SnapshotTick))
		{
			SnapshotTick.Target = this;
		}
	}
	else
	{
		if (DriverTick.IsTickFunctionRegistered())
		{
			DriverTick.UnRegisterTickFunction();
		}
		if (SnapshotTick.IsTickFunctionRegistered())
		{
			SnapshotTick.UnRegisterTickFunction();
		}
	}
}

void UEGTurnInPlaceComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* const Owner = GetOwner();
	const UEGTurnInPlaceMovementSource* const Source = GetSource();
	CachedMesh = Source->GetMesh(Owner);

	// Snapshot, then movement, then driver, then mesh: the driver replaces only the movement's own
	// turn, and the animation sees the final rotation.
	DriverTick.AddPrerequisite(this, SnapshotTick);
	if (UActorComponent* const Movement = Source->GetMovementComponent(Owner))
	{
		Movement->PrimaryComponentTick.AddPrerequisite(this, SnapshotTick);
		DriverTick.AddPrerequisite(Movement, Movement->PrimaryComponentTick);
	}
	UpdateSnapshotPrerequisite();

	if (USkeletalMeshComponent* const Mesh = GetMesh())
	{
		Mesh->PrimaryComponentTick.AddPrerequisite(this, DriverTick);

		// The main tick runs after the mesh, so the curve copy is taken once its animation is done.
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
		ReportProblem(TEXT("No skeletal mesh found (the movement source's GetMesh); turn in place is off."));
	}

	FEGTurnInPlaceMovementInfo Info;
	if (!GatherMovementInfo(Info))
	{
		ReportProblem(FString::Printf(TEXT("The movement source %s cannot read %s (Character Movement needs an ACharacter); turn in place is off."),
			*GetNameSafe(Source->GetClass()), *GetNameSafe(Owner ? Owner->GetClass() : nullptr)));
	}
	else
	{
		LastInputDirection = Info.Rotation.Quaternion().GetForwardVector();
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
		Mesh->PrimaryComponentTick.RemovePrerequisite(this, DriverTick);
	}
	if (UActorComponent* const Movement = GetSource()->GetMovementComponent(GetOwner()))
	{
		Movement->PrimaryComponentTick.RemovePrerequisite(this, SnapshotTick);
	}
	if (AActor* const Controller = SnapshotController.Get())
	{
		SnapshotTick.RemovePrerequisite(Controller, Controller->PrimaryActorTick);
	}
	SnapshotController.Reset();
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

	if (!AnimInstance && !WantsPseudoAnimState())
	{
		ReportProblem(TEXT("The mesh has no anim instance; turn in place is off."));
	}

	// Blueprint components exist by now on spawned characters.
	RefreshStateProviders();
}

void UEGTurnInPlaceComponent::RefreshStateProviders() const
{
	StateProviders.Reset();
	const AActor* const Owner = GetOwner();
	ProviderComponentCount = Owner ? Owner->GetComponents().Num() : INDEX_NONE;
	if (!Owner)
	{
		return;
	}

	if (Owner->Implements<UEGAnimStateProvider>())
	{
		StateProviders.Add(const_cast<AActor*>(Owner));
	}
	TInlineComponentArray<UActorComponent*> Components(Owner);
	for (UActorComponent* Component : Components)
	{
		// Not this component: its own state tags depend on the state being resolved.
		if (Component && Component != this && Component->Implements<UEGAnimStateProvider>())
		{
			StateProviders.Add(Component);
		}
	}
}

const FGameplayTagContainer& UEGTurnInPlaceComponent::GetStateTags() const
{
	// Once per frame. Marked gathered first: a provider that asks this component for its state
	// would otherwise recurse.
	if (CachedStateTagsFrame != GFrameCounter)
	{
		CachedStateTagsFrame = GFrameCounter;
		CachedStateTags.Reset();

		// Components added at runtime (a movement modifier added on demand) are picked up here.
		const AActor* const Owner = GetOwner();
		if (Owner && Owner->GetComponents().Num() != ProviderComponentCount)
		{
			RefreshStateProviders();
		}

		FGameplayTagContainer Tags;
		for (const TWeakObjectPtr<UObject>& Provider : StateProviders)
		{
			if (UObject* const Object = Provider.Get())
			{
				Tags.AppendTags(IEGAnimStateProvider::Execute_GetAnimStateTags(Object));
			}
		}
		CachedStateTags = MoveTemp(Tags);
	}
	return CachedStateTags;
}

// ---------------------------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------------------------

const UEGTurnInPlaceMovementSource* UEGTurnInPlaceComponent::GetSource() const
{
	// Cleared in the details panel: fall back to the stateless default.
	return MovementSource ? MovementSource.Get() : GetDefault<UEGTurnInPlaceCharacterSource>();
}

bool UEGTurnInPlaceComponent::GatherMovementInfo(FEGTurnInPlaceMovementInfo& OutInfo) const
{
	const AActor* const Owner = GetOwner();
	if (!Owner || !GetSource()->GatherMovement(Owner, OutInfo))
	{
		OutInfo = FEGTurnInPlaceMovementInfo();
		return false;
	}
	return OutInfo.bValid;
}

bool UEGTurnInPlaceComponent::GetMovementInfo(FEGTurnInPlaceMovementInfo& OutInfo) const
{
	return GatherMovementInfo(OutInfo);
}

USkeletalMeshComponent* UEGTurnInPlaceComponent::GetMesh() const
{
	return CachedMesh.Get();
}

bool UEGTurnInPlaceComponent::WantsPseudoAnimState() const
{
	return GetNetMode() == NM_DedicatedServer && DedicatedServerAnimMode == EEGTurnServerAnimMode::Pseudo;
}

bool UEGTurnInPlaceComponent::HasValidData(const FEGTurnInPlaceMovementInfo& Info) const
{
	return TurnSet && Info.bValid && (CachedAnimInstance.IsValid() || WantsPseudoAnimState());
}

bool UEGTurnInPlaceComponent::HasValidData() const
{
	FEGTurnInPlaceMovementInfo Info;
	return GatherMovementInfo(Info) && HasValidData(Info);
}

bool UEGTurnInPlaceComponent::IsStationary(const FEGTurnInPlaceMovementInfo& Info)
{
	return Info.Velocity.IsNearlyZero();
}

EEGTurnMethod UEGTurnInPlaceComponent::GetTurnMethod() const
{
	FEGTurnInPlaceMovementInfo Info;
	return GatherMovementInfo(Info) ? Info.TurnMethod : EEGTurnMethod::None;
}

EEGTurnRotationDriver UEGTurnInPlaceComponent::ResolveDriver(const FEGTurnInPlaceMovementInfo& Info) const
{
	if (RotationDriver != EEGTurnRotationDriver::Auto)
	{
		return RotationDriver;
	}
	switch (Info.TurnMethod)
	{
	case EEGTurnMethod::FaceRotation: return bFaceHookSeen ? EEGTurnRotationDriver::Hooks : EEGTurnRotationDriver::Component;
	case EEGTurnMethod::PhysicsRotation: return bPhysicsHookSeen ? EEGTurnRotationDriver::Hooks : EEGTurnRotationDriver::Component;
	default: return EEGTurnRotationDriver::Component;
	}
}

EEGTurnRotationDriver UEGTurnInPlaceComponent::GetActiveRotationDriver() const
{
	FEGTurnInPlaceMovementInfo Info;
	GatherMovementInfo(Info);
	return ResolveDriver(Info);
}

FEGTurnInPlaceCurves UEGTurnInPlaceComponent::GetCurves() const
{
	if (WantsPseudoAnimState())
	{
		if (!TurnSet || PseudoState == EEGTurnPseudoState::Idle)
		{
			return FEGTurnInPlaceCurves();
		}
		return FEGTurnInPlaceSolver::EvaluateTurnCurves(GetEntry(), PseudoNode, PseudoState == EEGTurnPseudoState::Recovery, TurnSet->Curves);
	}
	return CachedCurves;
}

void UEGTurnInPlaceComponent::RefreshCurveCache()
{
	UAnimInstance* const AnimInstance = CachedAnimInstance.Get();
	if (!AnimInstance || !TurnSet)
	{
		CachedCurves = FEGTurnInPlaceCurves();
		return;
	}

	if (bHasAnimSource)
	{
		CachedCurves = IEGTurnInPlaceAnimSource::Execute_GetTurnInPlaceCurves(AnimInstance);
		return;
	}

	// Any anim instance: the curves it evaluated this frame (complete by now, the mesh ticked first).
	const FEGTurnInPlaceCurveNames& Names = TurnSet->Curves;
	CachedCurves.RemainingTurnYaw = AnimInstance->GetCurveValue(Names.RemainingTurnYaw);
	CachedCurves.TurnYawWeight = AnimInstance->GetCurveValue(Names.TurnYawWeight);
	CachedCurves.PauseTurnInPlace = AnimInstance->GetCurveValue(Names.PauseTurnInPlace);
	CachedCurves.LockTurnInPlace = AnimInstance->GetCurveValue(Names.LockTurnInPlace);
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
	return TurnSet->FindStanceKey(GetStateTags());
}

FGameplayTag UEGTurnInPlaceComponent::GetStanceKey() const
{
	// Once per frame, so the movement and the anim graph agree on the stance. Marked resolved (as
	// Default) before resolving: a provider or override that asks this component for its state
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
	FEGTurnInPlaceMovementInfo Info;
	GatherMovementInfo(Info);
	return ResolveEnabledState(Info, OutReason);
}

EEGTurnInPlaceState UEGTurnInPlaceComponent::ResolveEnabledState(const FEGTurnInPlaceMovementInfo& Info, EEGTurnStateReason& OutReason) const
{
	if (!HasValidData(Info))
	{
		OutReason = EEGTurnStateReason::NoData;
		return EEGTurnInPlaceState::Locked;
	}

	const FEGTurnInPlaceEntry& Entry = GetEntry();

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

	// Soft links: anim event windows and movement modifiers publish tags; nothing here knows them.
	const FGameplayTagContainer& Tags = GetStateTags();
	In.bPauseWindowOpen = Tags.HasTag(TAG_EGTurnInPlace_Pause);
	In.bLockWindowOpen = Tags.HasTag(TAG_EGTurnInPlace_Lock);
	In.bRotationLockedByModifier = Tags.HasTag(TAG_EGMovement_RotationLocked);

	if (const UAnimMontage* const Montage = Info.RootMotionMontage.Get())
	{
		In.MontageOverride = ShouldIgnoreMontage(Montage)
			? EEGTurnInPlaceOverride::Default
			: FEGTurnInPlaceSolver::EvaluateRootMotionMontage(Entry.Montages, Montage);
	}
	In.bHasRootMotionSources = Info.bHasRootMotionSources;

	return FEGTurnInPlaceSolver::ResolveState(In, OutReason);
}

FGameplayTag UEGTurnInPlaceComponent::GetStanceTag_Implementation() const
{
	return FGameplayTag();
}

FGameplayTag UEGTurnInPlaceComponent::GetTurnModeTag_Implementation() const
{
	FEGTurnInPlaceMovementInfo Info;
	GatherMovementInfo(Info);
	return Info.bOrientToMovement ? FGameplayTag(TAG_EGTurnInPlace_Mode_Movement) : FGameplayTag(TAG_EGTurnInPlace_Mode_Strafe);
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

void UEGTurnInPlaceComponent::ResetTurn()
{
	MoveState = FEGTurnInPlaceMoveState();
	bHasDriverRotation = false;
	SettleTimer = 0.0f;
	bWantsSettle = false;

	FEGTurnInPlaceMovementInfo Info;
	if (GatherMovementInfo(Info))
	{
		LastInputDirection = Info.Rotation.Quaternion().GetForwardVector();
	}
	UpdateReplicatedOffset();
}

// ---------------------------------------------------------------------------------------------
// Rotation
// ---------------------------------------------------------------------------------------------

void UEGTurnInPlaceComponent::ApplyRotation(const FRotator& NewRotation)
{
	GetSource()->ApplyRotation(GetOwner(), NewRotation);
}

FRotator UEGTurnInPlaceComponent::RunTurnInPlace(const FRotator& Current, const FRotator& Desired, EEGTurnInPlaceState State, bool bSimulatedProxy, const FVector& Up)
{
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
	if (bSimulatedProxy)
	{
		return Current;
	}

	const FRotator NewRotation = FEGTurnInPlaceSolver::MakeUprightRotation(Current, Turn, Up);
	ApplyRotation(NewRotation);
	return NewRotation;
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

void UEGTurnInPlaceComponent::UpdateLastInputDirection(const FEGTurnInPlaceMovementInfo& Info)
{
	const UWorld* const World = GetWorld();
	if (!World || !Info.bValid)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	FEGTurnInPlaceSolver::FLastInputInputs In;
	In.bOrientToMovement = Info.bOrientToMovement;
	In.bRootMotion = Info.bAnimRootMotion || Info.bHasRootMotionSources;
	if (In.bRootMotion)
	{
		LastRootMotionTime = Now;
	}
	In.Forward = Info.Rotation.Quaternion().GetForwardVector();
	In.Velocity = Info.Velocity;
	In.Acceleration = Info.Acceleration;
	In.bMovingOnGround = Info.bMovingOnGround;
	In.Up = Info.Up;
	In.AnalogInput = Info.MaxAcceleration > UE_SMALL_NUMBER ? FMath::Clamp(static_cast<float>(Info.Acceleration.Size() / Info.MaxAcceleration), 0.0f, 1.0f) : 0.0f;
	In.MaxSpeed = Info.MaxSpeed;
	In.TimeSinceRootMotion = Now - LastRootMotionTime;
	In.bBotControlled = Info.bBotControlled;
	In.ControlForward = Info.ControlRotation.Vector();

	FEGTurnInPlaceSolver::UpdateLastInputDirection(LastInputDirection, In);
}

// ---------------------------------------------------------------------------------------------
// Hooks
// ---------------------------------------------------------------------------------------------

bool UEGTurnInPlaceComponent::HandleFaceRotation(FRotator NewControlRotation, float DeltaTime)
{
	bFaceHookSeen = true;
	FramesWithoutFaceHook = 0;

	// Deactivated (or its tick turned off): the character rotates as if turn in place did not exist.
	ACharacter* const Character = Cast<ACharacter>(GetOwner());
	FEGTurnInPlaceMovementInfo Info;
	if (!IsComponentTickEnabled() || !Character || !GatherMovementInfo(Info) || Info.TurnMethod != EEGTurnMethod::FaceRotation
		|| ResolveDriver(Info) != EEGTurnRotationDriver::Hooks)
	{
		return false;
	}
	if (!HasValidData(Info))
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

	EEGTurnStateReason Reason;
	const EEGTurnInPlaceState State = ResolveEnabledState(Info, Reason);

	if (State == EEGTurnInPlaceState::Paused)
	{
		MoveState = FEGTurnInPlaceMoveState();
		UpdateReplicatedOffset();
		return false;
	}

	const FRotator Current = Character->GetActorRotation();
	if (State == EEGTurnInPlaceState::Locked || IsStationary(Info))
	{
		RunTurnInPlace(Current, NewControlRotation, State, false, Info.Up);
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
		MoveState.InterpOutAlpha = AdvanceInterpOut(MoveState.InterpOutAlpha, DeltaTime, GetEntry().MovingInterpOutRate);
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

	FEGTurnInPlaceMovementInfo Info;
	if (!IsComponentTickEnabled() || !Movement || !GatherMovementInfo(Info))
	{
		return false;
	}

	// Tracked on every movement step, root motion included, so no CalcVelocity override is needed.
	UpdateLastInputDirection(Info);

	const ACharacter* const Character = Movement->GetCharacterOwner();
	if (!Character || Info.TurnMethod != EEGTurnMethod::PhysicsRotation || ResolveDriver(Info) != EEGTurnRotationDriver::Hooks)
	{
		return false;
	}

	// The engine's own early-outs.
	if (!Movement->UpdatedComponent || (!Character->Controller && !Movement->bRunPhysicsWithNoController))
	{
		return false;
	}

	if (!HasValidData(Info))
	{
		MoveState = FEGTurnInPlaceMoveState();
		return false;
	}

	EEGTurnStateReason Reason;
	const EEGTurnInPlaceState State = ResolveEnabledState(Info, Reason);
	if (State == EEGTurnInPlaceState::Paused)
	{
		MoveState = FEGTurnInPlaceMoveState();
		UpdateReplicatedOffset();
		return false;
	}

	const FRotator Current = Movement->UpdatedComponent->GetComponentRotation();
	if (State == EEGTurnInPlaceState::Locked)
	{
		RunTurnInPlace(Current, Current, State, false, Info.Up);
		UpdateReplicatedOffset();
		return true;
	}

	if (!IsStationary(Info))
	{
		// Moving: the movement component rotates; the offset is measured again once stopped.
		MoveState = FEGTurnInPlaceMoveState();
		UpdateReplicatedOffset();
		return false;
	}

	if (Info.bOrientToMovement)
	{
		if (bRotateToLastInputDirection && !LastInputDirection.IsNearlyZero())
		{
			RunTurnInPlace(Current, LastInputDirection.Rotation(), State, false, Info.Up);
		}
	}
	else if (Info.bHasDesiredRotation)
	{
		RunTurnInPlace(Current, Info.DesiredRotation, State, false, Info.Up);
	}

	UpdateReplicatedOffset();
	return true;
}

bool UEGTurnInPlaceComponent::ComputeOrientToMovementRotation(const UCharacterMovementComponent* Movement, const FRotator& CurrentRotation, FRotator& OutRotation) const
{
	const ACharacter* const Character = Movement ? Movement->GetCharacterOwner() : nullptr;
	if (!IsComponentTickEnabled() || !bRotateToLastInputDirection || !Character || Character->IsBotControlled())
	{
		// Off with the component, as the other hooks (its last input direction is no longer kept up
		// to date). Bots keep the engine's path-following orientation.
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
// Component driver
// ---------------------------------------------------------------------------------------------

void UEGTurnInPlaceComponent::SetDriverRotation(const FRotator& Rotation)
{
	DriverRotation = Rotation;
	bHasDriverRotation = true;
}

void UEGTurnInPlaceComponent::TickSnapshot()
{
	const AActor* const Owner = GetOwner();
	const USceneComponent* const Root = Owner ? Owner->GetRootComponent() : nullptr;
	if (Root)
	{
		PreMovementRotation = Root->GetComponentRotation();
		PreMovementFrame = GFrameCounter;
	}
}

void UEGTurnInPlaceComponent::UpdateSnapshotPrerequisite()
{
	// After the controller, whose tick handles input (where gameplay often turns the character).
	const APawn* const Pawn = Cast<APawn>(GetOwner());
	AActor* const Controller = Pawn ? Pawn->GetController() : nullptr;
	if (Controller == SnapshotController.Get())
	{
		return;
	}
	if (AActor* const Previous = SnapshotController.Get())
	{
		SnapshotTick.RemovePrerequisite(Previous, Previous->PrimaryActorTick);
	}
	if (Controller)
	{
		SnapshotTick.AddPrerequisite(Controller, Controller->PrimaryActorTick);
	}
	SnapshotController = Controller;
}

FRotator UEGTurnInPlaceComponent::GetDriverStartRotation(const FEGTurnInPlaceMovementInfo& Info) const
{
	if (!bHasDriverRotation)
	{
		return Info.Rotation;
	}

	FRotator Start;
	if (Info.TurnMethod == EEGTurnMethod::FaceRotation)
	{
		// FaceRotation runs in the controller's tick and snaps the yaw to the control rotation. That
		// snap is what turn in place replaces; a facing other than the control rotation came from
		// gameplay after it, and stays.
		const bool bControllerSnap = Info.bHasDesiredRotation
			&& FMath::Abs(FEGTurnInPlaceSolver::YawDeltaAroundUp(Info.Rotation, Info.DesiredRotation, Info.Up)) < 0.1f;
		if (!bControllerSnap)
		{
			return Info.Rotation;
		}
		Start = DriverRotation;
	}
	else
	{
		// The rotation just before the movement turned the owner: what gameplay set (cover, abilities,
		// teleports) is kept; the movement's own turn towards the desired facing is not.
		Start = PreMovementFrame == GFrameCounter ? PreMovementRotation : DriverRotation;
	}

	// The movement applied the rotation of what the owner stands on; carry it over.
	if (Info.Base && Info.Base.Get() == DriverBase.Get())
	{
		const FQuat BaseDelta = Info.BaseRotation.Quaternion() * DriverBaseRotation.Quaternion().Inverse();
		if (!BaseDelta.IsIdentity(1.e-6))
		{
			Start = (BaseDelta * Start.Quaternion()).Rotator();
		}
	}
	return Start;
}

void UEGTurnInPlaceComponent::TickDriver(float DeltaTime)
{
	const AActor* const Owner = GetOwner();
	FEGTurnInPlaceMovementInfo Info;

	UpdateSnapshotPrerequisite();

	// Off with the component (Deactivate, tick disabled); simulated proxies follow the replicated
	// rotation; the hooks drive when they are in use.
	if (!Owner || !IsComponentTickEnabled() || Owner->GetLocalRole() == ROLE_SimulatedProxy || !GatherMovementInfo(Info)
		|| ResolveDriver(Info) != EEGTurnRotationDriver::Component)
	{
		bHasDriverRotation = false;
		return;
	}

	UpdateLastInputDirection(Info);

	if (!HasValidData(Info))
	{
		MoveState = FEGTurnInPlaceMoveState();
		bHasDriverRotation = false;
		return;
	}

	// Root motion or no one to rotate it: whatever moved the owner this frame keeps it.
	if (!Info.bCanRotate || Info.bAnimRootMotion)
	{
		bHasDriverRotation = false;
		return;
	}

	// Not from where the movement turned the owner this frame: its turn towards the desired facing
	// is what turn in place replaces.
	const FRotator Current = GetDriverStartRotation(Info);
	DriverBase = Info.Base.Get();
	DriverBaseRotation = Info.BaseRotation;

	EEGTurnStateReason Reason;
	const EEGTurnInPlaceState State = ResolveEnabledState(Info, Reason);
	if (State == EEGTurnInPlaceState::Paused)
	{
		MoveState = FEGTurnInPlaceMoveState();
		bHasDriverRotation = false;
		UpdateReplicatedOffset();
		return;
	}

	if (State == EEGTurnInPlaceState::Locked)
	{
		SetDriverRotation(RunTurnInPlace(Current, Current, State, false, Info.Up));
		UpdateReplicatedOffset();
		return;
	}

	if (!IsStationary(Info))
	{
		FRotator Result = Info.Rotation;
		if (Info.TurnMethod == EEGTurnMethod::FaceRotation)
		{
			// As the FaceRotation hook: clear the standing state and ease away from the held-back
			// facing instead of snapping to the camera.
			MoveState.TurnOffset = 0.0f;
			MoveState.CurveValue = 0.0f;
			MoveState.bLastCurveValid = false;
			MoveState.InterpOutAlpha = AdvanceInterpOut(MoveState.InterpOutAlpha, DeltaTime, GetEntry().MovingInterpOutRate);
			if (MoveState.InterpOutAlpha < 1.0f)
			{
				Result = FQuat::Slerp(Current.Quaternion(), Info.Rotation.Quaternion(), MoveState.InterpOutAlpha).GetNormalized().Rotator();
				ApplyRotation(Result);
			}
		}
		else
		{
			// The movement rotates; the offset is measured again once stopped.
			MoveState = FEGTurnInPlaceMoveState();
		}
		SetDriverRotation(Result);
		UpdateReplicatedOffset();
		return;
	}

	// Standing: hold back from the desired facing while the turn animation closes the gap.
	if (Info.bOrientToMovement)
	{
		if (bRotateToLastInputDirection && !LastInputDirection.IsNearlyZero())
		{
			SetDriverRotation(RunTurnInPlace(Current, LastInputDirection.Rotation(), State, false, Info.Up));
		}
		else
		{
			SetDriverRotation(Info.Rotation);
		}
	}
	else if (Info.bHasDesiredRotation)
	{
		SetDriverRotation(RunTurnInPlace(Current, Info.DesiredRotation, State, false, Info.Up));
	}
	else
	{
		SetDriverRotation(Info.Rotation);
	}
	UpdateReplicatedOffset();
}

// ---------------------------------------------------------------------------------------------
// Anim graph
// ---------------------------------------------------------------------------------------------

FEGTurnInPlaceGraphInput UEGTurnInPlaceComponent::MakeGraphInput() const
{
	FEGTurnInPlaceGraphInput In;
	FEGTurnInPlaceMovementInfo Info;
	if (!GatherMovementInfo(Info) || !HasValidData(Info))
	{
		return In;
	}

	In.bValid = true;
	In.Set = TurnSet;
	In.StanceKey = GetStanceKey();
	In.CurveNames = TurnSet->Curves;

	const FEGTurnInPlaceEntry& Entry = GetEntry();
	EEGTurnStateReason Reason;
	// Off with the component (Deactivate, tick disabled), the hooks and driver no longer consume the
	// offset: as Paused, a turn in progress stops and no new one starts that nothing would turn.
	In.State = IsComponentTickEnabled() ? ResolveEnabledState(Info, Reason) : EEGTurnInPlaceState::Paused;
	In.TurnOffset = MoveState.TurnOffset;
	In.bIsTurning = GetCurves().IsTurning();
	In.StepIndex = FEGTurnInPlaceSolver::SelectStep(Entry, In.TurnOffset, In.bTurnRight);
	In.ModeTag = GetTurnModeTag();
	In.bIsStrafing = !Info.bOrientToMovement;
	In.bIsMoving = !IsStationary(Info);
	In.bAbortTurn = In.State != EEGTurnInPlaceState::Enabled && CanAbortTurn();
	In.bPseudo = WantsPseudoAnimState();
	In.bWantsSettle = bWantsSettle && In.State == EEGTurnInPlaceState::Enabled;

	if (const FEGTurnAngles* const Angles = Entry.GetAngles(In.ModeTag))
	{
		In.bHasAngles = true;
		In.Angles = *Angles;
		// Only while Enabled: a proxy keeps its replicated offset while paused, and wanting to turn
		// while aborting would flip the graph between Turn and Abort.
		In.bWantsToTurn = In.State == EEGTurnInPlaceState::Enabled && Entry.StepSizes.Num() > 0 && FMath::Abs(In.TurnOffset) >= Angles->MinTurnAngle;
	}

	if (In.bWantsToTurn)
	{
		const UAnimSequence* const Sequence = Entry.GetTurnSequence(In.StepIndex, In.bTurnRight);
		if (!Sequence && !Entry.Procedural.bTurnWithoutAnimation)
		{
			ReportProblem(FString::Printf(TEXT("The turn set has no %s sequence for step %d and procedural turns are off."), In.bTurnRight ? TEXT("right") : TEXT("left"), In.StepIndex));
		}
		else if (Sequence && In.bPseudo && !UEGTurnInPlaceSet::HasTurnCurves(Sequence, TurnSet->Curves))
		{
			ReportProblem(FString::Printf(TEXT("%s has no turn curves; Pseudo servers read only curves. Run the Turn In Place Curves modifier on it."), *GetNameSafe(Sequence)));
		}
	}
	return In;
}

FGameplayTagContainer UEGTurnInPlaceComponent::GetAnimStateTags_Implementation() const
{
	FGameplayTagContainer Tags;
	FEGTurnInPlaceMovementInfo Info;
	if (!GatherMovementInfo(Info) || !HasValidData(Info))
	{
		return Tags;
	}

	EEGTurnStateReason Reason;
	switch (ResolveEnabledState(Info, Reason))
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

	FEGTurnInPlaceMovementInfo Info;
	const bool bHasInfo = GatherMovementInfo(Info);

#if !UE_BUILD_SHIPPING
	if (bHasInfo)
	{
		TickSelfCheck(Info);
	}
#endif

	if (!bHasInfo || !HasValidData(Info))
	{
		SettleTimer = 0.0f;
		bWantsSettle = false;
		return;
	}

	// Nothing rotates the owner (no controller, movement off), so neither the hooks nor the driver
	// consume an offset left from before, and the graph would start turn after turn on it. The
	// FaceRotation hook still runs with movement off: the controller calls it. Simulated proxies
	// never rotate and keep their replicated offset.
	const bool bFaceHookRuns = Info.TurnMethod == EEGTurnMethod::FaceRotation && Info.bHasDesiredRotation
		&& ResolveDriver(Info) == EEGTurnRotationDriver::Hooks;
	if (!Info.bCanRotate && !bFaceHookRuns && GetOwnerRole() != ROLE_SimulatedProxy)
	{
		MoveState = FEGTurnInPlaceMoveState();
		UpdateReplicatedOffset();
	}

	// Anim root motion skips PhysicsRotation: record the facing it leaves (and the time, for the
	// grace period) here, or Orient To Movement would turn back to where it faced before.
	if (Info.bAnimRootMotion || Info.bHasRootMotionSources)
	{
		LastRootMotionTime = GetWorld()->GetTimeSeconds();
		if (Info.bOrientToMovement)
		{
			LastInputDirection = Info.Rotation.Quaternion().GetForwardVector();
		}
	}

	UpdateSettle(Info, DeltaTime);

	if (WantsPseudoAnimState())
	{
		TickPseudoStates(DeltaTime);
	}
	TickSimulatedProxy(Info);

#if !UE_BUILD_SHIPPING
	DrawDebug();
#endif
}

void UEGTurnInPlaceComponent::UpdateSettle(const FEGTurnInPlaceMovementInfo& Info, float DeltaTime)
{
	const FEGTurnInPlaceEntry& Entry = GetEntry();
	const FEGTurnAngles* const Angles = Entry.GetAngles(GetTurnModeTag());

	EEGTurnStateReason Reason;
	const bool bCanSettle = Angles && IsStationary(Info) && !GetCurves().IsTurning()
		&& ResolveEnabledState(Info, Reason) == EEGTurnInPlaceState::Enabled;

	bWantsSettle = FEGTurnInPlaceSolver::UpdateSettleTimer(SettleTimer, Entry.Procedural, MoveState.TurnOffset,
		Angles ? Angles->MinTurnAngle : 0.0f, bCanSettle, DeltaTime);
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

	// As the node: locomotion takes over once the character moves.
	if (Input.bIsMoving || !Input.bValid)
	{
		Output.bAbortTurn = true;
		Output.bWantsToTurn = false;
		Output.bWantsRecovery = false;
	}

	const EEGTurnPseudoState Before = PseudoState;
	FEGTurnInPlaceSolver::AdvancePseudoState(PseudoState, PseudoNode, Input, Output, DeltaTime);
	if (PseudoState != Before)
	{
		// As the graph does on every state entry.
		bPseudoTurnedThisEntry = false;
	}
}

void UEGTurnInPlaceComponent::TickSimulatedProxy(const FEGTurnInPlaceMovementInfo& Info)
{
	const AActor* const Owner = GetOwner();
	if (!bSimulateCurvesOnProxies || !Owner || Owner->GetLocalRole() != ROLE_SimulatedProxy || !IsStationary(Info))
	{
		return;
	}

	// Consume the replicated offset with the local turn animation between server updates.
	EEGTurnStateReason Reason;
	RunTurnInPlace(FRotator::ZeroRotator, FRotator::ZeroRotator, ResolveEnabledState(Info, Reason), true, Info.Up);
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

void UEGTurnInPlaceComponent::TickSelfCheck(const FEGTurnInPlaceMovementInfo& Info)
{
	const AActor* const Owner = GetOwner();
	if (!bReportSetupProblems || !Owner || !TurnSet || Owner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		return;
	}

	const ACharacter* const Character = Cast<ACharacter>(Owner);
	const UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr;
	const EEGTurnRotationDriver Driver = ResolveDriver(Info);

	// Works without the hooks, but a remote player's own client is then not predicted (a
	// listen-server host's own character needs no prediction).
	const bool bHostOwnCharacter = Character && Character->HasAuthority() && Character->IsLocallyControlled();
	if (RotationDriver == EEGTurnRotationDriver::Auto && Driver == EEGTurnRotationDriver::Component && Movement
		&& Info.TurnMethod != EEGTurnMethod::None && GetNetMode() != NM_Standalone && Character->IsPlayerControlled() && !bHostOwnCharacter
		&& ++FramesDrivenUnpredicted > MissingHookFrames)
	{
		ReportProblem(TEXT("A player character in multiplayer turns from the component tick, which is not predicted. Forward the hooks and add TEGTurnInPlaceSavedMove (Docs/TurnInPlace.md), or set Rotation Driver to Component to accept it."));
	}

	// Only an explicit Hooks driver needs them: Auto falls back to the component.
	if (Movement && RotationDriver == EEGTurnRotationDriver::Hooks)
	{
		// Only count frames where the engine would have called the hook.
		const bool bMovementActive = Movement->MovementMode != MOVE_None && !Info.bAnimRootMotion && !Info.bHasRootMotionSources;
		switch (Info.TurnMethod)
		{
		case EEGTurnMethod::FaceRotation:
			// Player controllers call FaceRotation every frame; AI only when its facing differs, so an
			// idle, aligned AI would be reported falsely.
			if (!bFaceHookSeen && Character->IsPlayerControlled() && ++FramesWithoutFaceHook > MissingHookFrames)
			{
				ReportProblem(TEXT("Rotation Driver is Hooks but ACharacter::FaceRotation is not forwarded. Override it and call EGTurnInPlace::HandleFaceRotation first (see Docs/TurnInPlace.md)."));
			}
			break;
		case EEGTurnMethod::PhysicsRotation:
			if (!bPhysicsHookSeen && bMovementActive && (Character->Controller || Movement->bRunPhysicsWithNoController) && ++FramesWithoutPhysicsHook > MissingHookFrames)
			{
				ReportProblem(TEXT("Rotation Driver is Hooks but UCharacterMovementComponent::PhysicsRotation is not forwarded. Use a movement component that calls EGTurnInPlace::HandlePhysicsRotation first (see Docs/TurnInPlace.md)."));
			}
			break;
		default:
			break;
		}
	}

	// The offset asks for a turn but nothing plays one: no Turn In Place node (or recipe) in the
	// AnimBP, or its curves are scaled away before the output pose.
	if (HasValidData(Info))
	{
		const FEGTurnInPlaceEntry& Entry = GetEntry();
		const FEGTurnAngles* const Angles = Entry.GetAngles(GetTurnModeTag());
		EEGTurnStateReason Reason;
		const bool bWaiting = Angles && Entry.StepSizes.Num() > 0 && IsStationary(Info) && !GetCurves().IsTurning()
			&& FMath::Abs(MoveState.TurnOffset) >= Angles->MinTurnAngle && ResolveEnabledState(Info, Reason) == EEGTurnInPlaceState::Enabled;
		FramesWaitingForTurn = bWaiting ? FramesWaitingForTurn + 1 : 0;
		if (FramesWaitingForTurn > 3 * MissingHookFrames)
		{
			ReportProblem(TEXT("The offset has asked for a turn for seconds and no turn played. Add the Turn In Place node to the AnimBP (or the state machine recipe), and make sure nothing after it scales the RemainingTurnYaw / TurnYawWeight curves."));
		}
	}

	// An owning client predicts moves; without the saved-move mixin combined moves lose part of the turn.
	if (Character && Character->GetLocalRole() == ROLE_AutonomousProxy && GetNetMode() == NM_Client && Driver == EEGTurnRotationDriver::Hooks
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
	const AActor* const Owner = GetOwner();
	if ((!bShowOffset && !bShowState) || !Owner || !GEngine)
	{
		return;
	}

	const TCHAR* const Role = Owner->HasAuthority() ? TEXT("Server") : (Owner->GetLocalRole() == ROLE_AutonomousProxy ? TEXT("Client") : TEXT("Simulated"));
	const uint64 Key = Owner->GetUniqueID() + 0x7E1F;
	const FRandomStream ColorStream(static_cast<int32>(Key));
	const FColor Color(ColorStream.RandRange(64, 255), ColorStream.RandRange(64, 255), ColorStream.RandRange(64, 255));

	FString Text = FString::Printf(TEXT("[%s] %s"), Role, *GetNameSafe(Owner));
	if (bShowOffset)
	{
		Text += FString::Printf(TEXT("  offset %.1f"), MoveState.TurnOffset);
	}
	if (bShowState)
	{
		EEGTurnStateReason Reason;
		const EEGTurnInPlaceState State = GetEnabledState(Reason);
		Text += FString::Printf(TEXT("  %s (%s), %s"),
			*StaticEnum<EEGTurnInPlaceState>()->GetNameStringByValue(static_cast<int64>(State)),
			*StaticEnum<EEGTurnStateReason>()->GetNameStringByValue(static_cast<int64>(Reason)),
			*StaticEnum<EEGTurnRotationDriver>()->GetNameStringByValue(static_cast<int64>(GetActiveRotationDriver())));
		if (bWantsSettle)
		{
			Text += TEXT(", settling");
		}
	}
	GEngine->AddOnScreenDebugMessage(static_cast<int32>(Key), 0.0f, Color, Text);

	// Arrows once per character: the server skips characters a client owns and draws itself.
	if (!bShowOffset || (Owner->HasAuthority() && Owner->GetRemoteRole() == ROLE_AutonomousProxy))
	{
		return;
	}

	UWorld* const World = GetWorld();
	const FVector Feet = Owner->GetActorLocation() - Owner->GetActorUpVector() * Owner->GetSimpleCollisionHalfHeight();
	DrawDebugDirectionalArrow(World, Feet, Feet + Owner->GetActorForwardVector() * 200.0f, 40.0f, FColor(199, 10, 143), false, -1.0f, 0, 2.0f);
	if (const APawn* const Pawn = Cast<APawn>(Owner))
	{
		if (const AController* const Controller = Pawn->GetController())
		{
			DrawDebugDirectionalArrow(World, Feet, Feet + FRotator(0.0, Controller->GetControlRotation().Yaw, 0.0).Vector() * 200.0f, 40.0f, FColor::Black, false, -1.0f, 0, 2.0f);
		}
	}
	const FVector Target = (Owner->GetActorRotation() + FRotator(0.0, MoveState.TurnOffset, 0.0)).GetNormalized().Vector();
	DrawDebugDirectionalArrow(World, Feet, Feet + Target * 200.0f, 40.0f, FColor(38, 199, 0), false, -1.0f, 0, 2.0f);
#endif
}
