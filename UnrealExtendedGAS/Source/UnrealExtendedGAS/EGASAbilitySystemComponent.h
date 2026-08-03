#pragma once

#include "AbilitySystemComponent.h"
#include "CoreMinimal.h"
#include "EGASAbilitySet.h"
#include "GameplayAbilitySpec.h"
#include "InputActionValue.h"

#include "EGASAbilitySystemComponent.generated.h"

class UAttributeSet;
class UDataTable;
class UEnhancedInputComponent;
class UGameplayAbility;
class UGameplayEffect;
class UInputAction;

struct FEGASAbilityInputBindingHandles
{
	uint32 Pressed = 0;
	uint32 Completed = 0;
	uint32 Canceled = 0;
};

// ---------------------------------------------------------------------------
// Structs
// ---------------------------------------------------------------------------

/** Definition for a single attribute set to grant, with optional DataTable initializer. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAS_API FEGASAttributeSetDefinition
{
	GENERATED_BODY()

	/** Attribute Set class to grant. */
	UPROPERTY(EditAnywhere, Category = Attributes)
	TSubclassOf<UAttributeSet> AttributeSet;

	/** Optional DataTable to initialize the attributes with. */
	UPROPERTY(EditAnywhere, Category = Attributes, meta = (RequiredAssetDataTags = "RowStructure=/Script/GameplayAbilities.AttributeMetaData"))
	TObjectPtr<UDataTable> InitializationData = nullptr;
};

/** Internal tracking for a granted ability + its handle. */
USTRUCT()
struct UNREALEXTENDEDGAS_API FEGASMappedAbility
{
	GENERATED_BODY()

	FGameplayAbilitySpecHandle Handle;
	FGameplayAbilitySpec Spec;

	FEGASMappedAbility() {}

	FEGASMappedAbility(const FGameplayAbilitySpecHandle& InHandle, const FGameplayAbilitySpec& InSpec)
		: Handle(InHandle), Spec(InSpec)
	{
	}
};

// ---------------------------------------------------------------------------
// Delegates
// ---------------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEGASOnInitAbilityActorInfo);
DECLARE_MULTICAST_DELEGATE_OneParam(FEGASOnGiveAbility, FGameplayAbilitySpec&);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGASOnAbilityGranted, TSubclassOf<UGameplayAbility>, AbilityClass);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGASOnAbilityEnded, FGameplayTag, AbilityTag);

// ---------------------------------------------------------------------------
// Component
// ---------------------------------------------------------------------------

/**
 * UEGASAbilitySystemComponent
 *
 * Ability System Component with the whole grant lifecycle handled for you:
 * abilities, ability sets, attribute sets (with optional DataTable init),
 * startup gameplay effects, Enhanced Input auto-binding, respawn persistence
 * policy, and gameplay event dispatch.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup = (Abilities), meta = (BlueprintSpawnableComponent))
class UNREALEXTENDEDGAS_API UEGASAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	UEGASAbilitySystemComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// -----------------------------------------------------------------
	// Configuration
	// -----------------------------------------------------------------

	/** Abilities automatically granted when the ASC is initialized. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Unreal Extended GAS|Startup")
	TArray<TSubclassOf<UGameplayAbility>> DefaultAbilities;

	/** Ability sets granted when the ASC is initialized. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Unreal Extended GAS|Startup")
	TArray<TObjectPtr<UEGASAbilitySet>> DefaultAbilitySets;

	/** Attribute Sets to grant when the ASC is initialized, with optional DataTable init data. */
	UPROPERTY(EditDefaultsOnly, Category = "Unreal Extended GAS|Startup")
	TArray<FEGASAttributeSetDefinition> GrantedAttributes;

	/** Gameplay Effects to apply when the ASC begins play (startup buffs, passives, etc.). */
	UPROPERTY(EditDefaultsOnly, Category = "Unreal Extended GAS|Startup")
	TArray<TSubclassOf<UGameplayEffect>> GrantedEffects;

	/**
	 * If true, abilities are re-granted each time InitAbilityActorInfo is called
	 * (e.g., respawn, repossession). If false, abilities are only granted once.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Unreal Extended GAS|Startup")
	bool bResetAbilitiesOnSpawn = true;

	/**
	 * If true, attributes are re-initialized each time InitAbilityActorInfo is called.
	 * If false, attribute values persist across respawns.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Unreal Extended GAS|Startup")
	bool bResetAttributesOnSpawn = true;

	// -----------------------------------------------------------------
	// Delegates
	// -----------------------------------------------------------------

	/**
	 * Broadcast after InitAbilityActorInfo, once abilities and attributes have been granted.
	 * Fires multiple times (server init, client init, replication, etc.).
	 */
	UPROPERTY(BlueprintAssignable, Category = "Unreal Extended GAS|Abilities")
	FEGASOnInitAbilityActorInfo OnInitAbilityActorInfo;

	/** Broadcast when an ability is granted to this ASC. */
	UPROPERTY(BlueprintAssignable, Category = "Unreal Extended GAS|Abilities")
	FEGASOnAbilityGranted OnAbilityGranted;

	/** Broadcast when any ability ends, once per asset tag on that ability. */
	UPROPERTY(BlueprintAssignable, Category = "Unreal Extended GAS|Abilities")
	FEGASOnAbilityEnded OnAbilityEnded;

	/** Native delegate when an ability spec is given (for internal tracking). */
	FEGASOnGiveAbility OnGiveAbilityDelegate;

	// -----------------------------------------------------------------
	// Lifecycle
	// -----------------------------------------------------------------

	virtual void BeginPlay() override;
	virtual void BeginDestroy() override;

	//~ UAbilitySystemComponent overrides
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;

	// -----------------------------------------------------------------
	// Grant / Revoke
	// -----------------------------------------------------------------

	/** Grant a single ability class. SourceObject defaults to the owning actor. Returns the ability handle. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Unreal Extended GAS|Abilities")
	FGameplayAbilitySpecHandle GrantAbility(TSubclassOf<UGameplayAbility> AbilityClass, int32 AbilityLevel = 1, int32 InputID = -1, UObject* SourceObject = nullptr);

	/** Batch grant from an array, each at level 1. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Unreal Extended GAS|Abilities")
	TArray<FGameplayAbilitySpecHandle> GrantAbilities(const TArray<TSubclassOf<UGameplayAbility>>& AbilityClasses);

	/** Revoke a previously granted ability by handle. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Unreal Extended GAS|Abilities")
	void RevokeAbility(FGameplayAbilitySpecHandle AbilityHandle);

	/** Batch revoke from an array of handles. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Unreal Extended GAS|Abilities")
	void RevokeAbilities(const TArray<FGameplayAbilitySpecHandle>& Handles);

	/** Revoke all abilities granted through this component. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Unreal Extended GAS|Abilities")
	void RevokeAllGrantedAbilities();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Unreal Extended GAS|Effects")
	FActiveGameplayEffectHandle GrantGameplayEffect(TSubclassOf<UGameplayEffect> GameplayEffectClass, float EffectLevel = 1.0f);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Unreal Extended GAS|Effects")
	void RevokeGameplayEffect(FActiveGameplayEffectHandle EffectHandle, int32 StacksToRemove = -1);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Unreal Extended GAS|Attributes")
	UAttributeSet* GrantAttributeSet(TSubclassOf<UAttributeSet> AttributeSetClass);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Unreal Extended GAS|Attributes")
	void RevokeAttributeSet(UAttributeSet* AttributeSet);

	UFUNCTION(BlueprintPure, Category = "Unreal Extended GAS")
	bool HasAuthorityToGrant() const;

	// -----------------------------------------------------------------
	// Input Auto-Binding (GAS InputID System)
	// -----------------------------------------------------------------

	/** Rebuilds ability-owned Enhanced Input bindings against the current avatar input component. */
	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Input")
	void RefreshAbilityInputBindings();

	/** Removes all Enhanced Input bindings created by this ability system. */
	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Input")
	void ClearAbilityInputBindings();

	/** True when any granted ability auto-binds to the provided Enhanced Input action. */
	UFUNCTION(BlueprintPure, Category = "Unreal Extended GAS|Input")
	bool HasAbilityBindingForInputAction(const UInputAction* InputAction) const;

	// -----------------------------------------------------------------
	// Query
	// -----------------------------------------------------------------

	/** Returns whether an ability matching all of the given tags is currently active on the ASC. */
	UFUNCTION(BlueprintPure, Category = "Unreal Extended GAS|Abilities")
	bool IsAbilityActive(const FGameplayTagContainer& AbilityTags) const;

	// -----------------------------------------------------------------
	// Gameplay Events
	// -----------------------------------------------------------------

	/** Send a gameplay event to this ASC (local). */
	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Events")
	void SendGameplayEvent(FGameplayTag EventTag, FGameplayEventData Payload);

	/** Send a gameplay event to a specific actor's ASC. */
	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Events")
	void SendGameplayEventToActor(AActor* Actor, FGameplayTag EventTag, FGameplayEventData Payload);

	/** Send a gameplay event to this ASC via server RPC. Optionally multicasts. */
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Unreal Extended GAS|Events")
	void ServerSendGameplayEvent(FGameplayTag EventTag, bool bUseMulticast = false, FGameplayEventData Payload = FGameplayEventData());

	/** Multicast a gameplay event to all clients. */
	UFUNCTION(BlueprintCallable, NetMulticast, Reliable, Category = "Unreal Extended GAS|Events")
	void MulticastGameplayEvent(FGameplayTag EventTag, FGameplayEventData Payload);

	// -----------------------------------------------------------------
	// Debug
	// -----------------------------------------------------------------

	/** Print all granted abilities to screen/log. */
	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Debug")
	void PrintAllGrantedAbilities(bool bAddLog = false, FLinearColor PrintColor = FLinearColor::Green, float Duration = 0.f) const;

	/** Print all owned gameplay tags to screen/log. */
	UFUNCTION(BlueprintCallable, Category = "Unreal Extended GAS|Debug")
	void PrintAllGrantedTags(bool bAddLog = false, FLinearColor PrintColor = FLinearColor::Green, float Duration = 0.f) const;

	// -----------------------------------------------------------------
	// Ability Callbacks (forwarded from ASC delegates)
	// -----------------------------------------------------------------

	virtual void OnAbilityActivatedCallback(UGameplayAbility* Ability);
	virtual void OnAbilityFailedCallback(const UGameplayAbility* Ability, const FGameplayTagContainer& Tags);
	virtual void OnAbilityEndedCallback(UGameplayAbility* Ability);

	// -----------------------------------------------------------------
	// Grant Internals (virtual for subclass override)
	// -----------------------------------------------------------------

	/** Called from InitAbilityActorInfo to grant configured abilities and attributes. */
	virtual void GrantDefaultAbilitiesAndAttributes(AActor* InOwnerActor, AActor* InAvatarActor);

	/** Called from InitAbilityActorInfo to grant configured ability sets. */
	virtual void GrantDefaultAbilitySets(AActor* InOwnerActor, AActor* InAvatarActor);

	/** Returns true if an ability should be granted (prevents duplicates when bResetAbilitiesOnSpawn is false). */
	virtual bool ShouldGrantAbility(TSubclassOf<UGameplayAbility> InAbility, int32 InLevel = 1);

protected:
	//~ UAbilitySystemComponent overrides
	virtual void OnGiveAbility(FGameplayAbilitySpec& AbilitySpec) override;
	virtual void OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec) override;

	/**
	 * Resolve the Enhanced Input action a granted ability should auto-bind to.
	 * Base implementation reads UEGASGameplayAbility::ActivationInputAction; override
	 * to support ability bases declared outside this plugin.
	 */
	virtual TSoftObjectPtr<UInputAction> GetAbilityInputAction(const UGameplayAbility* Ability) const;

	/** Apply startup effects from the GrantedEffects list. */
	virtual void GrantStartupEffects();

	/** Resolve the owning pawn's UEnhancedInputComponent. Returns nullptr for NPCs or if unavailable. */
	UEnhancedInputComponent* GetEnhancedInputComponent() const;

	// Cached handles for cleanup
	UPROPERTY(Transient)
	TArray<FEGASMappedAbility> AddedAbilityHandles;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAttributeSet>> AddedAttributes;

	UPROPERTY(Transient)
	TArray<FActiveGameplayEffectHandle> AddedEffects;

	UPROPERTY(Transient)
	FEGASAbilitySetGrantedHandles DefaultAbilitySetHandles;

private:
	void BindAbilityInput(FGameplayAbilitySpec& AbilitySpec);
	void UnbindAbilityInput(FGameplayAbilitySpec& AbilitySpec);
	int32 GetOrCreateInputID(UInputAction* InputAction);
	void HandleAbilityInputPressed(const FInputActionValue& Value, int32 InputID);
	void HandleAbilityInputReleased(const FInputActionValue& Value, int32 InputID);
	void RemoveBindingsForInputID(int32 InputID);

	/** Reinit cached actor info when the pawn's controller changes. */
	UFUNCTION()
	void OnPawnControllerChanged(APawn* Pawn, AController* NewController);

	/** Maps UInputAction* -> auto-assigned InputID. */
	TMap<TObjectPtr<UInputAction>, int32> InputActionToIDMap;

	/** Maps InputID -> Enhanced Input binding handles. */
	TMap<int32, FEGASAbilityInputBindingHandles> InputIDBindingHandles;

	TWeakObjectPtr<UEnhancedInputComponent> BoundInputComponent;

	/** Counter for auto-assigning InputIDs. Starts at 100 to avoid conflicts with manual IDs. */
	int32 NextAutoInputID = 100;
};
