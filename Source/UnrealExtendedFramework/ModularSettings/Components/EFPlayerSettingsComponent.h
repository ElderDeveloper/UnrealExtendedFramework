// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "UnrealExtendedFramework/ModularSettings/Settings/EFModularSettingsBase.h"
#include "EFPlayerSettingsComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerSettingChanged, UEFModularSettingsBase*, Setting);

// For asset-based settings (loaded from containers/data assets)
USTRUCT(BlueprintType)
struct FEFPlayerSettingDefinition
{
	GENERATED_BODY()

	// Unique key for the setting
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag Tag;

	// A template object (typically from a data asset/container) that will be duplicated on both server and clients.
	// Replicating a soft object path lets clients resolve it and DuplicateObject to build the same subobject graph.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UEFModularSettingsBase> Template;
};

// For runtime-created settings (created via AddBoolSetting, etc.)
// Contains all the data needed to reconstruct settings on clients
USTRUCT(BlueprintType)
struct FEFRuntimeSettingDefinition
{
	GENERATED_BODY()

	UPROPERTY()
	FGameplayTag Tag;

	UPROPERTY()
	FText DisplayName;

	UPROPERTY()
	FName ConfigCategory = NAME_None;

	UPROPERTY()
	uint8 SettingType = 0; // 0: Bool, 1: Float, 2: MultiSelect

	// Bool settings
	UPROPERTY()
	bool BoolDefaultValue = false;

	UPROPERTY()
	bool BoolValue = false;

	// Float settings
	UPROPERTY()
	float FloatDefaultValue = 0.f;

	UPROPERTY()
	float FloatValue = 0.f;

	UPROPERTY()
	float FloatMin = 0.f;

	UPROPERTY()
	float FloatMax = 1.f;

	// MultiSelect settings
	UPROPERTY()
	TArray<FString> MultiSelectValues;

	UPROPERTY()
	TArray<FText> MultiSelectDisplayNames;

	UPROPERTY()
	FString MultiSelectDefaultValue;

	UPROPERTY()
	int32 MultiSelectIndex = 0;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALEXTENDEDFRAMEWORK_API UEFPlayerSettingsComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UEFPlayerSettingsComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags) override;

	UPROPERTY(BlueprintAssignable, Category = "Modular Settings")
	FOnPlayerSettingChanged OnSettingChanged;

	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	UEFModularSettingsBase* GetSettingByTag(FGameplayTag Tag) const;

	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	TArray<UEFModularSettingsBase*> GetSettingsByCategory(FName Category) const;
	
	// Creates and adds a new Bool setting at runtime.
	// Returns the created setting, or existing setting if Tag already exists.
	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	UEFModularSettingsBool* AddBoolSetting(FGameplayTag Tag,FText DisplayName,FName ConfigCategory,bool DefaultValue,bool InitialValue);

	// Creates and adds a new Float setting at runtime.
	// Returns the created setting, or existing setting if Tag already exists.
	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	UEFModularSettingsFloat* AddFloatSetting(FGameplayTag Tag,FText DisplayName,FName ConfigCategory,float DefaultValue,float InitialValue,float Min,float Max);

	// Creates and adds a new MultiSelect setting at runtime.
	// Returns the created setting, or existing setting if Tag already exists.
	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	UEFModularSettingsMultiSelect* AddMultiSelectSetting(FGameplayTag Tag,FText DisplayName,FName ConfigCategory,const TArray<FString>& Values,const TArray<FText>& ValueDisplayNames,int32 DefaultIndex,int32 InitialIndex);


protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Modular Settings")
	TArray<TObjectPtr<UEFModularSettingsBase>> DefaultSettings;

	// Instanced settings (created via DuplicateObject). Values replicate via subobject replication.
	UPROPERTY(ReplicatedUsing = OnRep_Settings)
	TArray<TObjectPtr<UEFModularSettingsBase>> Settings;

	// Replicated "what exists" list for asset-based settings (from containers/data assets).
	UPROPERTY(ReplicatedUsing = OnRep_SettingDefinitions)
	TArray<FEFPlayerSettingDefinition> SettingDefinitions;

	// Replicated definitions for runtime-created settings (AddBoolSetting, etc.)
	// Contains all data needed to reconstruct settings on clients.
	UPROPERTY(ReplicatedUsing = OnRep_RuntimeSettingDefinitions)
	TArray<FEFRuntimeSettingDefinition> RuntimeSettingDefinitions;

	// Centralized local creation (used by BeginPlay defaults and by OnRep for runtime adds).
	UEFModularSettingsBase* AddSettingFromTemplate_Local(UEFModularSettingsBase* Template);
	
	// Create a setting from runtime definition data
	UEFModularSettingsBase* CreateSettingFromRuntimeDefinition(const FEFRuntimeSettingDefinition& Def);
	
public:
	// Client-to-Server request
	UFUNCTION(BlueprintCallable, Category = "Modular Settings")
	void RequestUpdateSetting(FGameplayTag Tag, const FString& NewValue);

protected:
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerUpdateSetting(FGameplayTag Tag, const FString& NewValue);

private:
	UFUNCTION()
	void OnRep_Settings();

	UFUNCTION()
	void OnRep_RuntimeSettingDefinitions();

	UFUNCTION()
	void OnRep_SettingDefinitions();
};
