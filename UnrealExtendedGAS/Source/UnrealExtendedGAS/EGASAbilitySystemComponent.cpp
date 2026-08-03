#include "EGASAbilitySystemComponent.h"

#include "Abilities/GameplayAbility.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "EGASGameplayAbility.h"
#include "EnhancedInputComponent.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameplayEffect.h"
#include "InputAction.h"
#include "Kismet/KismetSystemLibrary.h"

UEGASAbilitySystemComponent::UEGASAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
	SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}

// -------------------------------------------------------------------------
// Lifecycle
// -------------------------------------------------------------------------

void UEGASAbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();

	// Register ability lifecycle callbacks
	AbilityActivatedCallbacks.AddUObject(this, &UEGASAbilitySystemComponent::OnAbilityActivatedCallback);
	AbilityFailedCallbacks.AddUObject(this, &UEGASAbilitySystemComponent::OnAbilityFailedCallback);
	AbilityEndedCallbacks.AddUObject(this, &UEGASAbilitySystemComponent::OnAbilityEndedCallback);

	// Grant startup effects on BeginPlay (not in InitAbilityActorInfo to avoid
	// periodic effects ticking when BP is opened in editor)
	GrantStartupEffects();
}

void UEGASAbilitySystemComponent::BeginDestroy()
{
	ClearAbilityInputBindings();

	// Clean up pawn controller delegate
	if (AbilityActorInfo && AbilityActorInfo->OwnerActor.IsValid())
	{
		if (UGameInstance* GameInstance = AbilityActorInfo->OwnerActor->GetGameInstance())
		{
			GameInstance->GetOnPawnControllerChanged().RemoveAll(this);
		}
	}

	OnGiveAbilityDelegate.RemoveAll(this);

	// Remove granted attributes
	for (UAttributeSet* AttributeSetInstance : AddedAttributes)
	{
		RemoveSpawnedAttribute(AttributeSetInstance);
	}

	// Clear granted abilities on authority
	if (IsOwnerActorAuthoritative())
	{
		for (const FEGASMappedAbility& MappedAbility : AddedAbilityHandles)
		{
			SetRemoveAbilityOnEnd(MappedAbility.Handle);
		}
	}

	AddedAbilityHandles.Reset();
	AddedAttributes.Reset();
	AddedEffects.Reset();

	DefaultAbilitySetHandles.TakeFromAbilitySystem(this);

	Super::BeginDestroy();
}

// -------------------------------------------------------------------------
// InitAbilityActorInfo
// -------------------------------------------------------------------------

void UEGASAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);

	// Only run in game worlds (prevents editor preview initialization)
	if (GetWorld() && !GetWorld()->IsGameWorld())
	{
		return;
	}

	if (AbilityActorInfo && InOwnerActor)
	{
		// Cache anim instance if missing
		if (AbilityActorInfo->AnimInstance == nullptr)
		{
			AbilityActorInfo->AnimInstance = AbilityActorInfo->GetAnimInstance();
		}

		// Register for controller change events to re-init cached info
		if (UGameInstance* GameInstance = InOwnerActor->GetGameInstance())
		{
			if (!GameInstance->GetOnPawnControllerChanged().Contains(this, TEXT("OnPawnControllerChanged")))
			{
				GameInstance->GetOnPawnControllerChanged().AddDynamic(this, &UEGASAbilitySystemComponent::OnPawnControllerChanged);
			}
		}
	}

	// Grant abilities, attributes, and ability sets
	GrantDefaultAbilitiesAndAttributes(InOwnerActor, InAvatarActor);
	GrantDefaultAbilitySets(InOwnerActor, InAvatarActor);

	RefreshAbilityInputBindings();

	// Notify Blueprints/listeners that init is complete
	OnInitAbilityActorInfo.Broadcast();
}

void UEGASAbilitySystemComponent::OnPawnControllerChanged(APawn* Pawn, AController* NewController)
{
	if (AbilityActorInfo && AbilityActorInfo->OwnerActor == Pawn && AbilityActorInfo->PlayerController != NewController)
	{
		if (!NewController)
		{
			return;
		}

		AbilityActorInfo->InitFromActor(AbilityActorInfo->OwnerActor.Get(), AbilityActorInfo->AvatarActor.Get(), this);
	}
}

// -------------------------------------------------------------------------
// Grant / Revoke
// -------------------------------------------------------------------------

FGameplayAbilitySpecHandle UEGASAbilitySystemComponent::GrantAbility(TSubclassOf<UGameplayAbility> AbilityClass, int32 AbilityLevel, int32 InputID, UObject* SourceObject)
{
	if (!HasAuthorityToGrant() || !AbilityClass)
	{
		return FGameplayAbilitySpecHandle();
	}

	if (!SourceObject)
	{
		SourceObject = GetOwnerActor();
	}

	FGameplayAbilitySpec AbilitySpec(AbilityClass, AbilityLevel, InputID, SourceObject);
	const FGameplayAbilitySpecHandle Handle = GiveAbility(AbilitySpec);

	if (Handle.IsValid())
	{
		AddedAbilityHandles.Add(FEGASMappedAbility(Handle, AbilitySpec));
		OnAbilityGranted.Broadcast(AbilityClass);
	}

	return Handle;
}

TArray<FGameplayAbilitySpecHandle> UEGASAbilitySystemComponent::GrantAbilities(const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses)
{
	TArray<FGameplayAbilitySpecHandle> Handles;
	Handles.Reserve(AbilityClasses.Num());

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : AbilityClasses)
	{
		Handles.Add(GrantAbility(AbilityClass));
	}

	return Handles;
}

void UEGASAbilitySystemComponent::RevokeAbility(FGameplayAbilitySpecHandle AbilityHandle)
{
	if (!HasAuthorityToGrant() || !AbilityHandle.IsValid())
	{
		return;
	}

	// OnRemoveAbility unbinds the Enhanced Input binding for us.
	ClearAbility(AbilityHandle);
	AddedAbilityHandles.RemoveAll([&AbilityHandle](const FEGASMappedAbility& Mapped) { return Mapped.Handle == AbilityHandle; });
}

void UEGASAbilitySystemComponent::RevokeAbilities(const TArray<FGameplayAbilitySpecHandle>& Handles)
{
	for (const FGameplayAbilitySpecHandle& Handle : Handles)
	{
		RevokeAbility(Handle);
	}
}

void UEGASAbilitySystemComponent::RevokeAllGrantedAbilities()
{
	if (!HasAuthorityToGrant())
	{
		return;
	}

	for (const FEGASMappedAbility& Mapped : AddedAbilityHandles)
	{
		if (Mapped.Handle.IsValid())
		{
			ClearAbility(Mapped.Handle);
		}
	}

	AddedAbilityHandles.Empty();
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

void UEGASAbilitySystemComponent::RevokeGameplayEffect(FActiveGameplayEffectHandle EffectHandle, int32 StacksToRemove)
{
	if (HasAuthorityToGrant() && EffectHandle.IsValid())
	{
		RemoveActiveGameplayEffect(EffectHandle, StacksToRemove);
	}
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

void UEGASAbilitySystemComponent::RevokeAttributeSet(UAttributeSet* AttributeSet)
{
	if (HasAuthorityToGrant() && AttributeSet)
	{
		RemoveSpawnedAttribute(AttributeSet);
		AddedAttributes.Remove(AttributeSet);
	}
}

bool UEGASAbilitySystemComponent::HasAuthorityToGrant() const
{
	return IsOwnerActorAuthoritative();
}

// -------------------------------------------------------------------------
// Grant Internals
// -------------------------------------------------------------------------

void UEGASAbilitySystemComponent::GrantDefaultAbilitiesAndAttributes(AActor* InOwnerActor, AActor* InAvatarActor)
{
	// --- Attribute resets ---
	if (bResetAttributesOnSpawn)
	{
		for (UAttributeSet* AttributeSet : AddedAttributes)
		{
			RemoveSpawnedAttribute(AttributeSet);
		}
		AddedAttributes.Empty(GrantedAttributes.Num());
	}

	// --- Ability resets ---
	if (bResetAbilitiesOnSpawn && IsOwnerActorAuthoritative())
	{
		for (const FEGASMappedAbility& MappedAbility : AddedAbilityHandles)
		{
			SetRemoveAbilityOnEnd(MappedAbility.Handle);
		}
		AddedAbilityHandles.Empty(DefaultAbilities.Num());
	}

	// --- Grant abilities ---
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : DefaultAbilities)
	{
		if (!AbilityClass)
		{
			continue;
		}

		if (IsOwnerActorAuthoritative() && ShouldGrantAbility(AbilityClass))
		{
			GrantAbility(AbilityClass, 1, INDEX_NONE, InOwnerActor);
		}
	}

	// --- Grant attributes ---
	for (const FEGASAttributeSetDefinition& AttributeSetDefinition : GrantedAttributes)
	{
		if (!AttributeSetDefinition.AttributeSet)
		{
			continue;
		}

		const bool bHasAttributeSet = GetAttributeSubobject(AttributeSetDefinition.AttributeSet) != nullptr;
		if (!bHasAttributeSet && InOwnerActor)
		{
			UAttributeSet* AttributeSet = NewObject<UAttributeSet>(InOwnerActor, AttributeSetDefinition.AttributeSet);
			if (AttributeSetDefinition.InitializationData)
			{
				AttributeSet->InitFromMetaDataTable(AttributeSetDefinition.InitializationData);
			}
			AddedAttributes.Add(AttributeSet);
			AddAttributeSetSubobject(AttributeSet);
		}
	}
}

void UEGASAbilitySystemComponent::GrantDefaultAbilitySets(AActor* InOwnerActor, AActor* InAvatarActor)
{
	if (!IsValid(InOwnerActor) || !IsValid(InAvatarActor) || !HasAuthorityToGrant())
	{
		return;
	}

	// Clean up previously granted set contents before re-granting
	DefaultAbilitySetHandles.TakeFromAbilitySystem(this);

	for (UEGASAbilitySet* AbilitySet : DefaultAbilitySets)
	{
		if (AbilitySet)
		{
			AbilitySet->GiveToAbilitySystem(this, DefaultAbilitySetHandles, InOwnerActor);
		}
	}
}

bool UEGASAbilitySystemComponent::ShouldGrantAbility(TSubclassOf<UGameplayAbility> InAbility, int32 InLevel)
{
	if (bResetAbilitiesOnSpawn)
	{
		return true;
	}

	// Prevent duplicates
	for (const FGameplayAbilitySpec& ActivatableAbility : GetActivatableAbilities())
	{
		if (ActivatableAbility.Ability && ActivatableAbility.Ability->GetClass() == InAbility && ActivatableAbility.Level == InLevel)
		{
			return false;
		}
	}

	return true;
}

void UEGASAbilitySystemComponent::GrantStartupEffects()
{
	if (!IsOwnerActorAuthoritative())
	{
		return;
	}

	// Remove previously applied startup effects
	for (const FActiveGameplayEffectHandle& AddedEffect : AddedEffects)
	{
		RemoveActiveGameplayEffect(AddedEffect);
	}

	FGameplayEffectContextHandle EffectContext = MakeEffectContext();
	EffectContext.AddSourceObject(this);

	AddedEffects.Empty(GrantedEffects.Num());

	for (const TSubclassOf<UGameplayEffect>& GameplayEffect : GrantedEffects)
	{
		if (!GameplayEffect)
		{
			continue;
		}

		FGameplayEffectSpecHandle NewHandle = MakeOutgoingSpec(GameplayEffect, 1, EffectContext);
		if (NewHandle.IsValid())
		{
			AddedEffects.Add(ApplyGameplayEffectSpecToTarget(*NewHandle.Data.Get(), this));
		}
	}
}

// -------------------------------------------------------------------------
// Query
// -------------------------------------------------------------------------

bool UEGASAbilitySystemComponent::IsAbilityActive(const FGameplayTagContainer& AbilityTags) const
{
	TArray<FGameplayAbilitySpec*> MatchingSpecs;
	GetActivatableGameplayAbilitySpecsByAllMatchingTags(AbilityTags, MatchingSpecs);

	for (const FGameplayAbilitySpec* Spec : MatchingSpecs)
	{
		if (Spec && Spec->IsActive())
		{
			return true;
		}
	}

	return false;
}

// -------------------------------------------------------------------------
// Gameplay Events
// -------------------------------------------------------------------------

void UEGASAbilitySystemComponent::SendGameplayEvent(FGameplayTag EventTag, FGameplayEventData Payload)
{
	HandleGameplayEvent(EventTag, &Payload);
}

void UEGASAbilitySystemComponent::SendGameplayEventToActor(AActor* Actor, FGameplayTag EventTag, FGameplayEventData Payload)
{
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Actor, EventTag, Payload);
}

void UEGASAbilitySystemComponent::ServerSendGameplayEvent_Implementation(FGameplayTag EventTag, bool bUseMulticast, FGameplayEventData Payload)
{
	HandleGameplayEvent(EventTag, &Payload);
	if (bUseMulticast)
	{
		MulticastGameplayEvent(EventTag, Payload);
	}
}

void UEGASAbilitySystemComponent::MulticastGameplayEvent_Implementation(FGameplayTag EventTag, FGameplayEventData Payload)
{
	HandleGameplayEvent(EventTag, &Payload);
}

// -------------------------------------------------------------------------
// Debug
// -------------------------------------------------------------------------

void UEGASAbilitySystemComponent::PrintAllGrantedAbilities(bool bAddLog, FLinearColor PrintColor, float Duration) const
{
	TArray<FGameplayAbilitySpecHandle> AbilitySpecs;
	GetAllAbilities(AbilitySpecs);

	for (const FGameplayAbilitySpecHandle& AbilitySpecHandle : AbilitySpecs)
	{
		if (const FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecFromHandle(AbilitySpecHandle))
		{
			UKismetSystemLibrary::PrintString(
				GetWorld(),
				FString::Printf(TEXT("Granted Ability: %s (Level: %d, Active: %s)"),
					*AbilitySpec->Ability->GetClass()->GetName(),
					AbilitySpec->Level,
					AbilitySpec->IsActive() ? TEXT("Yes") : TEXT("No")),
				true, bAddLog, PrintColor, Duration);
		}
	}
}

void UEGASAbilitySystemComponent::PrintAllGrantedTags(bool bAddLog, FLinearColor PrintColor, float Duration) const
{
	FGameplayTagContainer Tags;
	GetOwnedGameplayTags(Tags);

	for (const FGameplayTag& Tag : Tags)
	{
		UKismetSystemLibrary::PrintString(
			GetWorld(),
			FString::Printf(TEXT("Tag: %s"), *Tag.ToString()),
			true, bAddLog, PrintColor, Duration);
	}
}

// -------------------------------------------------------------------------
// Ability Callbacks
// -------------------------------------------------------------------------

void UEGASAbilitySystemComponent::OnAbilityActivatedCallback(UGameplayAbility* Ability)
{
	// Intentionally left for subclass/Blueprint extension
}

void UEGASAbilitySystemComponent::OnAbilityFailedCallback(const UGameplayAbility* Ability, const FGameplayTagContainer& Tags)
{
	// Intentionally left for subclass/Blueprint extension
}

void UEGASAbilitySystemComponent::OnAbilityEndedCallback(UGameplayAbility* Ability)
{
	if (Ability)
	{
		// Broadcast via tag if the ability has asset tags
		const FGameplayTagContainer& AbilityTags = Ability->GetAssetTags();
		for (const FGameplayTag& Tag : AbilityTags)
		{
			OnAbilityEnded.Broadcast(Tag);
		}
	}
}

// -------------------------------------------------------------------------
// Input Auto-Binding (GAS InputID System)
// -------------------------------------------------------------------------

void UEGASAbilitySystemComponent::OnGiveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	Super::OnGiveAbility(AbilitySpec);
	OnGiveAbilityDelegate.Broadcast(AbilitySpec);
	BindAbilityInput(AbilitySpec);
}

void UEGASAbilitySystemComponent::OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	UnbindAbilityInput(AbilitySpec);
	Super::OnRemoveAbility(AbilitySpec);
}

TSoftObjectPtr<UInputAction> UEGASAbilitySystemComponent::GetAbilityInputAction(const UGameplayAbility* Ability) const
{
	if (const UEGASGameplayAbility* EGASAbility = Cast<UEGASGameplayAbility>(Ability))
	{
		return EGASAbility->ActivationInputAction;
	}

	return TSoftObjectPtr<UInputAction>();
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
		if (!GetAbilityInputAction(AbilitySpec.Ability).IsNull())
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
		const TSoftObjectPtr<UInputAction> ActivationInputAction = GetAbilityInputAction(AbilitySpec.Ability);
		if (ActivationInputAction.IsNull())
		{
			continue;
		}

		if (ActivationInputAction.Get() == InputAction
			|| ActivationInputAction.ToSoftObjectPath() == InputAction->GetPathName())
		{
			return true;
		}
	}

	return false;
}

void UEGASAbilitySystemComponent::BindAbilityInput(FGameplayAbilitySpec& AbilitySpec)
{
	TSoftObjectPtr<UInputAction> ActivationInputAction = GetAbilityInputAction(AbilitySpec.Ability);
	if (ActivationInputAction.IsNull())
	{
		return;
	}

	UInputAction* InputAction = ActivationInputAction.LoadSynchronous();
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
