// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"
#include "EFWorldSettingsComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorldSettingChanged, UEFModularSettingsBase*, Setting);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALEXTENDEDFRAMEWORK_API UEFWorldSettingsComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UEFWorldSettingsComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags) override;

	UPROPERTY(BlueprintAssignable, Category = "Modular Settings")
	FOnWorldSettingChanged OnSettingChanged;

	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	UEFModularSettingsBase* GetSettingByTag(FGameplayTag Tag) const;

	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	TArray<UEFModularSettingsBase*> GetSettingsByCategory(FName Category) const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Modular Settings")
	TArray<TObjectPtr<UEFModularSettingsBase>> DefaultSettings;

	UPROPERTY(ReplicatedUsing = OnRep_Settings)
	TArray<TObjectPtr<UEFModularSettingsBase>> Settings;

	// Helper to find setting index by tag
	int32 FindSettingIndex(FGameplayTag Tag) const;

public:
	// Server-side update
	UFUNCTION(BlueprintCallable, Category = "Modular Settings", BlueprintAuthorityOnly)
	void UpdateSettingValue(FGameplayTag Tag, const FString& NewValue);

private:
	UFUNCTION()
	void OnRep_Settings();
};
