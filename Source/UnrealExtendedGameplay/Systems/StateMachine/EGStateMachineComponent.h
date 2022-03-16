// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "EGStateMachineComponent.generated.h"


DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStateChangedDelegate , const FGameplayTag& ,  NewStateTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInitStateDelegate , const FGameplayTag& , InitStateTag );
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEndStateDelegate , const FGameplayTag& , EndStateTag );
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTickStateDelegate ,float , DeltaTime ,  const FGameplayTag& , TickTag );

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALEXTENDEDGAMEPLAY_API UEGStateMachineComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UEGStateMachineComponent();

	
	UPROPERTY(EditDefaultsOnly , BlueprintReadWrite)
	FGameplayTag InitialStateTag;
	
	UPROPERTY(EditDefaultsOnly)
	FGameplayTagContainer PossibleStateTags;

	
	UPROPERTY(EditDefaultsOnly ,BlueprintReadWrite)
	int32 StateHistorySize = 6;

	UPROPERTY(EditDefaultsOnly , BlueprintReadWrite)
	bool bDebug = false;

	
	UFUNCTION(BlueprintCallable)
	bool SwitchState(FGameplayTag stateTag);

protected:

	FGameplayTag StateTag;
	TArray<FGameplayTag> StateHistory;
	
	bool bCanTickState;

	void InitState();
	void TickState(float DeltaTime);
	void EndState();
	bool CanAcceptState(FGameplayTag stateTag) const;
	
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:

	UPROPERTY(BlueprintAssignable)
	FOnInitStateDelegate OnInitStateDelegate;

	UPROPERTY(BlueprintAssignable)
	FOnEndStateDelegate OnEndStateDelegate;

	UPROPERTY(BlueprintAssignable)
	FOnStateChangedDelegate OnStateChangedDelegate;

	UPROPERTY(BlueprintAssignable)
	FOnTickStateDelegate OnTickStateDelegate;

	
	UFUNCTION(BlueprintPure)
	FORCEINLINE FGameplayTag GetCurrentStateTag() const { return StateTag; }
	UFUNCTION(BlueprintPure)
	FORCEINLINE TArray<FGameplayTag> GetStateHistory() const { return StateHistory; }
};
