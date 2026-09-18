// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/HitResult.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "Templates/SubclassOf.h"

#include "EGInteractionSystemTypes.generated.h"

class AActor;
class UEGInteraction;
class UEGInteractionProviderComponent;
class UEGInteractionSystemComponent;
class UPrimitiveComponent;
class UTexture2D;

/**
 * Interaction System
 *
 * A second interaction stack, independent of the Interaction/ folder. It borrows the execution
 * model of the ability system without depending on it:
 *
 *  - A target carries a UEGInteractionProviderComponent with an authored list of definitions
 *    (instanced structs). Each definition names an input slot and a UEGInteraction class.
 *  - The interactor carries a UEGInteractionSystemComponent. It traces, resolves the provider,
 *    grants one UEGInteraction instance per definition, binds the input slots, and publishes
 *    prompts. Pressing a slot activates the granted instance; the server re-derives everything.
 *  - UEGInteraction owns the behaviour and runs latent UGameplayTask work through the system
 *    component, which is a UGameplayTasksComponent.
 *
 * Definitions are data. Interactions are behaviour. Tasks are time.
 */

UENUM(BlueprintType)
enum class EEGInteractionNetPolicy : uint8
{
	/** Runs on the interactor's client at once and on the server after validation. The server decides. */
	LocalPredicted UMETA(DisplayName = "Local Predicted"),
	/** Only the server instance runs. The client hears back through OnServerResult and the end notification. */
	ServerOnly UMETA(DisplayName = "Server Only"),
	/** Runs on the interactor's client only and never reaches the server. UI and inspection. */
	LocalOnly UMETA(DisplayName = "Local Only")
};

UENUM(BlueprintType)
enum class EEGInteractionEventPhase : uint8
{
	Started,
	Completed,
	Cancelled
};

UENUM(BlueprintType)
enum class EEGInteractionSystemTraceMode : uint8
{
	/** Camera-centre line trace only. */
	CameraCenter UMETA(DisplayName = "Camera Center Line Trace"),
	/** Line trace first, sphere sweep as a fallback. */
	SphereSweep UMETA(DisplayName = "Sphere Sweep")
};

/**
 * One authored interaction on a provider. Subclass this struct to carry parameters for a
 * specific interaction class; the instance reads them back with UEGInteraction::GetDefinition<T>().
 */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGInteractionDefinition
{
	GENERATED_BODY()

	/** Unique within its provider. The server matches activation requests by this tag. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FGameplayTag Id;

	/** Behaviour class instantiated on the interactor when this definition is granted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TSubclassOf<UEGInteraction> InteractionClass;

	/** Default prompt text. Override UEGInteraction::GetPresentation for anything dynamic. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText SubText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** Which input slot on the interactor fires this. Slots map to input actions on the system component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FGameplayTag InputTag;

	/** When two granted definitions share a slot, the highest priority wins the press. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	int32 Priority = 0;

	/** Restrict this definition to one component of the owner, by component name. None means the whole actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FName PartComponent;

	/** Designer switch. Runtime gating belongs in UEGInteraction::CanActivate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	bool bEnabled = true;
};

/** Parameters for hold-to-complete interactions. Only hold interactions carry a hold time. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGInteractionDef_Hold : public FEGInteractionDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction", meta = (ClampMin = "0.05"))
	float HoldTime = 1.0f;
};

/** Who is asking, and what they hit. Passed to providers when gathering. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGInteractionQuery
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<AActor> Interactor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UEGInteractionSystemComponent> System = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FHitResult Hit;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UPrimitiveComponent> HitComponent = nullptr;

	/** True while the server re-derives a client request. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bServer = false;
};

/** One granted definition, stored on the UEGInteraction instance that was created for it. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGInteractionSpec
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	int32 Handle = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FGameplayTag Id;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UEGInteractionProviderComponent> Provider = nullptr;

	/** Copy of the authored definition. FEGInteractionDefinition or a subclass. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FInstancedStruct Definition;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FHitResult Hit;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UPrimitiveComponent> HitComponent = nullptr;

	const FEGInteractionDefinition* GetBase() const { return Definition.GetPtr<FEGInteractionDefinition>(); }
};

/** What the interactor's HUD draws for one granted interaction. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGInteractionPrompt
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	int32 Handle = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FGameplayTag Id;

	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	FText Text;

	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	FText SubText;

	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	TObjectPtr<UTexture2D> Icon = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FGameplayTag InputTag;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	int32 Priority = 0;

	/** False draws the line greyed out. */
	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	bool bEnabled = true;

	/** False removes the line entirely. */
	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	bool bVisible = true;

	/** 0..1 while a hold runs, else 0. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float Progress = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<AActor> Target = nullptr;

	/** Everything except Progress, which is published on its own channel. */
	bool IsSameAs(const FEGInteractionPrompt& Other) const
	{
		return Handle == Other.Handle
			&& Id == Other.Id
			&& InputTag == Other.InputTag
			&& Priority == Other.Priority
			&& bEnabled == Other.bEnabled
			&& bVisible == Other.bVisible
			&& Icon == Other.Icon
			&& Target == Other.Target
			&& Text.EqualTo(Other.Text)
			&& SubText.EqualTo(Other.SubText);
	}
};

/** Replicated "somebody is using this" marker kept by the provider. */
USTRUCT()
struct FEGInteractionClaim
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag Id;

	UPROPERTY()
	TObjectPtr<AActor> Holder = nullptr;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGInteractionPromptsChanged, const TArray<FEGInteractionPrompt>&, Prompts);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEGInteractionPromptProgressChanged, int32, Handle, float, Progress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEGInteractionSystemFocusChanged, AActor*, Target, UEGInteractionProviderComponent*, Provider);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGInteractionInstanceStarted, UEGInteraction*, Interaction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEGInteractionInstanceEnded, UEGInteraction*, Interaction, bool, bCancelled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEGInteractionActivationRejected, UEGInteraction*, Interaction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FEGInteractionProviderEvent, FGameplayTag, Id, AActor*, Interactor, EEGInteractionEventPhase, Phase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEGInteractionProviderFocus, bool, bFocused, UPrimitiveComponent*, HitComponent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FEGInteractionClaimsChanged);
