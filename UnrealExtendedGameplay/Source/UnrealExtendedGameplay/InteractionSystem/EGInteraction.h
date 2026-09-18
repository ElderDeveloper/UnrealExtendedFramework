// Copyright Kemal Erdem YILMAZ. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EGInteractionSystemTypes.h"
#include "GameplayTaskOwnerInterface.h"
#include "UObject/Object.h"

#include "EGInteraction.generated.h"

class AController;
class APawn;
class APlayerController;
class UEGInteractionProviderComponent;
class UEGInteractionSystemComponent;
class UGameplayTask;
class UGameplayTasksComponent;
class UPrimitiveComponent;
class USkeletalMeshComponent;

/** Everything about the interactor, cached when the instance is created. */
USTRUCT(BlueprintType)
struct UNREALEXTENDEDGAMEPLAY_API FEGInteractionActorInfo
{
	GENERATED_BODY()

	/** The actor that carries the system component. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<AActor> OwnerActor = nullptr;

	/** OwnerActor as a pawn, when it is one. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<APawn> Pawn = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UEGInteractionSystemComponent> System = nullptr;

	/** First skeletal mesh on the owner. Montage tasks default to it. */
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<USkeletalMeshComponent> SkeletalMesh = nullptr;

	void Init(UEGInteractionSystemComponent* InSystem);

	/** Live lookups: possession can change after Init. */
	AController* GetController() const;
	APlayerController* GetPlayerController() const;
	bool IsLocallyControlled() const;
	bool HasAuthority() const;
};

/**
 * UEGInteraction
 *
 * The ability analog. One instance per granted definition, created on the interactor's
 * system component the moment the definition is granted and alive until it is revoked, so
 * CanActivate and GetPresentation always run on an object that knows its owner and its target.
 *
 * Logic lives here. Override ActivateInteraction, do the work, start tasks for anything that
 * takes time, and finish with EndInteraction. Tasks are UGameplayTasks owned by this object and
 * ticked by the system component; every task still running when the interaction ends is torn
 * down with it.
 *
 * Network shape follows NetPolicy. A LocalPredicted instance runs on the interactor's client
 * at once while the server creates its own instance after validation; OnServerResult tells the
 * client instance whether it was accepted. ServerOnly runs only on the server. LocalOnly never
 * leaves the client.
 *
 * Definitions are instanced structs. GetDefinition<T>() returns the typed parameters this
 * instance was granted with, or null when the authored struct is a different type.
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class UNREALEXTENDEDGAMEPLAY_API UEGInteraction : public UObject, public IGameplayTaskOwnerInterface
{
	GENERATED_BODY()

public:
	UEGInteraction();

	// -----------------------------------------------------------------
	// Class configuration
	// -----------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Extended|Interaction System")
	EEGInteractionNetPolicy NetPolicy = EEGInteractionNetPolicy::LocalPredicted;

	/** Keep a disabled prompt line when CanActivate is false instead of hiding it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Extended|Interaction System")
	bool bShowPromptWhenUnavailable = false;

	/** Cancel the running instance when the interactor's focus leaves the target. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Extended|Interaction System")
	bool bCancelOnFocusLost = true;

	// -----------------------------------------------------------------
	// Lifecycle. Override these.
	// -----------------------------------------------------------------

	/** Runtime gating. Runs on both sides, so every term must be state both sides agree on. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Interaction System")
	bool CanActivate() const;
	virtual bool CanActivate_Implementation() const;

	/** The prompt line for this instance. Default fills it from the definition and CanActivate. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Extended|Interaction System")
	FEGInteractionPrompt GetPresentation() const;
	virtual FEGInteractionPrompt GetPresentation_Implementation() const;

	/** The work. Start tasks for anything latent and call EndInteraction when done. */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|Interaction System")
	void ActivateInteraction();
	virtual void ActivateInteraction_Implementation() {}

	/** The slot that activated this instance was released. Only while active. */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|Interaction System")
	void OnInputReleased();
	virtual void OnInputReleased_Implementation() {}

	/** LocalPredicted client instances: the server accepted or refused the request. */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|Interaction System")
	void OnServerResult(bool bAccepted);
	virtual void OnServerResult_Implementation(bool bAccepted) {}

	/** Cleanup hook. Tasks are already ended when this runs. */
	UFUNCTION(BlueprintNativeEvent, Category = "Extended|Interaction System")
	void OnEndInteraction(bool bCancelled);
	virtual void OnEndInteraction_Implementation(bool bCancelled) {}

	// -----------------------------------------------------------------
	// Actions
	// -----------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction System")
	void EndInteraction(bool bCancelled = false);

	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction System")
	void CancelInteraction() { EndInteraction(true); }

	/** Publish 0..1 progress for this instance's prompt line. */
	UFUNCTION(BlueprintCallable, Category = "Extended|Interaction System")
	void SetHoldProgress(float Progress);

	/** Cosmetic multicast through the provider. Authority only. */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Extended|Interaction System")
	void SendProviderEvent(EEGInteractionEventPhase Phase);

	// -----------------------------------------------------------------
	// Queries
	// -----------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	bool IsActive() const { return bActive; }

	/** A LocalPredicted client instance that has not heard back from the server yet. */
	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	bool IsPredicting() const { return bPredicting; }

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	bool HasAuthority() const { return ActorInfo.HasAuthority(); }

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	bool IsLocallyControlled() const { return ActorInfo.IsLocallyControlled(); }

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	int32 GetActivationId() const { return ActivationId; }

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	FEGInteractionActorInfo GetActorInfo() const { return ActorInfo; }

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	AActor* GetInteractorActor() const { return ActorInfo.OwnerActor; }

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	APawn* GetInteractorPawn() const { return ActorInfo.Pawn; }

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	APlayerController* GetPlayerController() const { return ActorInfo.GetPlayerController(); }

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	UEGInteractionSystemComponent* GetInteractionSystem() const { return ActorInfo.System; }

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	FEGInteractionSpec GetSpec() const { return Spec; }

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	AActor* GetTargetActor() const;

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	UEGInteractionProviderComponent* GetProvider() const { return Spec.Provider; }

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	UPrimitiveComponent* GetHitComponent() const { return Spec.HitComponent; }

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	FHitResult GetHitResult() const { return Spec.Hit; }

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	FGameplayTag GetInteractionId() const { return Spec.Id; }

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	FGameplayTag GetInputTag() const;

	/** Input slot captured at activation, unaffected by subsequent definition edits. */
	FGameplayTag GetActivationInputTag() const { return ActivationInputTag; }

	/** The authored definition as an instanced struct. Break it with the struct-utils nodes in Blueprint. */
	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	FInstancedStruct GetDefinitionStruct() const { return Spec.Definition; }

	/** The base fields of the authored definition. */
	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	FEGInteractionDefinition GetDefinitionBase() const;

	template <class T>
	const T* GetDefinition() const
	{
		return Spec.Definition.GetPtr<T>();
	}

	UFUNCTION(BlueprintPure, Category = "Extended|Interaction System")
	bool HasAnyActiveTasks() const { return ActiveTasks.Num() > 0; }

	/** True when a task other than the given one is still running. For tasks that ask "am I the last?" from their own completion. */
	bool HasActiveTasksOtherThan(const UGameplayTask* Task) const;

	// -----------------------------------------------------------------
	// Native delegates, for tasks
	// -----------------------------------------------------------------

	DECLARE_MULTICAST_DELEGATE(FEGInteractionNativeSimple);
	DECLARE_MULTICAST_DELEGATE_OneParam(FEGInteractionNativeEnded, bool /*bCancelled*/);

	FEGInteractionNativeSimple OnInputReleasedNative;
	FEGInteractionNativeEnded OnEndedNative;

	// -----------------------------------------------------------------
	// Driven by the system component
	// -----------------------------------------------------------------

	void InitInteraction(UEGInteractionSystemComponent* InSystem, const FEGInteractionSpec& InSpec);
	void UpdateSpecHit(const FHitResult& InHit, UPrimitiveComponent* InHitComponent);
	void UpdateSpecDefinition(const FInstancedStruct& InDefinition);
	void BeginActivation(int32 InActivationId, bool bInPredicting);
	void BeginServerPending(int32 InActivationId);
	bool IsServerPending() const { return bServerPending; }
	void HandleInputReleased();
	void HandleServerResult(bool bAccepted);
	void HandleServerEnded(bool bCancelled);
	void HandleRevoked();

	// -----------------------------------------------------------------
	// UObject
	// -----------------------------------------------------------------

	virtual UWorld* GetWorld() const override;
	virtual bool IsSupportedForNetworking() const override { return false; }

	// -----------------------------------------------------------------
	// IGameplayTaskOwnerInterface
	// -----------------------------------------------------------------

	virtual UGameplayTasksComponent* GetGameplayTasksComponent(const UGameplayTask& Task) const override;
	virtual AActor* GetGameplayTaskOwner(const UGameplayTask* Task) const override;
	virtual AActor* GetGameplayTaskAvatar(const UGameplayTask* Task) const override;
	virtual void OnGameplayTaskInitialized(UGameplayTask& Task) override;
	virtual void OnGameplayTaskActivated(UGameplayTask& Task) override;
	virtual void OnGameplayTaskDeactivated(UGameplayTask& Task) override;

protected:
	UPROPERTY(BlueprintReadOnly, Transient, Category = "Extended|Interaction System")
	FEGInteractionActorInfo ActorInfo;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Extended|Interaction System")
	FEGInteractionSpec Spec;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameplayTask>> ActiveTasks;

	void EndAllTasks();

private:
	void EndInteractionInternal(bool bCancelled, bool bNotifyServer);

	int32 ActivationId = 0;
	FGameplayTag ActivationInputTag;
	bool bActive = false;
	bool bPredicting = false;
	bool bServerPending = false;
	bool bEnding = false;
};
