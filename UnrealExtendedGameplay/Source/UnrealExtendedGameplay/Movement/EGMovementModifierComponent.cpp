// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#include "EGMovementModifierComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

// ---------------------------------------------------------------------------------------------
// FEGMovementModifierStack
// ---------------------------------------------------------------------------------------------

int32 FEGMovementModifierStack::Push(const FEGMovementModifier& Modifier)
{
	NextId = (NextId == MAX_int32) ? 1 : NextId + 1;
	Modifiers.Emplace(NextId, Modifier);
	return NextId;
}

bool FEGMovementModifierStack::Remove(int32 Id)
{
	return Modifiers.RemoveAll([Id](const TPair<int32, FEGMovementModifier>& Entry) { return Entry.Key == Id; }) > 0;
}

FEGMovementModifier FEGMovementModifierStack::GetCombined() const
{
	FEGMovementModifier Combined;
	for (const TPair<int32, FEGMovementModifier>& Entry : Modifiers)
	{
		Combined.AccelerationScale *= Entry.Value.AccelerationScale;
		Combined.MaxWalkSpeedScale *= Entry.Value.MaxWalkSpeedScale;
		Combined.RotationRateScale *= Entry.Value.RotationRateScale;
	}
	return Combined;
}

float FEGMovementModifierStack::Rebase(float PreviousBase, float LastApplied, float Current, float LastScale)
{
	if (FMath::IsNearlyEqual(Current, LastApplied) || FMath::IsNearlyZero(LastScale))
	{
		return PreviousBase;
	}
	return Current / LastScale;
}

// ---------------------------------------------------------------------------------------------
// UEGMovementModifierComponent
// ---------------------------------------------------------------------------------------------

UEGMovementModifierComponent::UEGMovementModifierComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UEGMovementModifierComponent* UEGMovementModifierComponent::FindOrAdd(ACharacter* Character)
{
	if (!IsValid(Character))
	{
		return nullptr;
	}

	if (UEGMovementModifierComponent* Existing = Character->FindComponentByClass<UEGMovementModifierComponent>())
	{
		return Existing;
	}

	UEGMovementModifierComponent* Added = NewObject<UEGMovementModifierComponent>(Character, NAME_None, RF_Transient);
	Added->RegisterComponent();
	return Added;
}

UCharacterMovementComponent* UEGMovementModifierComponent::GetMovement() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	return Character ? Character->GetCharacterMovement() : nullptr;
}

FEGMovementModifierHandle UEGMovementModifierComponent::PushModifier(const FEGMovementModifier& Modifier)
{
	FEGMovementModifierHandle Handle;
	UCharacterMovementComponent* Movement = GetMovement();
	if (!Movement)
	{
		return Handle;
	}

	if (!bHasBase)
	{
		BaseAcceleration = Movement->MaxAcceleration;
		BaseWalkSpeed = Movement->MaxWalkSpeed;
		BaseRotationRate = Movement->RotationRate;
		LastAcceleration = BaseAcceleration;
		LastWalkSpeed = BaseWalkSpeed;
		LastRotationRate = BaseRotationRate;
		LastScale = FEGMovementModifier();
		bHasBase = true;
	}

	Handle.Id = Stack.Push(Modifier);
	Apply();
	return Handle;
}

void UEGMovementModifierComponent::RemoveModifier(FEGMovementModifierHandle& Handle)
{
	if (Handle.IsValid() && Stack.Remove(Handle.Id))
	{
		if (Stack.IsEmpty())
		{
			Restore();
		}
		else
		{
			Apply();
		}
	}
	Handle.Invalidate();
}

void UEGMovementModifierComponent::Apply()
{
	UCharacterMovementComponent* Movement = GetMovement();
	if (!Movement || !bHasBase)
	{
		return;
	}

	// Pick up changes other code made since our last write (a sprint toggled mid-attack).
	BaseAcceleration = FEGMovementModifierStack::Rebase(BaseAcceleration, LastAcceleration, Movement->MaxAcceleration, LastScale.AccelerationScale);
	BaseWalkSpeed = FEGMovementModifierStack::Rebase(BaseWalkSpeed, LastWalkSpeed, Movement->MaxWalkSpeed, LastScale.MaxWalkSpeedScale);
	if (!Movement->RotationRate.Equals(LastRotationRate) && !FMath::IsNearlyZero(LastScale.RotationRateScale))
	{
		BaseRotationRate = Movement->RotationRate * (1.0f / LastScale.RotationRateScale);
	}

	const FEGMovementModifier Scale = Stack.GetCombined();
	Movement->MaxAcceleration = BaseAcceleration * Scale.AccelerationScale;
	Movement->MaxWalkSpeed = BaseWalkSpeed * Scale.MaxWalkSpeedScale;
	Movement->RotationRate = BaseRotationRate * Scale.RotationRateScale;

	LastScale = Scale;
	LastAcceleration = Movement->MaxAcceleration;
	LastWalkSpeed = Movement->MaxWalkSpeed;
	LastRotationRate = Movement->RotationRate;
}

void UEGMovementModifierComponent::Restore()
{
	UCharacterMovementComponent* Movement = GetMovement();
	if (Movement && bHasBase)
	{
		// Through Apply with an empty stack, so a change made by other code meanwhile is kept.
		Apply();
	}
	bHasBase = false;
}

void UEGMovementModifierComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (!Stack.IsEmpty())
	{
		Stack = FEGMovementModifierStack();
		Restore();
	}
	Super::EndPlay(EndPlayReason);
}
