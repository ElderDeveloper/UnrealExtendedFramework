// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGAnimInstance.h"

#include "EGAnimStateProvider.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"

void UEGAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	OwningPawn = TryGetPawnOwner();
	OwningCharacter = Cast<ACharacter>(OwningPawn);
	CharacterMovement = OwningCharacter ? OwningCharacter->GetCharacterMovement() : nullptr;
	bHasPreviousYaw = false;
	RefreshStateProviders();
}

void UEGAnimInstance::NativeBeginPlay()
{
	Super::NativeBeginPlay();

	// A spawned pawn initialises its anim instance before its construction script adds Blueprint
	// components (cover, anim events), so look again now that they exist.
	RefreshStateProviders();
}

void UEGAnimInstance::RefreshStateProviders()
{
	StateProviders.Reset();

	AActor* const Owner = GetOwningActor();
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
		if (Component && Component->Implements<UEGAnimStateProvider>())
		{
			StateProviders.Add(Component);
		}
	}
}

void UEGAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// The pawn can be assigned after initialisation (a mesh set up before possession).
	if (!OwningPawn)
	{
		OwningPawn = TryGetPawnOwner();
		OwningCharacter = Cast<ACharacter>(OwningPawn);
		CharacterMovement = OwningCharacter ? OwningCharacter->GetCharacterMovement() : nullptr;
		if (OwningPawn)
		{
			RefreshStateProviders();
		}
	}

	GatherGameThreadData(DeltaSeconds);
}

void UEGAnimInstance::GatherGameThreadData(float DeltaSeconds)
{
	if (!OwningPawn)
	{
		return;
	}

	Velocity = OwningPawn->GetVelocity();
	ActorRotation = OwningPawn->GetActorRotation();
	AimRotation = OwningPawn->GetBaseAimRotation();

	if (CharacterMovement)
	{
		Acceleration = CharacterMovement->GetCurrentAcceleration();
		MaxWalkSpeed = CharacterMovement->MaxWalkSpeed;
		bIsFalling = CharacterMovement->IsFalling();
		bIsCrouching = CharacterMovement->IsCrouching();
	}
	else
	{
		Acceleration = FVector::ZeroVector;
		bIsFalling = false;
		bIsCrouching = false;
	}

	StateTags.Reset();
	for (int32 Index = StateProviders.Num() - 1; Index >= 0; --Index)
	{
		UObject* const Provider = StateProviders[Index].Get();
		if (!Provider)
		{
			StateProviders.RemoveAtSwap(Index);
			continue;
		}
		StateTags.AppendTags(IEGAnimStateProvider::Execute_GetAnimStateTags(Provider));
	}
}

void UEGAnimInstance::NativeThreadSafeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeThreadSafeUpdateAnimation(DeltaSeconds);
	ComputeThreadSafeData(DeltaSeconds);
}

void UEGAnimInstance::ComputeThreadSafeData(float DeltaSeconds)
{
	Speed = static_cast<float>(Velocity.Size());
	GroundSpeed = static_cast<float>(Velocity.Size2D());
	bHasAcceleration = Acceleration.SizeSquared2D() > KINDA_SMALL_NUMBER;
	bShouldMove = GroundSpeed > MoveSpeedThreshold && bHasAcceleration;

	if (GroundSpeed > KINDA_SMALL_NUMBER)
	{
		const FVector LocalVelocity = ActorRotation.UnrotateVector(Velocity);
		Direction = static_cast<float>(FMath::RadiansToDegrees(FMath::Atan2(LocalVelocity.Y, LocalVelocity.X)));
	}
	else
	{
		Direction = 0.0f;
	}

	const FRotator AimDelta = (AimRotation - ActorRotation).GetNormalized();
	AimPitch = static_cast<float>(AimDelta.Pitch);
	AimYaw = static_cast<float>(AimDelta.Yaw);

	// Lean from the turn rate, smoothed so a snap turn does not flick the body.
	const float Yaw = static_cast<float>(ActorRotation.Yaw);
	float TargetLean = 0.0f;
	if (bHasPreviousYaw && DeltaSeconds > KINDA_SMALL_NUMBER)
	{
		const float YawRate = FMath::FindDeltaAngleDegrees(PreviousYaw, Yaw) / DeltaSeconds;
		TargetLean = FMath::Clamp(YawRate / FullLeanYawRate, -1.0f, 1.0f);
	}
	Lean = FMath::FInterpTo(Lean, TargetLean, DeltaSeconds, LeanInterpSpeed);
	PreviousYaw = Yaw;
	bHasPreviousYaw = true;
}
