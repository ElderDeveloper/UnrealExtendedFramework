// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGSoftCollisionComponent.h"

#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Drivers/EGSoftCollisionDriver.h"
#include "Drivers/EGSoftCollisionDriver_CharacterForce.h"
#include "Drivers/EGSoftCollisionDriver_CharacterRootMotion.h"
#include "Drivers/EGSoftCollisionDriver_PawnMovement.h"
#include "Drivers/EGSoftCollisionDriver_PhysicsBody.h"
#include "EGSoftCollisionInterface.h"
#include "EGSoftCollisionProfile.h"
#include "EGSoftCollisionSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/UnrealNetwork.h"
#include "VisualLogger/VisualLogger.h"

namespace
{
	/** Requests per batch, client to server. A batch is one RPC. */
	constexpr int32 MaxRequestsPerBatch = 4;

	/** Requests the server reads from one batch. More is a misbehaving client. */
	constexpr int32 MaxRequestsAccepted = 8;

	/** A client timestamp this far below the last one seen is a timestamp reset. */
	constexpr float TimeStampResetThreshold = 1.0f;

	const TCHAR* BandToString(EEGSoftPushBand Band)
	{
		switch (Band)
		{
		case EEGSoftPushBand::Separation: return TEXT("Separation");
		case EEGSoftPushBand::Bump: return TEXT("Bump");
		default: return TEXT("None");
		}
	}
}

UEGSoftCollisionComponent::UEGSoftCollisionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// Requests, acks and cosmetic bumps travel through this component.
	SetIsReplicatedByDefault(true);
}

UEGSoftCollisionComponent* UEGSoftCollisionComponent::FindSoftCollisionComponent(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UEGSoftCollisionComponent>() : nullptr;
}

void UEGSoftCollisionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams OwnerParams;
	OwnerParams.bIsPushBased = true;
	OwnerParams.Condition = COND_OwnerOnly;
	DOREPLIFETIME_WITH_PARAMS_FAST(UEGSoftCollisionComponent, LastAcceptedSequence, OwnerParams);

	FDoRepLifetimeParams CosmeticParams;
	CosmeticParams.bIsPushBased = true;
	CosmeticParams.Condition = COND_SkipOwner;
	DOREPLIFETIME_WITH_PARAMS_FAST(UEGSoftCollisionComponent, CosmeticBump, CosmeticParams);
}

// =====================================================================
// Lifetime
// =====================================================================

void UEGSoftCollisionComponent::BeginPlay()
{
	Super::BeginPlay();

	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn)
	{
		UE_LOG(LogEGSoftCollision, Warning, TEXT("%s: Soft Collision needs a pawn owner; it stays inactive."), *GetPathName());
		return;
	}

	CachedPawn = Pawn;
	CachedCharacterMovement = Cast<UCharacterMovementComponent>(Pawn->GetMovementComponent());
	BeginPlayTime = GetGameTime();
	bSoftCollisionEnabled = bStartEnabled;

	CachePushPrimitive();
	RefreshFilter();
	ConfigureCollision();

	Pawn->ReceiveControllerChangedDelegate.AddDynamic(this, &UEGSoftCollisionComponent::HandleControllerChanged);
	UpdateControlState(true);

	if (UEGSoftCollisionSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->Register(*this);
	}
}

void UEGSoftCollisionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UEGSoftCollisionSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->Unregister(*this);
	}
	bFrameUpdatesRegistered = false;

	CancelActivePush();
	ResetPredictionState();

	if (bMovementBound)
	{
		if (ACharacter* Character = Cast<ACharacter>(GetPawn()))
		{
			Character->OnCharacterMovementUpdated.RemoveDynamic(this, &UEGSoftCollisionComponent::HandleCharacterMovementUpdated);
		}
		bMovementBound = false;
	}

	if (APawn* Pawn = GetPawn())
	{
		Pawn->ReceiveControllerChangedDelegate.RemoveDynamic(this, &UEGSoftCollisionComponent::HandleControllerChanged);
	}

	RestoreCollision();
	Super::EndPlay(EndPlayReason);
}

// =====================================================================
// Settings
// =====================================================================

void UEGSoftCollisionComponent::SetSoftCollisionEnabled(bool bEnable)
{
	if (bSoftCollisionEnabled == bEnable)
	{
		return;
	}
	bSoftCollisionEnabled = bEnable;
	if (!bEnable)
	{
		CancelActivePush();
	}
}

void UEGSoftCollisionComponent::SetProfile(UEGSoftCollisionProfile* NewProfile)
{
	Profile = NewProfile;
	RefreshFilter();
}

void UEGSoftCollisionComponent::RefreshFilter()
{
	EffectiveFilter = Filter;
	UEGSoftCollisionProfile* OverrideProfile = nullptr;

	AActor* Owner = GetOwner();
	if (Owner && Owner->GetClass()->ImplementsInterface(UEGSoftCollisionInterface::StaticClass()))
	{
		EffectiveFilter = IEGSoftCollisionInterface::Execute_GetSoftCollisionFilter(Owner, Filter);
		OverrideProfile = IEGSoftCollisionInterface::Execute_GetSoftCollisionProfileOverride(Owner);
	}

	UEGSoftCollisionProfile* NewProfile = OverrideProfile ? OverrideProfile : Profile.Get();
	if (!NewProfile)
	{
		NewProfile = GetMutableDefault<UEGSoftCollisionProfile>();
	}

	if (NewProfile != ResolvedProfile || (!PlayerDriver && !AIDriver))
	{
		CancelActivePush();
		ResolvedProfile = NewProfile;
		ResolveDrivers();
	}
}

bool UEGSoftCollisionComponent::IsPushActive() const
{
	return Active.bActive && GetGameTime() < Active.EndTime;
}

UEGSoftCollisionProfile* UEGSoftCollisionComponent::GetEffectiveProfile() const
{
	if (ResolvedProfile)
	{
		return ResolvedProfile;
	}
	return Profile ? Profile.Get() : GetMutableDefault<UEGSoftCollisionProfile>();
}

void UEGSoftCollisionComponent::ResolveDrivers()
{
	const UEGSoftCollisionProfile* EffectiveProfile = GetEffectiveProfile();
	PlayerDriver = EffectiveProfile && EffectiveProfile->PlayerDriver ? EffectiveProfile->PlayerDriver.Get() : PickDefaultDriver(true);
	AIDriver = EffectiveProfile && EffectiveProfile->AIDriver ? EffectiveProfile->AIDriver.Get() : PickDefaultDriver(false);
	bControlStateValid = false;
}

UEGSoftCollisionDriver* UEGSoftCollisionComponent::PickDefaultDriver(bool bForPlayer) const
{
	const APawn* Pawn = GetPawn();
	if (!Pawn)
	{
		return nullptr;
	}

	if (Pawn->IsA<ACharacter>() && CachedCharacterMovement.IsValid())
	{
		return bForPlayer
			? static_cast<UEGSoftCollisionDriver*>(GetMutableDefault<UEGSoftCollisionDriver_CharacterRootMotion>())
			: static_cast<UEGSoftCollisionDriver*>(GetMutableDefault<UEGSoftCollisionDriver_CharacterForce>());
	}

	const UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Pawn->GetRootComponent());
	if (Root && Root->IsSimulatingPhysics())
	{
		return GetMutableDefault<UEGSoftCollisionDriver_PhysicsBody>();
	}

	if (Pawn->GetMovementComponent())
	{
		return GetMutableDefault<UEGSoftCollisionDriver_PawnMovement>();
	}

	// Pushes others but is never pushed itself.
	return nullptr;
}

void UEGSoftCollisionComponent::CachePushPrimitive()
{
	const APawn* Pawn = GetPawn();
	if (!Pawn)
	{
		return;
	}

	UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Pawn->GetRootComponent());
	if (!Primitive)
	{
		Primitive = Pawn->FindComponentByClass<UCapsuleComponent>();
	}
	PushPrimitive = Primitive;
}

void UEGSoftCollisionComponent::ConfigureCollision()
{
	if (!bConfigureCollision || bCollisionConfigured)
	{
		return;
	}

	if (UPrimitiveComponent* Primitive = PushPrimitive.Get())
	{
		SavedResponse = Primitive->GetCollisionResponseToChannel(SoftChannel);
		Primitive->SetCollisionResponseToChannel(SoftChannel, SoftResponse);
		bCollisionConfigured = true;
	}
}

void UEGSoftCollisionComponent::RestoreCollision()
{
	if (!bCollisionConfigured)
	{
		return;
	}

	if (UPrimitiveComponent* Primitive = PushPrimitive.Get())
	{
		Primitive->SetCollisionResponseToChannel(SoftChannel, SavedResponse);
	}
	bCollisionConfigured = false;
}

// =====================================================================
// Control
// =====================================================================

bool UEGSoftCollisionComponent::ComputePlayerControlled() const
{
	const APawn* Pawn = GetPawn();
	if (!Pawn)
	{
		return false;
	}
	if (const AController* Controller = Pawn->GetController())
	{
		return Controller->IsPlayerController();
	}
	// Simulated proxies have no controller; the player state tells.
	return Pawn->IsPlayerControlled();
}

void UEGSoftCollisionComponent::UpdateControlState(bool bForce)
{
	const bool bNowPlayer = ComputePlayerControlled();
	if (!bForce && bControlStateValid && bNowPlayer == bPlayerControlled)
	{
		return;
	}

	const bool bChanged = bControlStateValid && bNowPlayer != bPlayerControlled;
	bPlayerControlled = bNowPlayer;
	bControlStateValid = true;

	if (bChanged)
	{
		// A different driver takes over: nothing of the old control survives.
		CancelActivePush();
		ResetPredictionState();
	}
	UpdateMovementBinding();
}

void UEGSoftCollisionComponent::HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
	UpdateControlState(false);
}

UEGSoftCollisionDriver* UEGSoftCollisionComponent::GetActiveDriver() const
{
	return bPlayerControlled ? PlayerDriver.Get() : AIDriver.Get();
}

EEGSoftCollisionDriverRole UEGSoftCollisionComponent::GetEffectiveRole() const
{
	const UEGSoftCollisionDriver* Driver = GetActiveDriver();
	EEGSoftCollisionDriverRole Role = Driver ? Driver->GetRole() : EEGSoftCollisionDriverRole::ServerAuthority;
	if (Role == EEGSoftCollisionDriverRole::Predicted && UEGSoftCollisionSubsystem::IsForceServerOnly())
	{
		Role = EEGSoftCollisionDriverRole::ServerAuthority;
	}
	return Role;
}

bool UEGSoftCollisionComponent::IsScanner() const
{
	const APawn* Pawn = GetPawn();
	const UEGSoftCollisionDriver* Driver = GetActiveDriver();
	if (!Pawn || !Driver || !bSoftCollisionEnabled || !Driver->SupportsPawn(*Pawn))
	{
		return false;
	}

	switch (GetEffectiveRole())
	{
	case EEGSoftCollisionDriverRole::Predicted:
	case EEGSoftCollisionDriverRole::OwnerAuthority:
		return Pawn->IsLocallyControlled() || (Pawn->HasAuthority() && !Pawn->GetController());
	case EEGSoftCollisionDriverRole::ServerAuthority:
	default:
		return Pawn->HasAuthority();
	}
}

bool UEGSoftCollisionComponent::IsPredictingClient() const
{
	const APawn* Pawn = GetPawn();
	return Pawn && Pawn->GetLocalRole() == ROLE_AutonomousProxy && GetEffectiveRole() == EEGSoftCollisionDriverRole::Predicted;
}

// =====================================================================
// Snapshot and results
// =====================================================================

void UEGSoftCollisionComponent::WriteSnapshot(FEGSoftBody& OutBody, FEGSoftCollisionSnapshotInfo& OutInfo)
{
	// Controllers and player states replicate separately; checking each frame catches either.
	UpdateControlState(false);

	const APawn* Pawn = GetPawn();
	const UEGSoftCollisionProfile* EffectiveProfile = GetEffectiveProfile();
	if (!Pawn || !EffectiveProfile)
	{
		OutBody.bEnabled = false;
		OutInfo = FEGSoftCollisionSnapshotInfo();
		return;
	}

	const UPrimitiveComponent* Primitive = PushPrimitive.Get();
	OutBody.Location = Primitive ? Primitive->GetComponentLocation() : Pawn->GetActorLocation();
	const FVector Velocity = Pawn->GetVelocity();
	OutBody.Velocity = FVector2D(Velocity.X, Velocity.Y);

	const FEGSoftCollisionShape& Shape = EffectiveProfile->Shape;
	EEGSoftCollisionShapeType Type = Shape.Type;
	float Radius = Shape.Radius;
	float HalfHeight = Shape.HalfHeight;
	FVector2D BoxHalfExtent = Shape.BoxHalfExtent;
	if (Shape.bFromPrimitive && Primitive)
	{
		if (const UCapsuleComponent* Capsule = Cast<UCapsuleComponent>(Primitive))
		{
			Type = EEGSoftCollisionShapeType::Cylinder;
			Radius = Capsule->GetScaledCapsuleRadius();
			HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		}
		else if (const USphereComponent* Sphere = Cast<USphereComponent>(Primitive))
		{
			Type = EEGSoftCollisionShapeType::Cylinder;
			Radius = Sphere->GetScaledSphereRadius();
			HalfHeight = Radius;
		}
		else if (const UBoxComponent* Box = Cast<UBoxComponent>(Primitive))
		{
			Type = EEGSoftCollisionShapeType::Box;
			const FVector Extent = Box->GetScaledBoxExtent();
			BoxHalfExtent = FVector2D(Extent.X, Extent.Y);
			HalfHeight = static_cast<float>(Extent.Z);
		}
	}

	const float Scale = FMath::Max(EffectiveProfile->RadiusScale, 0.01f);
	OutBody.Shape = Type;
	OutBody.Radius = Radius * Scale;
	OutBody.HalfHeight = HalfHeight;
	OutBody.BoxHalfExtent = BoxHalfExtent * Scale;
	if (Type == EEGSoftCollisionShapeType::Box)
	{
		const double Yaw = FMath::DegreesToRadians(Primitive ? Primitive->GetComponentRotation().Yaw : Pawn->GetActorRotation().Yaw);
		OutBody.Axis = FVector2D(FMath::Cos(Yaw), FMath::Sin(Yaw));
	}
	else
	{
		OutBody.Axis = FVector2D(1.0, 0.0);
	}

	OutBody.Weight = EffectiveProfile->Weight;
	OutBody.PushPower = EffectiveProfile->PushPower;
	OutBody.Layer = static_cast<uint8>(FMath::Clamp(EffectiveFilter.Layer, 0, 31));
	OutBody.PushedByMask = static_cast<uint32>(EffectiveFilter.PushedByLayers);
	OutBody.bCanPush = EffectiveFilter.bCanPush;
	OutBody.bCanBePushed = EffectiveFilter.bCanBePushed;
	OutBody.bEnabled = bSoftCollisionEnabled;

	OutInfo.bScanner = IsScanner();
	OutInfo.bPlayerControlled = bPlayerControlled;
	OutInfo.SleepSpeed = EffectiveProfile->Scan.SleepSpeed;
}

void UEGSoftCollisionComponent::GetEffectiveTuning(FEGSoftCollisionTuning& OutTuning) const
{
	const UEGSoftCollisionProfile* EffectiveProfile = GetEffectiveProfile();
	OutTuning = EffectiveProfile ? EffectiveProfile->Tuning : FEGSoftCollisionTuning();

	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (EffectiveProfile && Movement && Movement->bUseRVOAvoidance)
	{
		// Strengths only: durations must stay identical on every machine.
		OutTuning.SeparationStrength *= EffectiveProfile->AvoidanceScale;
		OutTuning.BumpStrength *= EffectiveProfile->AvoidanceScale;
	}
}

void UEGSoftCollisionComponent::ApplyScanResult(const FEGSoftPushResult& Result, const FEGSoftCollisionTuning& Tuning)
{
	if (Result.Band == EEGSoftPushBand::None || !bSoftCollisionEnabled || !IsScanner())
	{
		return;
	}

	const double Now = GetGameTime();
	FVector2D Velocity = Result.Velocity;
	EEGSoftPushBand Band = Result.Band;
	if (Band == EEGSoftPushBand::Bump && Now - LastBumpTime < Tuning.BumpCooldown)
	{
		Velocity = Result.SeparationVelocity;
		Band = Velocity.IsNearlyZero() ? EEGSoftPushBand::None : EEGSoftPushBand::Separation;
		if (Band == EEGSoftPushBand::None)
		{
			return;
		}
	}

	// An active bump plays out its duration and decay; separation only takes over as it ends.
	if (Active.bActive && Active.Command.Band == EEGSoftPushBand::Bump && Band != EEGSoftPushBand::Bump
		&& Active.EndTime - Now > Tuning.ReissueLeadTime)
	{
		return;
	}

	// Reissue rule: keep the active push while it still says the same thing.
	if (Active.bActive && Band != EEGSoftPushBand::Bump && Now < Active.EndTime)
	{
		const double ActiveSpeed = Active.RawVelocity.Size();
		const bool bTurned = EGSoftCollision::AngleBetweenDegrees(Velocity, Active.RawVelocity) > Tuning.ReissueAngle;
		const bool bChanged = FMath::Abs(Velocity.Size() - ActiveSpeed) > Tuning.ReissueStrengthRatio * FMath::Max(ActiveSpeed, 1.0);
		const bool bEnding = Active.EndTime - Now <= Tuning.ReissueLeadTime;
		if (!bTurned && !bChanged && !bEnding)
		{
			return;
		}
	}

	const UEGSoftCollisionProfile* EffectiveProfile = GetEffectiveProfile();
	if (!IssueBucket.TryConsume(GetRealTime(), EffectiveProfile->Network.MaxRequestsPerSecond, EffectiveProfile->Network.RequestBurst))
	{
		return;
	}

	IssuePush(Velocity, Band, Tuning, Result.NeighbourCount);
}

FEGSoftPushCommand UEGSoftCollisionComponent::MakeCommand(const FEGSoftPushQuantized& Quantized, EEGSoftPushBand Band, const FEGSoftCollisionTuning& Tuning) const
{
	FEGSoftPushCommand Command;
	Command.Quantized = Quantized;
	Command.Band = Band;
	const FVector2D Velocity = EGSoftCollision::Dequantize(Quantized);
	Command.Velocity = FVector(Velocity.X, Velocity.Y, 0.0);
	const bool bBump = Band == EEGSoftPushBand::Bump;
	Command.Duration = bBump ? Tuning.BumpDuration : Tuning.SeparationDuration;
	Command.Decay = bBump ? Tuning.BumpDecay : Tuning.SeparationDecay;
	return Command;
}

void UEGSoftCollisionComponent::IssuePush(const FVector2D& RawVelocity, EEGSoftPushBand Band, const FEGSoftCollisionTuning& Tuning, int32 NeighbourCount)
{
	FVector2D Velocity = RawVelocity;

	AActor* Owner = GetOwner();
	if (Owner && Owner->GetClass()->ImplementsInterface(UEGSoftCollisionInterface::StaticClass()))
	{
		const FVector Modified = IEGSoftCollisionInterface::Execute_ModifySoftPushVelocity(Owner, FVector(Velocity.X, Velocity.Y, 0.0), Band);
		Velocity = FVector2D(Modified.X, Modified.Y);
		// The server rejects anything over the cap, so a modified push is capped here.
		const double MaxSpeed = FMath::Max(Tuning.MaxTotalStrength, 0.0f);
		if (Velocity.Size() > MaxSpeed)
		{
			Velocity = Velocity.GetSafeNormal() * MaxSpeed;
		}
		if (Velocity.Size() < FMath::Max(Tuning.MinPushStrength, UE_KINDA_SMALL_NUMBER))
		{
			return;
		}
	}

	const FEGSoftPushQuantized Quantized = EGSoftCollision::Quantize(Velocity);
	if (Quantized.Strength == 0)
	{
		return;
	}

	FEGSoftPushCommand Command = MakeCommand(Quantized, Band, Tuning);
	Command.Sequence = ++NextSequence;

	const bool bPredicting = IsPredictingClient();
	if (bPredicting)
	{
		UCharacterMovementComponent* Movement = GetCharacterMovement();
		const FNetworkPredictionData_Client_Character* ClientData = Movement ? Movement->GetPredictionData_Client_Character() : nullptr;
		if (!ClientData)
		{
			return;
		}
		// The move about to be simulated starts at this timestamp, so the push starts with it.
		Command.bHasStartTime = true;
		Command.StartTime = ClientData->CurrentTimeStamp;
	}

	if (!StartPush(Command, true))
	{
		return;
	}
	Active.RawVelocity = RawVelocity;

	const double Now = GetGameTime();
	if (Band == EEGSoftPushBand::Bump)
	{
		LastBumpTime = Now;
	}

	if (bPredicting)
	{
		FEGSoftPushRequest Request;
		Request.Sequence = Command.Sequence;
		Request.ClientTimeStamp = Command.StartTime;
		Request.Push = Quantized;
		Request.Band = Band;
		ClientQueue.Add(Request, GetRealTime());
	}
	else if (Band == EEGSoftPushBand::Bump && Owner && Owner->HasAuthority())
	{
		SendCosmeticBump(Quantized);
	}

	if (UEGSoftCollisionSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->NotePushIssued();
	}

	BroadcastPush(Command, NeighbourCount, bPredicting, false);
	DebugDrawPush(Command, bPredicting);
	UpdateFrameUpdateRegistration();
}

bool UEGSoftCollisionComponent::StartPush(const FEGSoftPushCommand& Command, bool bCancelPrevious)
{
	UEGSoftCollisionDriver* Driver = GetActiveDriver();
	if (!Driver)
	{
		return false;
	}

	if (bCancelPrevious)
	{
		CancelActivePush();
	}
	Active.bActive = false;

	int32 Handle = INDEX_NONE;
	if (!Driver->ApplyPush(*this, Command, Handle))
	{
		return false;
	}

	const double Now = GetGameTime();
	Active.Command = Command;
	Active.Driver = Driver;
	Active.Handle = Handle;
	Active.StartTime = Now - Command.ElapsedTrim;
	Active.EndTime = Active.StartTime + Command.Duration;
	Active.RawVelocity = FVector2D(Command.Velocity.X, Command.Velocity.Y);
	Active.bActive = true;
	return true;
}

void UEGSoftCollisionComponent::CancelActivePush()
{
	if (Active.bActive && Active.Driver)
	{
		Active.Driver->CancelPush(*this, Active.Command, Active.Handle);
	}
	Active.bActive = false;
}

void UEGSoftCollisionComponent::BroadcastPush(const FEGSoftPushCommand& Command, int32 NeighbourCount, bool bPredicted, bool bCosmetic)
{
	FEGSoftPushEvent Event;
	Event.Velocity = Command.Velocity;
	Event.Duration = Command.Duration;
	Event.Band = Command.Band;
	Event.NeighbourCount = NeighbourCount;
	Event.bPredicted = bPredicted;
	Event.bCosmetic = bCosmetic;

	if (!bCosmetic)
	{
		OnSoftPush.Broadcast(Event);
	}
	if (Command.Band == EEGSoftPushBand::Bump)
	{
		OnBump.Broadcast(Event);
	}
}

void UEGSoftCollisionComponent::SendCosmeticBump(const FEGSoftPushQuantized& Quantized)
{
	const UEGSoftCollisionProfile* EffectiveProfile = GetEffectiveProfile();
	const double Now = GetGameTime();
	if (Now - LastCosmeticBumpTime < EffectiveProfile->Network.CosmeticBumpInterval)
	{
		return;
	}
	LastCosmeticBumpTime = Now;

	++CosmeticBump.Counter;
	CosmeticBump.Push = Quantized;
	MARK_PROPERTY_DIRTY_FROM_NAME(UEGSoftCollisionComponent, CosmeticBump, this);
}

void UEGSoftCollisionComponent::OnRep_CosmeticBump()
{
	// Initial replication arrives before BeginPlay and is an old bump, not a new one.
	if (!HasBegunPlay())
	{
		return;
	}

	FEGSoftCollisionTuning Tuning;
	GetEffectiveTuning(Tuning);
	BroadcastPush(MakeCommand(CosmeticBump.Push, EEGSoftPushBand::Bump, Tuning), 0, false, true);
}

// =====================================================================
// Frame updates
// =====================================================================

bool UEGSoftCollisionComponent::TickFrame(float DeltaSeconds)
{
	const double Now = GetGameTime();
	if (Active.bActive)
	{
		const float Elapsed = static_cast<float>(Now - Active.StartTime);
		if (Elapsed >= Active.Command.Duration || !Active.Driver || !Active.Driver->IsPushRunning(*this, Active.Handle))
		{
			Active.bActive = false;
		}
		else if (Active.Driver->NeedsTick())
		{
			Active.Driver->TickPush(*this, Active.Command, Active.Handle, Elapsed, DeltaSeconds);
		}
	}

	if (ClientQueue.HasPending())
	{
		if (!IsPredictingClient())
		{
			ClientQueue.Reset();
		}
		else
		{
			const FEGSoftCollisionNetSettings& Network = GetEffectiveProfile()->Network;
			const double RealNow = GetRealTime();
			UEGSoftCollisionSubsystem* Subsystem = GetSubsystem();

			TArray<uint16, TInlineAllocator<8>> TimedOut;
			ClientQueue.CollectTimedOut(RealNow, Network.UnansweredTimeout + GetRoundTripTime(), TimedOut);
			for (const uint16 Sequence : TimedOut)
			{
				// A push the server never answered is removed here: server corrections never remove it.
				if (Active.bActive && Active.Command.Sequence == Sequence)
				{
					CancelActivePush();
				}
				if (Subsystem)
				{
					Subsystem->NoteUnansweredPush();
				}
				UE_VLOG(GetOwner(), LogEGSoftCollision, Log, TEXT("Push %u unanswered, removed"), Sequence);
			}

			ClientQueue.CollectBatch(RealNow, Network.RedundancyWindow, Network.ResendInterval, MaxRequestsPerBatch, SendScratch);
			if (!SendScratch.IsEmpty())
			{
				ServerSoftPush(SendScratch);
				if (Subsystem)
				{
					Subsystem->NoteRequestsSent(SendScratch.Num());
				}
			}
		}
	}

	bFrameUpdatesRegistered = (Active.bActive && Active.Driver && Active.Driver->NeedsTick()) || ClientQueue.HasPending();
	return bFrameUpdatesRegistered;
}

void UEGSoftCollisionComponent::UpdateFrameUpdateRegistration()
{
	const bool bNeeds = (Active.bActive && Active.Driver && Active.Driver->NeedsTick()) || ClientQueue.HasPending();
	if (bNeeds == bFrameUpdatesRegistered)
	{
		return;
	}
	if (UEGSoftCollisionSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->SetNeedsFrameUpdate(*this, bNeeds);
		bFrameUpdatesRegistered = bNeeds;
	}
}

// =====================================================================
// Client side of prediction
// =====================================================================

void UEGSoftCollisionComponent::OnRep_LastAcceptedSequence()
{
	ClientQueue.Acknowledge(LastAcceptedSequence);
}

void UEGSoftCollisionComponent::ClientRejectSoftPush_Implementation(uint16 Sequence)
{
	ClientQueue.Remove(Sequence);
	if (Active.bActive && Active.Command.Sequence == Sequence)
	{
		CancelActivePush();
	}

	if (UEGSoftCollisionSubsystem::GetDebugLevel() >= 3)
	{
		UE_LOG(LogEGSoftCollision, Log, TEXT("%s: server rejected push %u"), *GetNameSafe(GetOwner()), Sequence);
	}
	UE_VLOG(GetOwner(), LogEGSoftCollision, Log, TEXT("Push %u rejected by the server"), Sequence);
}

// =====================================================================
// Server side of prediction
// =====================================================================

void UEGSoftCollisionComponent::ServerSoftPush_Implementation(const TArray<FEGSoftPushRequest>& Requests)
{
	if (!GetPawn() || !GetOwner()->HasAuthority())
	{
		return;
	}

	TArray<FEGSoftPushRequest, TInlineAllocator<MaxRequestsAccepted>> Sorted;
	Sorted.Append(Requests.GetData(), FMath::Min(Requests.Num(), MaxRequestsAccepted));
	// Oldest first, wrap-aware. A batch spans a few sequences, far from the wrap ambiguity.
	Sorted.Sort([](const FEGSoftPushRequest& A, const FEGSoftPushRequest& B)
	{
		return EGSoftCollision::IsSequenceNewer(B.Sequence, A.Sequence);
	});

	for (const FEGSoftPushRequest& Request : Sorted)
	{
		if (ServerState.IsDuplicate(Request.Sequence))
		{
			continue;
		}
		ServerState.MarkProcessed(Request.Sequence);
		ProcessServerRequest(Request);
	}

	UpdateMovementBinding();
}

UEGSoftCollisionComponent::ERejectReason UEGSoftCollisionComponent::ValidateRequest(const FEGSoftPushRequest& Request, bool& bOutEpochMismatch)
{
	bOutEpochMismatch = false;

	if (GetEffectiveRole() != EEGSoftCollisionDriverRole::Predicted)
	{
		return ERejectReason::NotPredicted;
	}
	if (!bSoftCollisionEnabled || !EffectiveFilter.bCanBePushed)
	{
		return ERejectReason::Disabled;
	}
	if (Request.Band != EEGSoftPushBand::Separation && Request.Band != EEGSoftPushBand::Bump)
	{
		return ERejectReason::BadBand;
	}
	if (const int32 RejectEvery = UEGSoftCollisionSubsystem::GetDebugRejectEvery(); RejectEvery > 0 && Request.Sequence % RejectEvery == 0)
	{
		return ERejectReason::DebugReject;
	}

	FEGSoftCollisionTuning Tuning;
	GetEffectiveTuning(Tuning);
	const FEGSoftCollisionNetSettings& Network = GetEffectiveProfile()->Network;

	if (EGSoftCollision::DequantizeStrength(Request.Push.Strength) > Tuning.MaxTotalStrength + 2.0f * EGSoftCollision::QuantizedStrengthStep)
	{
		return ERejectReason::TooStrong;
	}

	// Jitter bunches requests up on arrival, so the server's bucket is more generous than the client's.
	if (!ServerState.Bucket.TryConsume(GetRealTime(), Network.MaxRequestsPerSecond * 1.5f, Network.RequestBurst * 2.0f))
	{
		return ERejectReason::RateLimited;
	}

	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (const FNetworkPredictionData_Server_Character* ServerData = Movement ? Movement->GetPredictionData_Server_Character() : nullptr)
	{
		// A client timestamp reset moves the client's clock back by MinTimeBetweenTimeStampResets. A
		// request that far from the server's clock in either direction comes from the other side of a
		// reset the server has not processed yet (or has just processed): apply it without a start time.
		const float Delta = Request.ClientTimeStamp - ServerData->CurrentClientTimeStamp;
		const float EpochGap = 0.5f * FMath::Max(Movement->MinTimeBetweenTimeStampResets, 2.0f * Network.MaxRequestAge);
		bOutEpochMismatch = FMath::Abs(Delta) > EpochGap;
		if (!bOutEpochMismatch && Delta < -Network.MaxRequestAge)
		{
			return ERejectReason::Stale;
		}
	}

	if (Request.Band == EEGSoftPushBand::Bump && GetGameTime() - ServerState.LastBumpTime < Tuning.BumpCooldown * 0.5f)
	{
		return ERejectReason::BumpCooldown;
	}

	const UEGSoftCollisionSubsystem* Subsystem = GetSubsystem();
	if (!Subsystem || !Subsystem->HasPushingNeighbourWithin(RegistryIndex, Network.ValidationSlack))
	{
		return ERejectReason::NoNeighbour;
	}

	return ERejectReason::None;
}

void UEGSoftCollisionComponent::ProcessServerRequest(const FEGSoftPushRequest& Request)
{
	bool bEpochMismatch = false;
	const ERejectReason Reason = ValidateRequest(Request, bEpochMismatch);
	if (Reason != ERejectReason::None)
	{
		RejectRequest(Request.Sequence, Reason);
		return;
	}

	const UEGSoftCollisionProfile* EffectiveProfile = GetEffectiveProfile();
	const FEGSoftCollisionNetSettings& Network = EffectiveProfile->Network;
	FEGSoftCollisionTuning Tuning;
	GetEffectiveTuning(Tuning);

	FEGSoftPushCommand Command = MakeCommand(Request.Push, Request.Band, Tuning);
	Command.Sequence = Request.Sequence;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	const FNetworkPredictionData_Server_Character* ServerData = Movement ? Movement->GetPredictionData_Server_Character() : nullptr;
	const float CurrentClientTime = ServerData ? ServerData->CurrentClientTimeStamp : 0.0f;

	bool bLate = false;
	if (ServerData && !bEpochMismatch)
	{
		Command.bHasStartTime = true;
		Command.StartTime = Request.ClientTimeStamp;

		const float Missed = CurrentClientTime - Request.ClientTimeStamp;
		if (Missed > UE_KINDA_SMALL_NUMBER)
		{
			// The move this push starts on was already processed: its first copy was lost.
			bLate = true;
			if (UEGSoftCollisionSubsystem* Subsystem = GetSubsystem())
			{
				Subsystem->NoteLateRequest();
			}
			if (Network.LatePolicy == EEGSoftCollisionLatePolicy::Reject || Missed >= Command.Duration)
			{
				RejectRequest(Request.Sequence, ERejectReason::Late);
				return;
			}
			Command.ElapsedTrim = Missed;
		}
	}

	// The push being replaced ends exactly where this one starts. The client already switched; the
	// server switches after processing the client move that ends at this push's start time.
	if (Active.bActive && Active.Driver)
	{
		// Usually the move ending at the start time was already processed when the request arrives
		// (the client sent it last frame); then the boundary has passed and the old push goes now.
		const bool bBoundaryAhead = Command.bHasStartTime && !bLate && CurrentClientTime < Command.StartTime - UE_KINDA_SMALL_NUMBER;
		if (bBoundaryAhead)
		{
			FEGSoftPushScheduledRemoval& Removal = ServerState.Removals.AddDefaulted_GetRef();
			Removal.Driver = Active.Driver;
			Removal.Command = Active.Command;
			Removal.Handle = Active.Handle;
			Removal.ClientTime = Command.StartTime;
			Active.bActive = false;
		}
		else
		{
			CancelActivePush();
		}
	}

	if (!StartPush(Command, false))
	{
		RejectRequest(Request.Sequence, ERejectReason::DriverFailed);
		return;
	}
	UpdateFrameUpdateRegistration();

	LastAcceptedSequence = Request.Sequence;
	MARK_PROPERTY_DIRTY_FROM_NAME(UEGSoftCollisionComponent, LastAcceptedSequence, this);

	if (Movement)
	{
		const float StartClientTime = Command.bHasStartTime ? Command.StartTime : CurrentClientTime;
		SafetyLease.Acquire(*Movement, UEGSoftCollisionSubsystem::ResolveSafetyLevel(Network.SafetyLevel), StartClientTime + Command.Duration + Network.SafetyGrace);
	}

	if (Request.Band == EEGSoftPushBand::Bump)
	{
		ServerState.LastBumpTime = GetGameTime();
		LastBumpTime = ServerState.LastBumpTime;
		SendCosmeticBump(Request.Push);
	}

	if (UEGSoftCollisionSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->NoteRequestAccepted();
	}

	BroadcastPush(Command, 0, false, false);
	DebugDrawPush(Command, false);
}

void UEGSoftCollisionComponent::RejectRequest(uint16 Sequence, ERejectReason Reason)
{
	ClientRejectSoftPush(Sequence);
	if (UEGSoftCollisionSubsystem* Subsystem = GetSubsystem())
	{
		Subsystem->NoteRequestRejected();
	}

	if (UEGSoftCollisionSubsystem::GetDebugLevel() >= 3)
	{
		UE_LOG(LogEGSoftCollision, Log, TEXT("%s: rejected push %u (reason %d)"), *GetNameSafe(GetOwner()), Sequence, static_cast<int32>(Reason));
	}
	UE_VLOG(GetOwner(), LogEGSoftCollision, Log, TEXT("Rejected push %u (reason %d)"), Sequence, static_cast<int32>(Reason));
}

void UEGSoftCollisionComponent::HandleCharacterMovementUpdated(float DeltaSeconds, FVector OldLocation, FVector OldVelocity)
{
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	const FNetworkPredictionData_Server_Character* ServerData = Movement ? Movement->GetPredictionData_Server_Character() : nullptr;
	if (!ServerData)
	{
		return;
	}

	const float ClientTime = ServerData->CurrentClientTimeStamp;
	if (ServerState.bHasLastClientTime && ClientTime + TimeStampResetThreshold < ServerState.LastClientTime)
	{
		// The client reset its timestamps: scheduled times are meaningless now.
		for (const FEGSoftPushScheduledRemoval& Removal : ServerState.Removals)
		{
			if (Removal.Driver)
			{
				Removal.Driver->CancelPush(*this, Removal.Command, Removal.Handle);
			}
		}
		ServerState.Removals.Reset();
		SafetyLease.OnTimeStampReset(*Movement);
	}
	ServerState.LastClientTime = ClientTime;
	ServerState.bHasLastClientTime = true;

	for (int32 Index = ServerState.Removals.Num() - 1; Index >= 0; --Index)
	{
		const FEGSoftPushScheduledRemoval& Removal = ServerState.Removals[Index];
		if (ClientTime >= Removal.ClientTime - UE_KINDA_SMALL_NUMBER)
		{
			if (Removal.Driver)
			{
				Removal.Driver->CancelPush(*this, Removal.Command, Removal.Handle);
			}
			ServerState.Removals.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		}
	}

	if (SafetyLease.IsActive())
	{
		FEGSoftCollisionTuning Tuning;
		GetEffectiveTuning(Tuning);
		const FVector MoveEnd = Movement->UpdatedComponent ? Movement->UpdatedComponent->GetComponentLocation() : OldLocation;
		const float Tolerance = GetEffectiveProfile()->Network.AuthoritativeWindowTolerance;
		if (!SafetyLease.OnServerMove(*Movement, ClientTime, OldLocation, MoveEnd, DeltaSeconds, Tuning.MaxTotalStrength, Tolerance))
		{
			UE_LOG(LogEGSoftCollision, Warning, TEXT("%s: client moved further than its push allows; authoritative window closed."), *GetNameSafe(GetOwner()));
		}
	}

	UpdateMovementBinding();
}

void UEGSoftCollisionComponent::UpdateMovementBinding()
{
	ACharacter* Character = Cast<ACharacter>(GetPawn());
	const bool bNeeds = Character && Character->HasAuthority() && !Character->IsLocallyControlled()
		&& (SafetyLease.IsActive() || !ServerState.Removals.IsEmpty());

	if (bNeeds == bMovementBound || !Character)
	{
		return;
	}

	// Bound only while needed: a dynamic delegate on a thousand AI pawns would cost every move.
	if (bNeeds)
	{
		Character->OnCharacterMovementUpdated.AddDynamic(this, &UEGSoftCollisionComponent::HandleCharacterMovementUpdated);
	}
	else
	{
		Character->OnCharacterMovementUpdated.RemoveDynamic(this, &UEGSoftCollisionComponent::HandleCharacterMovementUpdated);
	}
	bMovementBound = bNeeds;
}

void UEGSoftCollisionComponent::ResetPredictionState()
{
	ClientQueue.Reset();

	for (const FEGSoftPushScheduledRemoval& Removal : ServerState.Removals)
	{
		if (Removal.Driver)
		{
			Removal.Driver->CancelPush(*this, Removal.Command, Removal.Handle);
		}
	}
	ServerState.Reset();

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		if (SafetyLease.IsActive())
		{
			SafetyLease.Release(*Movement);
		}
	}
	UpdateMovementBinding();
}

// =====================================================================
// Helpers
// =====================================================================

double UEGSoftCollisionComponent::GetGameTime() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

double UEGSoftCollisionComponent::GetRealTime() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetRealTimeSeconds() : 0.0;
}

float UEGSoftCollisionComponent::GetRoundTripTime() const
{
	const APawn* Pawn = GetPawn();
	const APlayerState* PlayerState = Pawn ? Pawn->GetPlayerState() : nullptr;
	return PlayerState ? PlayerState->GetPingInMilliseconds() * 0.001f : 0.2f;
}

UEGSoftCollisionSubsystem* UEGSoftCollisionComponent::GetSubsystem() const
{
	return UEGSoftCollisionSubsystem::Get(GetWorld());
}

void UEGSoftCollisionComponent::DebugDrawPush(const FEGSoftPushCommand& Command, bool bPredicted) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const FVector Start = Owner->GetActorLocation();
	const FVector End = Start + Command.Velocity * 0.25f;
	const FColor Color = Command.Band == EEGSoftPushBand::Bump ? FColor::Red : (bPredicted ? FColor::Green : FColor::Yellow);

	UE_VLOG_ARROW(Owner, LogEGSoftCollision, Log, Start, End, Color, TEXT("Push %u %s %.0f cm/s"),
		Command.Sequence, BandToString(Command.Band), Command.Velocity.Size());

#if UE_ENABLE_DEBUG_DRAWING
	if (UEGSoftCollisionSubsystem::GetDebugLevel() >= 1)
	{
		DrawDebugDirectionalArrow(GetWorld(), Start, End, 20.0f, Color, false, Command.Duration, 0, 2.0f);
	}
#endif
}
