// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Components/ActorComponent.h"
#include "EFExtendedAbilityComponent.generated.h"


class UEFAttributeComponent;
class UEFExtendedAbility;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActiveGameplayTagsUpdate , UEFExtendedAbility* , Instigator , FGameplayTag , ChangedTag);


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) , Blueprintable , BlueprintType)
class UNREALEXTENDEDFRAMEWORK_API UEFExtendedAbilityComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	// add AddTag Function , Check Has Tag Function

	UEFExtendedAbilityComponent();
	

	UFUNCTION(BlueprintCallable ,meta = (DefaultToSelf = Instigator) , Category="ExtendedAbilities")
	UEFExtendedAbility* AddExtendedAbility(AActor* Instigator , const TSubclassOf<UEFExtendedAbility> AbilityClass);

	UFUNCTION(BlueprintCallable , Category="ExtendedAbilities")
	bool RemoveExtendedAbilityByName(const FName AbilityName);

	UFUNCTION(BlueprintCallable , Category="ExtendedAbilities")
	bool RemoveExtendedAbilityByClass(const TSubclassOf<UEFExtendedAbility> AbilityClass);


	
	UFUNCTION(BlueprintCallable ,meta = (DefaultToSelf = Instigator), Category="ExtendedAbilities")
	bool StartExtendedAbilityByName(AActor* Instigator , const FName AbilityName);

	UFUNCTION(BlueprintCallable ,meta = (DefaultToSelf = Instigator) , Category="ExtendedAbilities")
	bool StopExtendedAbilityByName(AActor* Instigator ,const FName AbilityName);


	
	UFUNCTION(BlueprintCallable ,meta = (DefaultToSelf = Instigator) , Category="ExtendedAbilities")
	bool StartExtendedAbilityByClass(AActor* Instigator , const TSubclassOf<UEFExtendedAbility> AbilityClass);
	
	UFUNCTION(BlueprintCallable ,meta = (DefaultToSelf = Instigator) , Category="ExtendedAbilities")
	bool StopExtendedAbilityByClass(AActor* Instigator , const TSubclassOf<UEFExtendedAbility> AbilityClass);

	
	UFUNCTION(BlueprintCallable ,meta = (DefaultToSelf = Instigator) , Category="ExtendedAbilities")
	bool StartExtendedAbilityByIndex(AActor* Instigator , const int32 Index = 0);

	UFUNCTION(BlueprintCallable ,meta = (DefaultToSelf = Instigator) , Category="ExtendedAbilities")
	bool StopExtendedAbilityByIndex(AActor* Instigator ,const int32 Index = 0);

	
	UFUNCTION(BlueprintCallable , Category="ExtendedAbilities|Tags")
	bool AddExtendedActiveTag(const FGameplayTag Tag);

	UFUNCTION(BlueprintCallable , Category="ExtendedAbilities|Tags")
	void AppendExtendedActiveTags(const FGameplayTagContainer GrantedTags);

	UFUNCTION(BlueprintCallable , Category="ExtendedAbilities|Tags")
	bool RemoveExtendedActiveTag(const FGameplayTag Tag);

	UFUNCTION(BlueprintCallable , Category="ExtendedAbilities|Tags")
	void RemoveExtendedActiveTags(const FGameplayTagContainer RemovedTags);
	
	
	
	UPROPERTY(BlueprintAssignable)
	FOnActiveGameplayTagsUpdate OnExtendedGameplayTagAdded;
	
	UPROPERTY(BlueprintAssignable)
	FOnActiveGameplayTagsUpdate OnExtendedGameplayTagRemoved;

protected:

	UPROPERTY()
	UEFAttributeComponent* AttributeComponent = nullptr;

	UPROPERTY(EditAnywhere , Category="ExtendedAbilities|Tags")
	FGameplayTagContainer ActiveGameplayTags;

	UPROPERTY(EditDefaultsOnly , Category="ExtendedAbilities")
	TArray<TSubclassOf<UEFExtendedAbility>> StartupExtendedAbilities;

	UPROPERTY()
	TArray<UEFExtendedAbility*> ExtendedAbilities;

	UPROPERTY()
	TArray<UEFExtendedAbility*> ExtendedTickAbilities;
	
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;

public:

	//ExpandEnumAsExecs
	UFUNCTION(BlueprintCallable ,meta = (ExpandBoolAsExecs = "ReturnValue" ) , Category="ExtendedAbilities|Tags")
	FORCEINLINE bool CheckTagInActiveTagsExec(const FGameplayTag Tag) { return ActiveGameplayTags.HasTag(Tag); }

	UFUNCTION(BlueprintCallable ,meta = (ExpandBoolAsExecs = "ReturnValue" ) , Category="ExtendedAbilities|Tags")
	FORCEINLINE bool CheckHasAnyTagInActiveTagsExec(const FGameplayTagContainer Tags) { return  ActiveGameplayTags.HasAny(Tags); }

	UFUNCTION(BlueprintPure  , Category="ExtendedAbilities|Tags")
	FORCEINLINE bool CheckTagInActiveTags(const FGameplayTag Tag) const { return ActiveGameplayTags.HasTag(Tag); }

	UFUNCTION(BlueprintPure , Category="ExtendedAbilities|Tags")
	FORCEINLINE bool CheckHasAnyTagInActiveTags(const FGameplayTagContainer Tags) const { return ActiveGameplayTags.HasAny(Tags); }

	UFUNCTION(BlueprintPure , Category="ExtendedAbilities|Tags")
	FORCEINLINE FGameplayTagContainer GetActiveTags() const { return ActiveGameplayTags; }
	
	UFUNCTION(BlueprintPure , Category="ExtendedAbilities")
	FORCEINLINE UEFAttributeComponent* GetAttributeComponent() const { return AttributeComponent; }

	UFUNCTION(BlueprintPure , Category="ExtendedAbilities")
	FORCEINLINE TArray<UEFExtendedAbility*>& GetExtendedAbilities() { return ExtendedAbilities; }

	UFUNCTION(BlueprintPure , Category="ExtendedAbilities")
	bool GetHasAbility(FName AbilityName) const;

	UFUNCTION(BlueprintPure , Category="ExtendedAbilities")
	UEFExtendedAbility* GetExtendedAbilityByIndex(const int32 Index) const;

	UFUNCTION(BlueprintPure , Category="ExtendedAbilities")
	UEFExtendedAbility* GetExtendedAbilityByName(const FName AbilityName) const;

	UFUNCTION(BlueprintPure , Category="ExtendedAbilities")
	UEFExtendedAbility* GetExtendedAbilityByClass(const TSubclassOf<UEFExtendedAbility> AbilityClass) const;
	
	// Debug
	UFUNCTION(BlueprintCallable , Category="ExtendedAbilities|Debug")
	void PrintActiveTags(const float Time , FColor DisplayColor) const;
};



