// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"
#include "EFWorldSettingsComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWorldSettingChanged, UEFModularSettingsBase*, Setting);

USTRUCT(BlueprintType)
struct FEFWorldSettingDefinition
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag Tag;

	UPROPERTY()
	TSoftObjectPtr<UEFModularSettingsBase> Template;

	UPROPERTY()
	FString CurrentValue;

	// Locked options of a MultiSelect setting, kept in sync by the server so
	// clients that built the setting from this definition receive lock state.
	UPROPERTY()
	TArray<FString> LockedOptions;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALEXTENDEDFRAMEWORK_API UEFWorldSettingsComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UEFWorldSettingsComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintAssignable, Category = "Modular Settings")
	FOnWorldSettingChanged OnSettingChanged;

	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	UEFModularSettingsBase* GetSettingByTag(FGameplayTag Tag) const;

	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	TArray<UEFModularSettingsBase*> GetSettingsByCategory(FName Category) const;

protected:
	virtual void BeginPlay() override;
	UEFModularSettingsBase* AddSettingFromTemplate_Local(UEFModularSettingsBase* Template);

	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Modular Settings")
	TArray<TObjectPtr<UEFModularSettingsBase>> DefaultSettings;

	UPROPERTY(ReplicatedUsing = OnRep_Settings)
	TArray<TObjectPtr<UEFModularSettingsBase>> Settings;

	UPROPERTY(ReplicatedUsing = OnRep_SettingDefinitions)
	TArray<FEFWorldSettingDefinition> SettingDefinitions;

	// Helper to find setting index by tag
	int32 FindSettingIndex(FGameplayTag Tag) const;

public:
	// Server-side update through the setting's RequestChange pipeline
	// (refresh -> validate -> set), then Apply. Returns false when the change
	// was rejected (unknown or locked option, out of range) or off-authority.
	UFUNCTION(BlueprintCallable, Category = "Modular Settings", BlueprintAuthorityOnly)
	bool UpdateSettingValue(FGameplayTag Tag, const FString& NewValue);

	// Keeps SettingDefinitions[].LockedOptions in sync on the server. Called by
	// UEFModularSettingsMultiSelect::OnOptionLockChanged; no-op off-authority.
	void UpdateDefinitionLockedOptions(FGameplayTag Tag, const TArray<FString>& InLockedOptions);

private:
	UFUNCTION()
	void OnRep_Settings();

	UFUNCTION()
	void OnRep_SettingDefinitions();

	void UpdateDefinitionCurrentValue(FGameplayTag Tag, const FString& NewValue);
};
