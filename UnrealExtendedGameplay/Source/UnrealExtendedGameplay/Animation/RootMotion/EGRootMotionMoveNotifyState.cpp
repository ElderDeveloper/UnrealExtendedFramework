// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGRootMotionMoveNotifyState.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UnrealExtendedGameplay/Animation/EGAnimNotifyHelpers.h"

void UEGRootMotionMoveNotifyState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!EGAnimNotify::CanDriveOwnerTransform(MeshComp))
	{
		return;
	}

	ACharacter* const Character = Cast<ACharacter>(MeshComp->GetOwner());
	UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement)
	{
		return;
	}

	// The window lasts TotalDuration in clip time; in real time that is divided by the effective rate
	// (instance play rate x the montage's RateScale). A hit pause stretches the window further; the
	// source then ends before the window does, which NotifyEnd tolerates.
	float PlayRate = 1.0f;
	if (const UAnimMontage* Montage = Cast<UAnimMontage>(Animation))
	{
		// A montage that is blending out has no active instance and reports a rate of 0, which would
		// stretch the source by 1 / KINDA_SMALL_NUMBER; assume an instance rate of 1 then.
		const UAnimInstance* AnimInstance = MeshComp->GetAnimInstance();
		const float InstanceRate = AnimInstance ? FMath::Abs(AnimInstance->Montage_GetPlayRate(Montage)) : 0.0f;
		PlayRate = FMath::Abs((InstanceRate > KINDA_SMALL_NUMBER ? InstanceRate : 1.0f) * Montage->RateScale);
	}
	const float Duration = TotalDuration / FMath::Max(PlayRate, KINDA_SMALL_NUMBER);

	const FVector Direction = Character->GetActorQuat().RotateVector(DirectionOffset.Vector());

	TSharedPtr<FRootMotionSource_ConstantForce> Source = MakeShared<FRootMotionSource_ConstantForce>();
	Source->InstanceName = TEXT("EGRootMotionMove");
	Source->AccumulateMode = bAdditive ? ERootMotionAccumulateMode::Additive : ERootMotionAccumulateMode::Override;
	Source->Priority = static_cast<uint16>(FMath::Clamp(Priority, 0, 65535));
	Source->Force = Direction * Speed;
	Source->Duration = Duration;
	Source->StrengthOverTime = StrengthOverTime;
	Source->FinishVelocityParams.Mode = FinishVelocityMode;
	Source->FinishVelocityParams.SetVelocity = FinishSetVelocity;
	Source->FinishVelocityParams.ClampVelocity = FinishClampVelocity;

	const uint16 SourceId = Movement->ApplyRootMotionSource(Source);
	if (SourceId != (uint16)ERootMotionSourceID::Invalid)
	{
		for (auto It = ActiveSources.CreateIterator(); It; ++It)
		{
			if (!It->Key.IsValid())
			{
				It.RemoveCurrent();
			}
		}
		ActiveSources.FindOrAdd(TWeakObjectPtr<USkeletalMeshComponent>(MeshComp)).Add(SourceId);
	}
}

void UEGRootMotionMoveNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	const TWeakObjectPtr<USkeletalMeshComponent> Key(MeshComp);
	TArray<uint16>* Sources = ActiveSources.Find(Key);
	if (!Sources || Sources->Num() == 0)
	{
		return;
	}

	const uint16 SourceId = (*Sources)[0];
	Sources->RemoveAt(0);
	if (Sources->Num() == 0)
	{
		ActiveSources.Remove(Key);
	}

	// Ends early when the montage is interrupted; harmless when the source already finished.
	const ACharacter* const Character = MeshComp ? Cast<ACharacter>(MeshComp->GetOwner()) : nullptr;
	if (UCharacterMovementComponent* const Movement = Character ? Character->GetCharacterMovement() : nullptr)
	{
		Movement->RemoveRootMotionSourceByID(SourceId);
	}
}
