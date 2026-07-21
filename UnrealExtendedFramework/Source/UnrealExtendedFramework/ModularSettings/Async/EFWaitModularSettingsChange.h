// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "EFWaitModularSettingsChange.generated.h"


class UEFModularSettingsBase;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnModularSettingsChangeTriggered, UEFModularSettingsBase*, Setting);

UCLASS()
class UNREALEXTENDEDFRAMEWORK_API UEFWaitModularSettingsChange : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	// Delegate to notify Blueprint when triggered
	UPROPERTY(BlueprintAssignable)
	FOnModularSettingsChangeTriggered OnTriggered;

	// Factory function for Blueprint
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"), Category = "Modular Settings|Async")
	static UEFWaitModularSettingsChange* WaitForModularSettingsChange(UObject* WorldContextObject, FGameplayTag SettingTag);

	virtual void Activate() override;

protected:

	UPROPERTY()
	FGameplayTag SettingTag;

	UPROPERTY()
	UObject* WorldContextObject;

	UFUNCTION()
	void OnSettingChanged(UEFModularSettingsBase* ChangedSetting);
	
};
