#include "EGASAbilitySystemComponent.h"

#include "Abilities/GameplayAbility.h"
#include "EGASGameplayAbility.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "InputAction.h"

UEGASAbilitySystemComponent::UEGASAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
	SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}

void UEGASAbilitySystemComponent::BeginDestroy()
{
	ClearAbilityInputBindings();
	Super::BeginDestroy();
}

void UEGASAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);
	GrantStartupAbilities();
	RefreshAbilityInputBindings();
}

void UEGASAbilitySystemComponent::GrantStartupAbilities()
{
	if (!HasAuthorityToGrant() || bStartupAbilitiesGranted)
	{
		return;
	}

	bStartupAbilitiesGranted = true;

	UObject* SourceObject = GetOwnerActor();
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		GrantAbility(AbilityClass, 1, INDEX_NONE, SourceObject);
	}

	for (UEGASAbilitySet* AbilitySet : DefaultAbilitySets)
	{
		if (AbilitySet)
		{
			AbilitySet->GiveToAbilitySystem(this, DefaultAbilitySetHandles, SourceObject);
		}
	}
}

FGameplayAbilitySpecHandle UEGASAbilitySystemComponent::GrantAbility(TSubclassOf<UGameplayAbility> AbilityClass, int32 AbilityLevel, int32 InputID, UObject* SourceObject)
{
	if (!HasAuthorityToGrant() || !AbilityClass)
	{
		return FGameplayAbilitySpecHandle();
	}

	FGameplayAbilitySpec AbilitySpec(AbilityClass, AbilityLevel, InputID, SourceObject);
	return GiveAbility(AbilitySpec);
}

FActiveGameplayEffectHandle UEGASAbilitySystemComponent::GrantGameplayEffect(TSubclassOf<UGameplayEffect> GameplayEffectClass, float EffectLevel)
{
	if (!HasAuthorityToGrant() || !GameplayEffectClass)
	{
		return FActiveGameplayEffectHandle();
	}

	const UGameplayEffect* GameplayEffect = GameplayEffectClass->GetDefaultObject<UGameplayEffect>();
	return ApplyGameplayEffectToSelf(GameplayEffect, EffectLevel, MakeEffectContext());
}

UAttributeSet* UEGASAbilitySystemComponent::GrantAttributeSet(TSubclassOf<UAttributeSet> AttributeSetClass)
{
	if (!HasAuthorityToGrant() || !AttributeSetClass)
	{
		return nullptr;
	}

	UObject* AttributeOuter = GetOwner();
	if (!AttributeOuter)
	{
		AttributeOuter = this;
	}

	UAttributeSet* NewAttributeSet = NewObject<UAttributeSet>(AttributeOuter, AttributeSetClass);
	AddAttributeSetSubobject(NewAttributeSet);
	return NewAttributeSet;
}

void UEGASAbilitySystemComponent::RevokeAbility(FGameplayAbilitySpecHandle AbilityHandle)
{
	if (HasAuthorityToGrant() && AbilityHandle.IsValid())
	{
		ClearAbility(AbilityHandle);
	}
}

void UEGASAbilitySystemComponent::RevokeGameplayEffect(FActiveGameplayEffectHandle EffectHandle, int32 StacksToRemove)
{
	if (HasAuthorityToGrant() && EffectHandle.IsValid())
	{
		RemoveActiveGameplayEffect(EffectHandle, StacksToRemove);
	}
}

void UEGASAbilitySystemComponent::RevokeAttributeSet(UAttributeSet* AttributeSet)
{
	if (HasAuthorityToGrant() && AttributeSet)
	{
		RemoveSpawnedAttribute(AttributeSet);
	}
}

bool UEGASAbilitySystemComponent::HasAuthorityToGrant() const
{
	return IsOwnerActorAuthoritative();
}

void UEGASAbilitySystemComponent::RefreshAbilityInputBindings()
{
	ClearAbilityInputBindings();

	UEnhancedInputComponent* EnhancedInput = GetEnhancedInputComponent();
	if (!EnhancedInput)
	{
		return;
	}

	BoundInputComponent = EnhancedInput;

	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		BindAbilityInput(AbilitySpec);
	}
}

void UEGASAbilitySystemComponent::ClearAbilityInputBindings()
{
	if (UEnhancedInputComponent* EnhancedInput = BoundInputComponent.Get())
	{
		for (const TPair<int32, FEGASAbilityInputBindingHandles>& Pair : InputIDBindingHandles)
		{
			EnhancedInput->RemoveBindingByHandle(Pair.Value.Pressed);
			EnhancedInput->RemoveBindingByHandle(Pair.Value.Completed);
			EnhancedInput->RemoveBindingByHandle(Pair.Value.Canceled);
		}
	}

	InputIDBindingHandles.Reset();
	BoundInputComponent.Reset();

	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		const UEGASGameplayAbility* Ability = Cast<UEGASGameplayAbility>(AbilitySpec.Ability);
		if (Ability && !Ability->ActivationInputAction.IsNull())
		{
			AbilitySpec.InputID = INDEX_NONE;
		}
	}
}

bool UEGASAbilitySystemComponent::HasAbilityBindingForInputAction(const UInputAction* InputAction) const
{
	if (!InputAction)
	{
		return false;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		const UEGASGameplayAbility* Ability = Cast<UEGASGameplayAbility>(AbilitySpec.Ability);
		if (!Ability || Ability->ActivationInputAction.IsNull())
		{
			continue;
		}

		if (Ability->ActivationInputAction.Get() == InputAction
			|| Ability->ActivationInputAction.ToSoftObjectPath() == InputAction->GetPathName())
		{
			return true;
		}
	}

	return false;
}

void UEGASAbilitySystemComponent::OnGiveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	Super::OnGiveAbility(AbilitySpec);
	BindAbilityInput(AbilitySpec);
}

void UEGASAbilitySystemComponent::OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	UnbindAbilityInput(AbilitySpec);
	Super::OnRemoveAbility(AbilitySpec);
}

void UEGASAbilitySystemComponent::BindAbilityInput(FGameplayAbilitySpec& AbilitySpec)
{
	const UEGASGameplayAbility* Ability = Cast<UEGASGameplayAbility>(AbilitySpec.Ability);
	if (!Ability || Ability->ActivationInputAction.IsNull())
	{
		return;
	}

	UInputAction* InputAction = Ability->ActivationInputAction.LoadSynchronous();
	if (!InputAction)
	{
		return;
	}

	const int32 InputID = GetOrCreateInputID(InputAction);
	AbilitySpec.InputID = InputID;

	UEnhancedInputComponent* EnhancedInput = GetEnhancedInputComponent();
	if (!EnhancedInput)
	{
		return;
	}

	if (BoundInputComponent.IsValid() && BoundInputComponent.Get() != EnhancedInput)
	{
		RefreshAbilityInputBindings();
		return;
	}

	BoundInputComponent = EnhancedInput;
	if (InputIDBindingHandles.Contains(InputID))
	{
		return;
	}

	FEnhancedInputActionEventBinding& PressBinding = EnhancedInput->BindAction(
		InputAction, ETriggerEvent::Started, this,
		&UEGASAbilitySystemComponent::HandleAbilityInputPressed, InputID);
	FEnhancedInputActionEventBinding& ReleaseBinding = EnhancedInput->BindAction(
		InputAction, ETriggerEvent::Completed, this,
		&UEGASAbilitySystemComponent::HandleAbilityInputReleased, InputID);
	FEnhancedInputActionEventBinding& CancelBinding = EnhancedInput->BindAction(
		InputAction, ETriggerEvent::Canceled, this,
		&UEGASAbilitySystemComponent::HandleAbilityInputReleased, InputID);

	FEGASAbilityInputBindingHandles BindingHandles;
	BindingHandles.Pressed = PressBinding.GetHandle();
	BindingHandles.Completed = ReleaseBinding.GetHandle();
	BindingHandles.Canceled = CancelBinding.GetHandle();
	InputIDBindingHandles.Add(InputID, BindingHandles);
}

void UEGASAbilitySystemComponent::UnbindAbilityInput(FGameplayAbilitySpec& AbilitySpec)
{
	const int32 InputID = AbilitySpec.InputID;
	if (InputID == INDEX_NONE)
	{
		return;
	}

	AbilitySpec.InputID = INDEX_NONE;
	for (const FGameplayAbilitySpec& OtherSpec : GetActivatableAbilities())
	{
		if (OtherSpec.Handle != AbilitySpec.Handle && OtherSpec.InputID == InputID)
		{
			return;
		}
	}

	RemoveBindingsForInputID(InputID);
}

int32 UEGASAbilitySystemComponent::GetOrCreateInputID(UInputAction* InputAction)
{
	if (const int32* ExistingInputID = InputActionToIDMap.Find(InputAction))
	{
		return *ExistingInputID;
	}

	while (InputActionToIDMap.FindKey(NextAutoInputID)
		|| GetActivatableAbilities().ContainsByPredicate([this](const FGameplayAbilitySpec& AbilitySpec)
		{
			return AbilitySpec.InputID == NextAutoInputID;
		}))
	{
		++NextAutoInputID;
	}

	const int32 NewInputID = NextAutoInputID++;
	InputActionToIDMap.Add(InputAction, NewInputID);
	return NewInputID;
}

void UEGASAbilitySystemComponent::HandleAbilityInputPressed(const FInputActionValue& Value, int32 InputID)
{
	(void)Value;
	AbilityLocalInputPressed(InputID);
}

void UEGASAbilitySystemComponent::HandleAbilityInputReleased(const FInputActionValue& Value, int32 InputID)
{
	(void)Value;
	AbilityLocalInputReleased(InputID);
}

UEnhancedInputComponent* UEGASAbilitySystemComponent::GetEnhancedInputComponent() const
{
	const APawn* AvatarPawn = AbilityActorInfo.IsValid() ? Cast<APawn>(AbilityActorInfo->AvatarActor.Get()) : nullptr;
	return AvatarPawn ? Cast<UEnhancedInputComponent>(AvatarPawn->InputComponent) : nullptr;
}

void UEGASAbilitySystemComponent::RemoveBindingsForInputID(int32 InputID)
{
	FEGASAbilityInputBindingHandles BindingHandles;
	if (!InputIDBindingHandles.RemoveAndCopyValue(InputID, BindingHandles))
	{
		return;
	}

	if (UEnhancedInputComponent* EnhancedInput = BoundInputComponent.Get())
	{
		EnhancedInput->RemoveBindingByHandle(BindingHandles.Pressed);
		EnhancedInput->RemoveBindingByHandle(BindingHandles.Completed);
		EnhancedInput->RemoveBindingByHandle(BindingHandles.Canceled);
	}
}
